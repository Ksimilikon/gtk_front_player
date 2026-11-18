// the empty part of the bottom bar is the master volume control: click or
// drag anywhere in it (left — quiet, right — loud). A faint fill shows the
// level; the latest status message is written over it, and while the pointer
// is over the zone it shows the volume in percent. No tooltip, no wheel.
#pragma once

#include <gtkmm/drawingarea.h>

#include "util/Log.hpp"

namespace dp::ui {

class VolumeZone : public Gtk::DrawingArea {
public:
    VolumeZone();
    void show_status(Level level, const std::string& text);

private:
    void on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h);
    void set_from_x(double x);

    bool hover_ = false, dragging_ = false;
    double drag_x_ = 0;
    Level level_ = Level::Info;
    std::string status_;
    sigc::connection timer_;
};

}  // namespace dp::ui
