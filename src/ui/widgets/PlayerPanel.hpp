// the top player panel: cover, title, artists over the wave animation, the
// progress line as its bottom border. A click opens the song overview.
#pragma once

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/overlay.h>

#include "ui/widgets/Common.hpp"
#include "ui/widgets/SeekBar.hpp"
#include "ui/widgets/WaveView.hpp"

namespace dp::ui {

class PlayerPanel : public Gtk::Box {
public:
    PlayerPanel();
    sigc::signal<void()>& signal_open() { return open_; }

private:
    void on_track();
    void on_state();

    Gtk::Overlay overlay_;
    WaveView waves_;
    Gtk::Box content_{Gtk::Orientation::HORIZONTAL, 14};
    CoverView cover_{64};
    Gtk::Box text_{Gtk::Orientation::VERTICAL, 2};
    Gtk::Label title_, artists_, time_;
    SeekBar seek_;
    bool scrubbing_ = false;
    sigc::signal<void()> open_;
};

}  // namespace dp::ui
