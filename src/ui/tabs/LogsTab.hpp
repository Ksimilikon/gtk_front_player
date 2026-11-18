// the app log: events of the core, the player, sync, mods.
#pragma once

#include <gtkmm/dropdown.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>

#include "ui/Tab.hpp"
#include "util/Log.hpp"

namespace dp::ui {

class LogsTab : public Tab {
public:
    LogsTab();
    std::string key() const override { return "logs"; }
    std::string title() const override;

private:
    void add(const LogEntry& e);
    void reload();
    Gtk::DropDown* filter_ = nullptr;
    Gtk::ListBox list_;
    Gtk::ScrolledWindow scroll_;
};

}  // namespace dp::ui
