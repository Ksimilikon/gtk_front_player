// a row of a track list: number (or a playing mark), title, artists, album,
// duration; an optional slot for trailing buttons.
#pragma once

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/listboxrow.h>

#include "core/Core.hpp"

namespace dp::ui {

class TrackRow : public Gtk::ListBoxRow {
public:
    TrackRow(const core::Track& t, int number, bool show_album = true);
    int64_t track_id() const { return id_; }
    int number() const { return number_; }
    void set_number(int n) {
        number_ = n;
        num_.set_text(n > 0 ? std::to_string(n) : "");
    }
    void set_playing(bool playing);
    /// a box at the end of the row for buttons.
    Gtk::Box& trailing() { return trailing_; }
    /// the text the search matches against.
    static std::string search_text_for(const core::Track& t);
    const std::string& search_text() const { return search_; }

private:
    int64_t id_;
    int number_;
    Gtk::Box box_{Gtk::Orientation::HORIZONTAL, 12};
    Gtk::Label num_;
    Gtk::Box trailing_{Gtk::Orientation::HORIZONTAL, 2};
    std::string search_;
};

}  // namespace dp::ui
