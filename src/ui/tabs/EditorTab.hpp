// the playlist editor: create / edit a playlist — name, cover, songs (order,
// add from the library, remove). An empty name creates a new playlist.
#pragma once

#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/searchentry.h>

#include "ui/Tab.hpp"
#include "ui/widgets/Common.hpp"

namespace dp::ui {

class EditorTab : public Tab {
public:
    explicit EditorTab(std::string name);
    static std::string key_for(const std::string& name) { return "editor:" + name; }
    std::string key() const override { return key_for(original_); }
    std::string title() const override;

private:
    void rebuild_tracks();
    void rebuild_library();
    void add(int64_t id);
    void save();
    void set_cover(const std::string& path);

    std::string original_;  // "" — a new playlist
    std::vector<int64_t> tracks_;
    std::string cover_path_;

    Gtk::Entry name_;
    CoverView cover_{72};
    Gtk::Label count_;
    Gtk::ListBox tracks_list_, library_list_;
    Gtk::ScrolledWindow tracks_scroll_, library_scroll_;
    Gtk::SearchEntry search_;
    Gtk::Label error_;
};

}  // namespace dp::ui
