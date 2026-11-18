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

}  // namespace dp::ui
