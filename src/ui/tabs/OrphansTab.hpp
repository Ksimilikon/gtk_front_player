#pragma once

#include <gtkmm/listbox.h>
#include <gtkmm/searchentry.h>
#include <gtkmm/togglebutton.h>

#include "ui/Tab.hpp"
#include "ui/widgets/Common.hpp"
#include "ui/widgets/TrackSort.hpp"

namespace dp::ui {

class OrphansTab : public Tab {
public:
    OrphansTab();
    std::string key() const override { return "orphans"; }
    std::string title() const override;

private:
    void reload();
    void rebuild();
    void play(int64_t id);
    void ask_delete(const std::vector<int64_t>& ids);

    Gtk::DropDown filter_;
    TrackSort sort_;
    Gtk::ToggleButton descending_;
    Gtk::SearchEntry search_;
    Gtk::Button play_, remove_;
    Gtk::Label stats_, note_;
    Gtk::ListBox list_;
    std::vector<int64_t> candidates_, shown_;
};

}  // namespace dp::ui
