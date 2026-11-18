#include "core/Player.hpp"

#include <glibmm/main.h>

#include <algorithm>
#include <filesystem>
#include <numeric>
#include <random>

#include "core/Library.hpp"
#include "i18n/I18n.hpp"
#include "util/Log.hpp"
#include "util/MainThread.hpp"
#include "util/Settings.hpp"

namespace dp {

using core::Domain;

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
    if (order_.empty() || index_ >= order_.size()) return std::nullopt;
    return queue_[order_[index_]];
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

static std::vector<size_t> make_order(size_t n, bool shuffle, size_t first) {
    std::vector<size_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    if (shuffle && n > 1) {
        static std::mt19937 rng{std::random_device{}()};
        std::shuffle(order.begin(), order.end(), rng);
        // the chosen track plays first.
        std::iter_swap(order.begin(), std::find(order.begin(), order.end(), first));
    }
    return order;
}

void Player::play_queue(const std::string& name, std::vector<QueueItem> items, size_t start) {
    if (items.empty()) {
        Log::get().warn("player", tr("player.empty_queue"));
        return;
    }
    queue_ = std::move(items);
    queue_name_ = name;
    start = std::min(start, queue_.size() - 1);
    order_ = make_order(queue_.size(), shuffle_, start);
    index_ = shuffle_ ? 0 : start;
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

void Player::play_next(const QueueItem& item) {
    if (order_.empty()) {
        play_queue(tr("player.queue"), {item}, 0);
        return;
    }
    queue_.push_back(item);
    order_.insert(order_.begin() + static_cast<long>(index_ + 1), queue_.size() - 1);
    Log::get().info("player", tr("player.next_added"));
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
    if (order_.empty()) return;
    const long next = static_cast<long>(index_) + dir;
    if (next < 0) {
        index_ = 0;
    } else if (next >= static_cast<long>(order_.size())) {
        // end of the queue: stop.
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

void Player::set_repeat_one(bool on) {
    repeat_one_ = on;
    Settings::get().set_bool(key::repeat_one, on);
    modes_changed_.emit();
}

void Player::set_shuffle(bool on) {
    shuffle_ = on;
    Settings::get().set_bool(key::shuffle, on);
    if (!order_.empty()) {
        const size_t cur = order_[index_];
        order_ = make_order(queue_.size(), on, cur);
        index_ = on ? 0 : cur;
    }
    modes_changed_.emit();
}

void Player::set_volume(double v) {
    v = std::clamp(v, 0.0, 1.0);
    if (std::abs(v - volume_) < 1e-4) return;
    volume_ = v;
    if (uint32_t st = engine_set_volume_master(static_cast<float>(v)); st != 0 && st != 11)
        Log::get().warn("player", core::status_text(Domain::Engine, st));
    volume_changed_.emit();
}

void Player::set_track_volume(float v) {
    engine_set_volume_track(v);
    if (const auto c = current(); c && c->id) idx_set_track_volume(c->id, v);
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
