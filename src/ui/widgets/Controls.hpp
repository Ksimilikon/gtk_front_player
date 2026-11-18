// the compact playback control cluster (bottom right): shuffle, previous,
// play / pause, next, repeat one, volume.
#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/scale.h>
#include <gtkmm/togglebutton.h>

namespace dp::ui {

class Controls : public Gtk::Box {
public:
    Controls();

private:
    void sync();
    Gtk::ToggleButton shuffle_, repeat_;
    Gtk::Button prev_, play_, next_;
    Gtk::Button volume_icon_;
    Gtk::Scale volume_{Gtk::Orientation::HORIZONTAL};
    bool updating_ = false;
    double muted_from_ = 1.0;
};

}  // namespace dp::ui
