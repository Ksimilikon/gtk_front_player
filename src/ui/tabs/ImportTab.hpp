// import of a `.dplr` packet into the library.
#pragma once

#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/label.h>
#include <gtkmm/spinner.h>

#include "ui/Tab.hpp"

namespace dp::ui {

class ImportTab : public Tab {
public:
    ImportTab();
    std::string key() const override { return "import"; }
    std::string title() const override;

private:
    void run();
    std::string file_;
    Gtk::Label file_label_, target_label_, result_;
    Gtk::CheckButton force_;
    Gtk::Button import_;
    Gtk::Spinner spinner_;
};

}  // namespace dp::ui
