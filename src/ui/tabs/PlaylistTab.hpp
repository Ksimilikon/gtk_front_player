// a playlist view: its songs; activating one plays the playlist from it. An
// empty name is the whole library ("all songs").
#pragma once

#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/searchentry.h>

#include "ui/Tab.hpp"
#include "ui/widgets/Common.hpp"

namespace dp::ui {

class PlaylistTab : public Tab {
public:
    explicit PlaylistTab(std::string name);
    static std::string key_for(const std::string& name) { return name.empty() ? "library" : "playlist:" + name; }
    std::string key() const override { return key_for(name_); }
    std::string title() const override;

private:
    void rebuild();
    void mark_playing();
    std::vector<int64_t> ids() const;
    std::string queue_name() const;
    std::vector<MenuItem> track_menu(int64_t id, size_t index);

    std::string name_;
    CoverView cover_{120};
    Gtk::Label title_, stats_;
    Gtk::SearchEntry search_;
    Gtk::ScrolledWindow scroll_;
    Gtk::ListBox list_;
};

}  // namespace dp::ui
