// the library as the UI sees it: a cached copy of the db tracks and
// playlists, re-read after every change. Main thread only.
#pragma once

#include <sigc++/signal.h>

#include <unordered_map>

#include "core/Core.hpp"
#include "util/MainThread.hpp"

namespace dp {

class Library {
public:
    static Library& get();

    /// re-reads tracks and playlists from the db; emits `changed`.
    void reload();

    const std::vector<core::Track>& tracks() const { return tracks_; }
    const std::vector<core::Playlist>& playlists() const { return playlists_; }
    const core::Track* track(int64_t id) const;
    const core::Playlist* playlist(const std::string& name) const;
    /// the tracks of a playlist in its order (unknown ids skipped).
    std::vector<const core::Track*> tracks_of(const core::Playlist& p) const;
    uint64_t total_duration(const std::vector<int64_t>& ids) const;

    // writes: each logs failures, reloads on success and returns the status.
    uint32_t save_playlist(const std::string& name, const std::vector<int64_t>& ids, const std::string& cover);
    uint32_t remove_playlist(const std::string& name);
    /// renames by saving under the new name and deleting the old one.
    uint32_t rename_playlist(const std::string& from, const std::string& to);

    /// indexes a directory on a background thread (empty → the music dir).
    void index_dir_async(const std::string& dir);
    /// adds files on a background thread.
    void add_files_async(const std::vector<std::string>& paths);
    bool indexing() const { return indexing_; }

    sigc::signal<void()>& signal_changed() { return changed_; }
    sigc::signal<void(bool)>& signal_indexing() { return indexing_changed_; }

private:
    std::vector<core::Track> tracks_;
    std::unordered_map<int64_t, size_t> by_id_;
    std::vector<core::Playlist> playlists_;
    bool indexing_ = false;
    Alive alive_ = make_alive();
    sigc::signal<void()> changed_;
    sigc::signal<void(bool)> indexing_changed_;
};

}  // namespace dp
