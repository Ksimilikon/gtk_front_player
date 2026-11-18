// background of the player panel: three translucent sine waves in the accent
// color. While playing they drift left / right and their parameters
// (amplitude, length, speed, level) wander smoothly inside small ranges
// picked at random. Paused — frozen, the frame clock is released (no CPU).
#pragma once

#include <gtkmm/drawingarea.h>

#include <array>
#include <random>

namespace dp::ui {

class WaveView : public Gtk::DrawingArea {
public:
    WaveView();
    void set_playing(bool playing);

private:
    struct Param {
        double lo, hi;   // the wander range
        double value, target;
        void retarget(std::mt19937& rng);
        void step(double dt, double rate, std::mt19937& rng);
    };
    struct Wave {
        Param amp;     // fraction of the height
        Param cycles;  // periods across the width
        Param speed;   // radians per second
        Param level;   // baseline, fraction of the height
        double phase = 0;
        int dir = 1;   // drift direction
        double alpha = 0.15;
    };

    void on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h);
    bool on_tick(const Glib::RefPtr<Gdk::FrameClock>& clock);

    std::mt19937 rng_{std::random_device{}()};
    std::array<Wave, 3> waves_;
    bool playing_ = false;
    guint tick_ = 0;
    gint64 last_us_ = 0;
};

}  // namespace dp::ui
