#include "core/Player.hpp"

#include <glibmm/main.h>

#include <algorithm>
#include <filesystem>
#include <random>

#include "core/Library.hpp"
#include "i18n/I18n.hpp"
#include "util/Log.hpp"
#include "util/MainThread.hpp"
#include "util/Settings.hpp"

namespace dp {

using core::Domain;

static std::mt19937& rng() {
    static std::mt19937 r{std::random_device{}()};
    return r;
}

Player& Player::get() {
    static Player p;
    return p;
}

// the core calls it on its audio thread when a track has played to the end.
void Player::end_hook() {
    post([] { Player::get().on_track_end(); });
}

void Player::start() {
    float v = 1.0f;
    if (engine_get_volume_master(&v) == 0) volume_ = v;
    repeat_one_ = Settings::get().boolean(key::repeat_one, false);
    shuffle_ = Settings::get().boolean(key::shuffle, false);
    Glib::signal_timeout().connect(sigc::mem_fun(*this, &Player::poll), 250);
}

std::optional<QueueItem> Player::current() const {
    if (index_ >= queue_.size()) return std::nullopt;
    return queue_[index_].item;
}

const core::Track* Player::current_track() const {
    const auto c = current();
    if (!c || !state_.has_track) return nullptr;
    return c->id ? Library::get().track(c->id) : nullptr;
}

std::string Player::current_title() const {
    if (const auto* t = current_track()) return t->display_title();
    if (state_.has_track) return external_title_;
    return {};
}

std::string Player::current_artists() const {
    if (const auto* t = current_track()) return t->artists_text();
    return {};
}

// shuffles (or restores the original order of) the entries after the current.
void Player::shuffle_rest(bool on) {
    if (index_ + 1 >= queue_.size()) return;
    auto first = queue_.begin() + static_cast<long>(index_ + 1);
    if (on)
        std::shuffle(first, queue_.end(), rng());
    else
        std::stable_sort(first, queue_.end(), [](const auto& a, const auto& b) { return a.orig < b.orig; });
}

void Player::play_queue(const std::string& name, std::vector<QueueItem> items, size_t start, bool focus) {
    if (items.empty()) {
        Log::get().warn("player", tr("player.empty_queue"));
        return;
    }
    start = std::min(start, items.size() - 1);
    queue_.clear();
    for (size_t i = 0; i < items.size(); ++i)
        queue_.push_back({std::move(items[i]), next_key_++, static_cast<int64_t>(i) * 1024});
    queue_name_ = name;
    if (shuffle_) {
        // the chosen song first, the rest shuffled.
        std::rotate(queue_.begin(), queue_.begin() + static_cast<long>(start), queue_.begin() + static_cast<long>(start) + 1);
        index_ = 0;
        shuffle_rest(true);
    } else {
        index_ = start;
    }
    queue_changed_.emit();
    queue_started_.emit(focus);
    play_current();
}

void Player::play_ids(const std::string& name, const std::vector<int64_t>& ids, size_t start) {
    std::vector<QueueItem> items;
    items.reserve(ids.size());
    for (int64_t id : ids) items.push_back({id, {}});
    play_queue(name, std::move(items), start);
}

void Player::play_path(const std::string& path) {
    // a file already in the library plays as that track (metadata, volume).
    for (const auto& t : Library::get().tracks())
        if (t.path == path) return play_queue(t.display_title(), {QueueItem{t.id, {}}}, 0);
    play_queue(std::filesystem::path(path).stem().string(), {QueueItem{0, path}}, 0);
}

void Player::preview(int64_t id) {
    const auto* t = Library::get().track(id);
    play_queue(tr("player.preview", {t ? t->display_title() : std::to_string(id)}), {QueueItem{id, {}}}, 0, false);
}

void Player::enqueue(const std::vector<int64_t>& ids, Where where, const std::string& name) {
    if (ids.empty()) return;
    if (queue_.empty()) return play_ids(name, ids, 0);
    std::vector<QueueEntry> add;
    const int64_t base = where == Where::AfterCurrent ? queue_[index_].orig : queue_.back().orig + 1024;
    for (size_t i = 0; i < ids.size(); ++i)
        add.push_back({{ids[i], {}}, next_key_++, base + static_cast<int64_t>(i + 1)});
    const auto at = where == Where::AfterCurrent ? queue_.begin() + static_cast<long>(index_ + 1) : queue_.end();
    queue_.insert(at, add.begin(), add.end());
    if (where == Where::AfterCurrent) {
        // keep the original order consistent after the inserted block.
        for (size_t i = index_ + 1 + add.size(); i < queue_.size(); ++i)
            if (queue_[i].orig <= add.back().orig) queue_[i].orig += static_cast<int64_t>(add.size()) + 1;
    }
    Log::get().info("player", tr(where == Where::AfterCurrent ? "player.added_next" : "player.added_end",
                                 {name, std::to_string(ids.size())}));
    queue_changed_.emit();
}

void Player::play_next(const QueueItem& item) {
    if (queue_.empty()) return play_queue(tr("player.queue"), {item}, 0);
    queue_.insert(queue_.begin() + static_cast<long>(index_ + 1), {item, next_key_++, queue_[index_].orig + 1});
    Log::get().info("player", tr("player.next_added"));
    queue_changed_.emit();
}

void Player::jump(size_t pos) {
    if (pos >= queue_.size()) return;
    index_ = pos;
    play_current();
}

void Player::move(size_t from, size_t to) {
    if (from >= queue_.size() || to >= queue_.size() || from == to) return;
    const uint64_t cur_key = index_ < queue_.size() ? queue_[index_].key : 0;
    QueueEntry e = queue_[from];
    queue_.erase(queue_.begin() + static_cast<long>(from));
    queue_.insert(queue_.begin() + static_cast<long>(to), e);
    // the moved order becomes the original one too.
    for (size_t i = 0; i < queue_.size(); ++i) {
        queue_[i].orig = static_cast<int64_t>(i) * 1024;
        if (queue_[i].key == cur_key) index_ = i;
    }
    queue_changed_.emit();
}

void Player::remove_at(size_t pos) {
    if (pos >= queue_.size()) return;
    const bool was_current = pos == index_;
    queue_.erase(queue_.begin() + static_cast<long>(pos));
    if (queue_.empty()) return clear();
    if (pos < index_) --index_;
    if (index_ >= queue_.size()) index_ = queue_.size() - 1;
    queue_changed_.emit();
    if (was_current && state_.has_track) play_current();
}

void Player::clear() {
    engine_stop();
    queue_.clear();
    queue_name_.clear();
    index_ = 0;
    state_ = {};
    queue_changed_.emit();
    track_changed_.emit();
    state_changed_.emit();
}

void Player::play_current() {
    const auto c = current();
    if (!c) return;
    uint32_t st;
    if (c->id) {
        st = engine_add_track_id(c->id, &Player::end_hook);
    } else {
        st = engine_add_track_path(c->path.c_str(), &Player::end_hook);
        external_title_ = std::filesystem::path(c->path).stem().string();
    }
    if (st != 0) {
        Log::get().error("player", tr("player.play_failed", {core::status_text(Domain::Engine, st)}));
        state_ = {};
        track_changed_.emit();
        state_changed_.emit();
        return;
    }
    state_.has_track = true;
    state_.playing = true;
    state_.position = 0;
    uint64_t d = 0;
    engine_get_duration(&d);
    state_.duration = d;
    track_changed_.emit();
    state_changed_.emit();
}

void Player::step(int dir) {
    if (queue_.empty()) return;
    const long next = static_cast<long>(index_) + dir;
    if (next < 0) {
        index_ = 0;
    } else if (next >= static_cast<long>(queue_.size())) {
        // end of the queue: stop (the queue stays, Play starts it again).
        engine_stop();
        index_ = 0;
        state_ = {};
        track_changed_.emit();
        state_changed_.emit();
        Log::get().info("player", tr("player.queue_finished"));
        return;
    } else {
        index_ = static_cast<size_t>(next);
    }
    play_current();
}

void Player::on_track_end() {
    if (repeat_one_)
        play_current();
    else
        step(+1);
}

void Player::toggle() {
    if (!state_.has_track) {
        if (current()) play_current();
        return;
    }
    if (uint32_t st = engine_play_pause(); st != 0)
        Log::get().error("player", core::status_text(Domain::Engine, st));
    state_.playing = !state_.playing;
    state_changed_.emit();
}

void Player::play() {
    if (!state_.has_track) return toggle();
    engine_play();
    state_.playing = true;
    state_changed_.emit();
}

void Player::pause() {
    if (!state_.has_track) return;
    engine_pause();
    state_.playing = false;
    state_changed_.emit();
}

void Player::stop() {
    engine_stop();
    state_ = {};
    track_changed_.emit();
    state_changed_.emit();
}

void Player::next() { step(+1); }

void Player::previous() {
    // the first seconds of a track: the previous one; later: from the start.
    if (state_.position > 3 || index_ == 0) {
        if (state_.has_track) seek(0);
        return;
    }
    step(-1);
}

void Player::seek(uint64_t seconds) {
    if (!state_.has_track) return;
    if (uint32_t st = engine_set_seek(seconds); st != 0) {
        Log::get().warn("player", core::status_text(Domain::Engine, st));
        return;
    }
    state_.position = seconds;
    state_changed_.emit();
    seeked_.emit(seconds);
}

void Player::seek_by(int seconds) {
    if (!state_.has_track) return;
    const long target = static_cast<long>(state_.position) + seconds;
    const long max = state_.duration ? static_cast<long>(state_.duration) - 1 : target;
    seek(static_cast<uint64_t>(std::clamp(target, 0L, std::max(0L, max))));
}

void Player::set_repeat_one(bool on) {
    repeat_one_ = on;
    Settings::get().set_bool(key::repeat_one, on);
    modes_changed_.emit();
}

void Player::set_shuffle(bool on) {
    if (on == shuffle_) return;
    shuffle_ = on;
    Settings::get().set_bool(key::shuffle, on);
    shuffle_rest(on);
    modes_changed_.emit();
    queue_changed_.emit();
}

void Player::set_volume(double v) {
    v = std::clamp(v, 0.0, 1.0);
    if (std::abs(v - volume_) < 1e-4) return;
    volume_ = v;
    if (uint32_t st = engine_set_volume_master(static_cast<float>(v)); st != 0 && st != 11)
        Log::get().warn("player", core::status_text(Domain::Engine, st));
    volume_changed_.emit();
}

float Player::track_volume() const {
    float v = 1.0f;
    engine_get_volume_track(&v);
    return v;
}

void Player::set_track_volume(float v) {
    v = std::clamp(v, 0.0f, 2.0f);
    engine_set_volume_track(v);
    if (const auto c = current(); c && c->id) idx_set_track_volume(c->id, v);
    volume_changed_.emit();
}

void Player::change_track_volume(float delta) {
    if (!state_.has_track) return;
    set_track_volume(track_volume() + delta);
    Log::get().info("player", tr("player.track_volume", {std::to_string(static_cast<int>(track_volume() * 100 + 0.5f))}));
}

bool Player::poll() {
    if (!state_.has_track || core::LongOp::active()) return true;
    uint64_t pos = 0, dur = 0;
    bool paused = false;
    engine_get_seek(&pos);
    engine_get_duration(&dur);
    engine_is_pause(&paused);
    const bool changed = pos != state_.position || dur != state_.duration || paused == state_.playing;
    state_.position = pos;
    state_.duration = dur;
    state_.playing = !paused;
    if (changed) state_changed_.emit();
    return true;
}

}  // namespace dp
