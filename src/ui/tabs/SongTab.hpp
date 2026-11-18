// the song overview: the cover, the tags (editable, written into the file),
// the personal marks (color, label, volume) and technical info.
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
    void on_activated() override { reload(); }

private:
    void reload();
    void save_tags();
    void set_color(const std::string& c);

    int64_t id_;
    core::Track track_;
    CoverView cover_{240};
    Gtk::Label heading_, artists_heading_;
    Gtk::Entry title_, artists_, album_, genres_, label_;
    Gtk::Scale volume_{Gtk::Orientation::HORIZONTAL};
    Gtk::CheckButton keep_;
    Gtk::Box colors_{Gtk::Orientation::HORIZONTAL, 6};
    Gtk::Box info_{Gtk::Orientation::VERTICAL, 4};
    Gtk::Box mod_actions_{Gtk::Orientation::HORIZONTAL, 6};
    bool loading_ = false;
};

}  // namespace dp::ui
