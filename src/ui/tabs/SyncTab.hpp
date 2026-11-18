// sync between devices: two-way sync, receive or send, over TCP (local
// network) or USB through ADB; the paired devices.
#pragma once

#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/spinner.h>

#include <memory>

#include "core/SyncWorker.hpp"
#include "ui/Tab.hpp"

namespace dp::ui {

class SyncTab : public Tab {
public:
    SyncTab();
    std::string key() const override { return "sync"; }
    std::string title() const override;
    void on_activated() override { fill_paired(); }

private:
    void start();
    void on_message(const SyncMessage& m);
    void add_line(Level lv, const std::string& text);
    void update_controls();
    void fill_paired();

    Gtk::DropDown* mode_ = nullptr;
    Gtk::DropDown* transport_ = nullptr;
    Gtk::SpinButton port_;
    Gtk::Entry address_;
    Gtk::Box* address_row_ = nullptr;
    Gtk::CheckButton force_;
    Gtk::Button* start_ = nullptr;
    Gtk::Button* cancel_ = nullptr;
    Gtk::Spinner spinner_;
    Gtk::Box log_{Gtk::Orientation::VERTICAL, 2};
    Gtk::ScrolledWindow log_scroll_;
    Gtk::Box paired_{Gtk::Orientation::VERTICAL, 4};
    std::unique_ptr<SyncWorker> worker_;
};

}  // namespace dp::ui
