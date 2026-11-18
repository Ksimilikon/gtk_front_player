// cover textures: a track cover (the db file or the one embedded in the tags,
// via `idx_get_track_cover`) or an image file (playlist covers).
//
// - full-size covers (player panel, song page) — synchronous, small cache;
// - thumbnails for lists — decoded and scaled on a background thread, many
//   cached; the callback runs on the main thread if the requester is alive.
#pragma once

#include <gdkmm/texture.h>

#include <condition_variable>
#include <deque>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "util/MainThread.hpp"

namespace dp {

class Covers {
public:
    using Callback = std::function<void(const Glib::RefPtr<Gdk::Texture>&)>;

    static Covers& get();

    /// null if the track has no cover.
    Glib::RefPtr<Gdk::Texture> track(int64_t id);
    /// a cover of a song by file path (not necessarily in the library).
    Glib::RefPtr<Gdk::Texture> track_path(const std::string& path);
    /// an image file; null if unreadable.
    Glib::RefPtr<Gdk::Texture> file(const std::string& path);
    /// raw cover bytes of a track (for MPRIS art), empty if none.
    std::string track_bytes(int64_t id);

    /// a thumbnail (`px` — the displayed size) of a track / an image file.
    /// Cached ones are delivered right away, others later.
    void thumb_track(int64_t id, int px, AliveRef alive, Callback cb);
    void thumb_file(const std::string& path, int px, AliveRef alive, Callback cb);

    void invalidate_track(int64_t id);
    void clear();

private:
    struct Job {
        std::string key;
        int64_t id = 0;
        std::string path;  // image file (id == 0)
        int px = 0;
        AliveRef alive;
        Callback cb;
    };
    void request(Job job);
    void worker();
    Glib::RefPtr<Gdk::Texture> lookup(const std::string& key);
    void store(const std::string& key, const Glib::RefPtr<Gdk::Texture>& tex, size_t max);

    static constexpr size_t kMax = 64, kMaxThumbs = 1200;
    std::unordered_map<std::string, Glib::RefPtr<Gdk::Texture>> map_;
    std::list<std::string> order_;  // most recent at the back
    std::unordered_map<std::string, std::vector<Job>> waiting_;  // key → requesters

    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;  // newest first
    bool started_ = false;
};

}  // namespace dp
