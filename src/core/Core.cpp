#include "core/Core.hpp"

#include "i18n/I18n.hpp"
#include "util/Format.hpp"

namespace dp::core {

static Dirs g_dirs;

std::string Track::artists_text() const {
    return artists.empty() ? tr("track.unknown_artist") : fmt::join(artists, ", ");
}

std::string Track::display_title() const {
    if (!title.empty()) return title;
    if (!path.empty()) return std::filesystem::path(path).stem().string();
    return tr("track.untitled");
}

std::string status_text(Domain d, uint32_t code) {
    static const char* prefixes[] = {"init", "idx", "engine", "cdc", "con", "sync", "mod"};
    const std::string key = std::string("status.") + prefixes[static_cast<int>(d)] + "." + std::to_string(code);
    const std::string& text = tr(key);
    if (text != key) return text;
    return tr("status.unknown", {prefixes[static_cast<int>(d)], std::to_string(code)});
}

uint32_t init(const Dirs& d) {
    g_dirs = d;
    return ::init(d.cache.c_str(), d.config.c_str(), d.data.c_str(), d.music.c_str(), true);
}

const Dirs& dirs() { return g_dirs; }

std::string take_string(char* s) {
    if (!s) return {};
    std::string out = s;
    idx_free_string(s);
    return out;
}

std::string str(const char* s) { return s ? std::string(s) : std::string(); }

std::vector<std::string> take_strings(FFIStringList list, void (*free_fn)(FFIStringList)) {
    std::vector<std::string> out;
    for (size_t i = 0; i < list.len; ++i) out.push_back(str(list.ptr[i]));
    free_fn(list);
    return out;
}

static std::vector<std::string> strings(char** p, size_t n) {
    std::vector<std::string> out;
    for (size_t i = 0; i < n; ++i)
        if (p[i]) out.emplace_back(p[i]);
    return out;
}

Track to_track(const FFITrackMetadata& m) {
    Track t;
    t.id = m.has_id ? m.id : 0;
    t.path = str(m.path);
    t.title = str(m.title);
    t.artists = strings(m.artists, m.artists_len);
    t.album = str(m.album);
    t.genres = strings(m.genres, m.genres_len);
    if (m.has_params) {
        t.duration = m.params.duration_s;
        t.sample_rate = m.params.sample_rate;
        t.bitrate = m.params.bitrate;
        t.listen_count = m.params.listen_count;
        t.cover_art = str(m.params.cover_art);
    }
    t.volume = m.volume;
    t.invalid = m.invalid;
    t.color = str(m.color);
    t.label = str(m.user_label);
    t.ignore_orphan = m.ignore_orphan;
    return t;
}

CStrings::CStrings(const std::vector<std::string>& v) {
    ptrs_.reserve(v.size());
    for (const auto& s : v) ptrs_.push_back(s.c_str());
}

std::optional<Track> fetch_track(int64_t id) {
    FFITrackMetadata m{};
    if (idx_get_track(id, nullptr, &m) != 0) return std::nullopt;
    Track t = to_track(m);
    idx_free_track_metadata(m);
    return t;
}

std::vector<std::string> output_devices() {
    FFIStringList list{};
    if (engine_get_output_devices(&list) != 0) return {};
    return take_strings(list, engine_free_output_devices);
}

std::string output_device() {
    char* s = nullptr;
    if (engine_get_output_device(&s) != 0) return {};
    return take_string(s);
}

std::string last_panic() { return take_string(core_last_panic()); }

}  // namespace dp::core
