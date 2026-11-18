// the start tab: every playlist of the library (plus "all songs").
#pragma once

#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/searchentry.h>

#include "ui/Tab.hpp"

namespace dp::ui {

class PlaylistsTab : public Tab {
public:
    PlaylistsTab();
    std::string key() const override { return "playlists"; }
    std::string title() const override;
    bool closable() const override { return false; }

private:
    void rebuild();
    Gtk::SearchEntry search_;
    Gtk::ScrolledWindow scroll_;
    Gtk::ListBox list_;
    Gtk::Label empty_;
};

}  // namespace dp::ui
