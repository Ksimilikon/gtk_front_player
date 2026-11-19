// navigation between tabs, implemented by the main window; pages use it to
// open other pages without knowing the window.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dp::ui {

class Nav {
public:
    virtual ~Nav() = default;
    virtual void open_playlists() = 0;
    /// the playlist view; an empty name — all songs of the library.
    virtual void open_playlist(const std::string& name) = 0;
    virtual void play_playlist(const std::string& name, const std::vector<int64_t>& ids, size_t start) = 0;
    /// the playlist editor; an empty name — a new playlist.
    virtual void open_editor(const std::string& name) = 0;
    virtual void open_song(int64_t id) = 0;
    /// the song playing now.
    virtual void open_current_song() = 0;
    /// "settings", "sync", "import", "export", "logs", "mods", "about".
    virtual void open_page(const std::string& page) = 0;
    /// a tab of a running mod.
    virtual void open_mod_tab(const std::string& mod_path, uint32_t tab_id) = 0;
    /// export with a preset selection.
    virtual void open_export_playlists(const std::vector<std::string>& names) = 0;
    virtual void open_export_tracks(const std::vector<int64_t>& ids) = 0;
};

Nav& nav();
void set_nav(Nav* n);

}  // namespace dp::ui
