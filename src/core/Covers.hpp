// cover textures: a track cover (the db file or the one embedded in the tags,
// via `idx_get_track_cover`) or an image file (playlist covers). A small
// cache; main thread only.
#pragma once

#include <gdkmm/texture.h>

#include <list>
#include <string>
#include <unordered_map>

namespace dp {

class Covers {
public:
    static Covers& get();

    /// null if the track has no cover.
    Glib::RefPtr<Gdk::Texture> track(int64_t id);
    /// a cover of a song by file path (not necessarily in the library).
    Glib::RefPtr<Gdk::Texture> track_path(const std::string& path);
    /// an image file; null if unreadable.
    Glib::RefPtr<Gdk::Texture> file(const std::string& path);
    /// raw cover bytes of a track (for MPRIS art), empty if none.
    std::string track_bytes(int64_t id);

    void invalidate_track(int64_t id);
    void clear();

private:
    Glib::RefPtr<Gdk::Texture> lookup(const std::string& key);
    void store(const std::string& key, const Glib::RefPtr<Gdk::Texture>& tex);
    static constexpr size_t kMax = 96;
    std::unordered_map<std::string, Glib::RefPtr<Gdk::Texture>> map_;
    std::list<std::string> order_;  // most recent at the back
};

}  // namespace dp
