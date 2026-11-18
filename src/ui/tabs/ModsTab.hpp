// mod management (the core mod system is pre-alpha): the list of mods,
// enabling / disabling, adding a library file, their tabs.
#pragma once

#include "ui/Tab.hpp"

namespace dp::ui {

class ModsTab : public Tab {
public:
    ModsTab();
    std::string key() const override { return "mods"; }
    std::string title() const override;
    void on_activated() override { rebuild(); }

private:
    void rebuild();
    Gtk::Box list_{Gtk::Orientation::VERTICAL, 8};
    bool updating_ = false;
};

}  // namespace dp::ui
