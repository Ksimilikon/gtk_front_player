// the playlist editor: create / edit a playlist — name, cover, songs. The
// screen is split in half: the playlist (reorder by drag-and-drop) and the
// library pool (drag songs into the playlist, sort, search, hide the added
// ones). An empty name creates a new playlist.
#pragma once

#include <gtkmm/checkbutton.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/searchentry.h>
#include <gtkmm/togglebutton.h>

#include <unordered_set>

#include "ui/widgets/TrackSort.hpp"
#include "ui/Tab.hpp"
#include "ui/widgets/Common.hpp"

namespace dp::ui {

class EditorTab : public Tab {
public:
    explicit EditorTab(std::string name);
    static std::string key_for(const std::string& name) { return "editor:" + name; }
    std::string key() const override { return key_; }
    std::string title() const override;

    bool has_unsaved() const override;
    bool save_changes() override;

private:
    void rebuild_tracks();
    void rebuild_pool();
    void insert(int64_t id, size_t at);
    void add(int64_t id) { insert(id, tracks_.size()); }
    void move(size_t from, size_t to);
    void set_cover(const std::string& path);
    void tracks_changed();

    std::string key_;
    std::string original_;  // "" — a new playlist
    std::vector<int64_t> original_tracks_, tracks_;
    std::unordered_set<int64_t> in_playlist_;
    std::string original_cover_, cover_path_;
    bool deleted_ = false;

    Gtk::Entry name_;
    CoverView cover_{72};
    Gtk::Label count_, error_, sort_note_;
    Gtk::ListBox tracks_list_, pool_list_;
    Gtk::ScrolledWindow tracks_scroll_, pool_scroll_;
    Gtk::SearchEntry search_;
    TrackSort sort_;
    Gtk::ToggleButton desc_;
    Gtk::CheckButton hide_added_;
};

}  // namespace dp::ui
