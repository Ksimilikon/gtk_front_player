// menus: the main menu (pages + actions of running mods) and the mod items of
// the playlist / song context menus.
#pragma once

#include <gtkmm/menubutton.h>

#include "ui/widgets/Common.hpp"

namespace dp::ui {

/// the main menu button (top right of the tab bar). Rebuilt on open, so mod
/// actions are always current.
class MainMenuButton : public Gtk::MenuButton {
public:
    MainMenuButton();

private:
    void rebuild();
    Gtk::Popover popover_;
};

/// appends the actions of running mods at `place` (`UI_PLACE_PLAYLIST` /
/// `UI_PLACE_SONG`) for `target` (playlist / song id).
void append_mod_items(std::vector<MenuItem>& items, uint32_t place, uint64_t target);

/// the common song items: play next, song overview, add to…, export, mods.
void append_song_items(std::vector<MenuItem>& items, Gtk::Widget& anchor, int64_t id);
/// "queue after the current song" / "queue at the end" for a set of songs.
void append_queue_items(std::vector<MenuItem>& items, std::function<std::vector<int64_t>()> ids,
                        const std::string& name);
/// chooses a playlist (or a new name) and appends `ids` to it.
void add_to_playlist(Gtk::Window& parent, const std::vector<int64_t>& ids);

}  // namespace dp::ui
