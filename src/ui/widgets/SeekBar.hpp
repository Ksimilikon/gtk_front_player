// a minimal playback progress line (the bottom border of the player panel):
// thin in the accent color; under the pointer it grows thicker and shows a
// knob, so it is easy to grab. Click / drag seeks.
#pragma once

#include <gtkmm/drawingarea.h>
#include <sigc++/signal.h>

namespace dp::ui {

class SeekBar : public Gtk::DrawingArea {
public:
    SeekBar();
    /// 0..1; ignored while the user drags.
    void set_fraction(double f);
    /// the user chose a position (0..1).
    sigc::signal<void(double)>& signal_seek() { return seek_; }
    /// while dragging: the position under the pointer (for a time hint).
    sigc::signal<void(double)>& signal_scrub() { return scrub_; }

private:
    void on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h);
    void animate_to(double thickness);
    double fraction_at(double x) const;

    static constexpr double kThin = 1.5, kThick = 4;
    double fraction_ = 0;
    double drag_fraction_ = 0;
    bool dragging_ = false;
    double drag_start_x_ = 0;
    double thickness_ = kThin, target_ = kThin;
    guint tick_ = 0;
    sigc::signal<void(double)> seek_, scrub_;
};

}  // namespace dp::ui
