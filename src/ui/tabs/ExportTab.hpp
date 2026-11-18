// export to a `.dplr` packet: the whole library, chosen playlists or songs.
#pragma once

#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/label.h>
#include <gtkmm/spinner.h>

#include "core/Exchange.hpp"
#include "ui/Tab.hpp"

namespace dp::ui {

class ExportTab : public Tab {
public:
    ExportTab();
    std::string key() const override { return "export"; }
    std::string title() const override;

    void preset_playlists(const std::vector<std::string>& names);
    void preset_tracks(const std::vector<int64_t>& ids);

private:
    void fill_playlists();
    void update();
    void run(const std::string& file);

    Gtk::DropDown* what_ = nullptr;
    Gtk::Box playlists_{Gtk::Orientation::VERTICAL, 2};
    Gtk::Label tracks_label_;
    std::vector<std::pair<std::string, Gtk::CheckButton*>> checks_;
    std::vector<int64_t> tracks_;
    Gtk::CheckButton audio_, names_, covers_;
    Gtk::Button export_;
    Gtk::Spinner spinner_;
    Gtk::Label result_;
};

}  // namespace dp::ui
