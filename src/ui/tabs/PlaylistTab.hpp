// a playlist: just its songs. Activating a song starts the playlist as the
// queue (the queue has its own tab, marked with `*`). Songs are reordered by
// drag-and-drop. An empty name is the whole library ("all songs").
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
    std::vector<int64_t> ids() const;
    std::vector<MenuItem> track_menu(Gtk::Widget& row, int64_t id, size_t index);
    void move_track(size_t from, size_t to);

    std::string name_;
    CoverView cover_{120};
    Gtk::Label title_, stats_;
    Gtk::SearchEntry search_;
    Gtk::ScrolledWindow scroll_;
    Gtk::ListBox list_;
};

}  // namespace dp::ui
