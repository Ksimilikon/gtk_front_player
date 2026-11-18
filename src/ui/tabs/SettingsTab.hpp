// settings: language, theme, music directory and indexing, audio output,
// paths, keyboard shortcuts.
#pragma once

#include <gtkmm/dropdown.h>
#include <gtkmm/label.h>

#include "ui/Tab.hpp"

namespace dp::ui {

class SettingsTab : public Tab {
public:
    SettingsTab();
    std::string key() const override { return "settings"; }
    std::string title() const override;

private:
    void fill_devices();
    Gtk::Label music_dir_;
    Gtk::DropDown* devices_ = nullptr;
    std::vector<std::string> device_names_;
    bool filling_ = false;
};

}  // namespace dp::ui
