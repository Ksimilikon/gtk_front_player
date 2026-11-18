#include "core/Library.hpp"

#include <algorithm>

#include "i18n/I18n.hpp"
#include "util/Log.hpp"

namespace dp {

using core::Domain;

Library& Library::get() {
    static Library l;
    return l;
}

void Library::reload() {
    FFITracksList tl{};
    if (uint32_t st = idx_get_tracks(&tl); st != 0) {
        Log::get().error("core", tr("log.tracks_failed", {core::status_text(Domain::Idx, st)}));
    } else {
        tracks_.clear();
        tracks_.reserve(tl.len);
        for (size_t i = 0; i < tl.len; ++i) tracks_.push_back(core::to_track(tl.ptr[i]));
        idx_free_tracks_list(tl);
    }
    std::sort(tracks_.begin(), tracks_.end(), [](const auto& a, const auto& b) {
        return a.display_title() < b.display_title();
    });
    by_id_.clear();
    for (size_t i = 0; i < tracks_.size(); ++i) by_id_[tracks_[i].id] = i;

    FFIPlaylistsList pl{};
    if (uint32_t st = idx_get_playlists(&pl); st != 0) {
        Log::get().error("core", tr("log.playlists_failed", {core::status_text(Domain::Idx, st)}));
    } else {
        playlists_.clear();
        for (size_t i = 0; i < pl.len; ++i) {
            const FFIPlaylist& p = pl.ptr[i];
            if (!p.name) continue;
            core::Playlist out;
            out.name = p.name;
            out.tracks.assign(p.tracks, p.tracks + p.tracks_len);
            out.cover = core::str(p.cover_art);
            out.created_at = p.has_created_at ? p.created_at : 0;
            out.updated_at = p.has_updated_at ? p.updated_at : 0;
            playlists_.push_back(std::move(out));
        }
        idx_free_playlists_list(pl);
    }
    std::sort(playlists_.begin(), playlists_.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    changed_.emit();
}

const core::Track* Library::track(int64_t id) const {
    const auto it = by_id_.find(id);
    return it == by_id_.end() ? nullptr : &tracks_[it->second];
}

const core::Playlist* Library::playlist(const std::string& name) const {
    for (const auto& p : playlists_)
        if (p.name == name) return &p;
    return nullptr;
}

std::vector<const core::Track*> Library::tracks_of(const core::Playlist& p) const {
    std::vector<const core::Track*> out;
    out.reserve(p.tracks.size());
    for (int64_t id : p.tracks)
        if (const auto* t = track(id)) out.push_back(t);
    return out;
}

uint64_t Library::total_duration(const std::vector<int64_t>& ids) const {
    uint64_t sum = 0;
    for (int64_t id : ids)
        if (const auto* t = track(id)) sum += t->duration;
    return sum;
}

uint32_t Library::save_playlist(const std::string& name, const std::vector<int64_t>& ids, const std::string& cover) {
    const uint32_t st =
        idx_save_playlist(name.c_str(), ids.empty() ? nullptr : ids.data(), ids.size(), cover.empty() ? nullptr : cover.c_str());
    if (st != 0)
        Log::get().error("core", tr("log.playlist_save_failed", {name, core::status_text(Domain::Idx, st)}));
    else
        reload();
    return st;
}

uint32_t Library::remove_playlist(const std::string& name) {
    const uint32_t st = idx_remove_playlist(name.c_str());
    if (st != 0)
        Log::get().error("core", tr("log.playlist_remove_failed", {name, core::status_text(Domain::Idx, st)}));
    else
        reload();
    return st;
}

uint32_t Library::rename_playlist(const std::string& from, const std::string& to) {
    const core::Playlist* p = playlist(from);
    if (!p) return 5;  // NotFound
    const core::Playlist copy = *p;
    uint32_t st = idx_save_playlist(to.c_str(), copy.tracks.empty() ? nullptr : copy.tracks.data(), copy.tracks.size(),
                                    copy.cover.empty() ? nullptr : copy.cover.c_str());
    if (st == 0) st = idx_remove_playlist(from.c_str());
    if (st != 0) Log::get().error("core", tr("log.playlist_save_failed", {to, core::status_text(Domain::Idx, st)}));
    reload();
    return st;
}

void Library::index_dir_async(const std::string& dir) {
    if (indexing_) return;
    indexing_ = true;
    indexing_changed_.emit(true);
    Log::get().info("core", tr("log.indexing", {dir.empty() ? core::dirs().music.string() : dir}));
    run_async(
        alive_, [dir] { core::LongOp op; return idx_index_dir(dir.empty() ? nullptr : dir.c_str()); },
        [this](uint32_t st) {
            indexing_ = false;
            indexing_changed_.emit(false);
            if (st != 0)
                Log::get().error("core", tr("log.index_failed", {core::status_text(Domain::Idx, st)}));
            else
                Log::get().ok("core", tr("log.indexed"));
            reload();
        });
}

void Library::add_files_async(const std::vector<std::string>& paths) {
    run_async(
        alive_,
        [paths] {
            core::LongOp op;
            core::CStrings c(paths);
            uint64_t added = 0;
            const uint32_t st = idx_add_tracks(c.data(), c.size(), &added);
            return std::pair{st, added};
        },
        [this](std::pair<uint32_t, uint64_t> r) {
            if (r.first != 0)
                Log::get().error("core", tr("log.add_failed", {core::status_text(Domain::Idx, r.first)}));
            else
                Log::get().ok("core", tr("log.added", {std::to_string(r.second)}));
            reload();
        });
}

}  // namespace dp
