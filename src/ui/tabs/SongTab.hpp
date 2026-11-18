// the song overview. By default it only shows the song: the cover in the
// center with the song's own volume as a vertical slider along it, the tags,
// the playlists the song is in and technical info. The "Edit" button turns
// on editing (tags written into the file, marks, the file path, the cover);
// Ctrl+S saves.
#pragma once

#include <gtkmm/checkbutton.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/scale.h>

#include "core/Core.hpp"
#include "ui/Tab.hpp"
#include "ui/widgets/Common.hpp"

namespace dp::ui {

class SongTab : public Tab {
public:
    explicit SongTab(int64_t id);
    static std::string key_for(int64_t id) { return "song:" + std::to_string(id); }
    std::string key() const override { return key_for(id_); }
    std::string title() const override;
    void on_activated() override;

    bool has_unsaved() const override;
    bool save_changes() override;
    void discard_changes() override { set_editing(false); }

private:
    void reload();
    void build_view();
    void build_edit();
    void set_editing(bool on);
    bool rename_file(const std::string& to);

    int64_t id_;
    core::Track track_;
    bool editing_ = false;
    std::string edit_color_;

    CoverView cover_{260};
    Gtk::Scale volume_{Gtk::Orientation::VERTICAL};
    Gtk::Label volume_value_;
    Gtk::Label heading_, subheading_;
    Gtk::Box actions_{Gtk::Orientation::HORIZONTAL, 6};
    Gtk::Box body_{Gtk::Orientation::VERTICAL, 6};
    // edit mode fields
    Gtk::Entry title_, artists_, album_, genres_, label_, path_;
    Gtk::CheckButton keep_;
    Gtk::Label error_;
    bool loading_ = false;
};

}  // namespace dp::ui
