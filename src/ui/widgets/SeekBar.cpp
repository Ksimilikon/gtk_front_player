#include "ui/widgets/SeekBar.hpp"

#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gesturedrag.h>
#include <gdkmm/frameclock.h>

#include <algorithm>
#include <cmath>

#include <sigc++/adaptors/track_obj.h>

#include "theme/Theme.hpp"

namespace dp::ui {

SeekBar::SeekBar() {
    add_css_class("seekbar");
    set_content_height(10);  // the hit area; the line is drawn at the bottom
    set_hexpand(true);
    set_cursor("pointer");
    set_draw_func(sigc::mem_fun(*this, &SeekBar::on_draw));
    Theme::get().signal_changed().connect(sigc::track_obj([this] { queue_draw(); }, *this));

    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_enter().connect([this](double, double) { animate_to(kThick); });
    motion->signal_leave().connect([this] {
        if (!dragging_) animate_to(kThin);
    });
    add_controller(motion);

    auto drag = Gtk::GestureDrag::create();
    drag->signal_drag_begin().connect([this](double x, double) {
        dragging_ = true;
        drag_start_x_ = x;
        drag_fraction_ = fraction_at(x);
        scrub_.emit(drag_fraction_);
        queue_draw();
    });
    drag->signal_drag_update().connect([this](double dx, double) {
        drag_fraction_ = fraction_at(drag_start_x_ + dx);
        scrub_.emit(drag_fraction_);
        queue_draw();
    });
    drag->signal_drag_end().connect([this](double dx, double) {
        drag_fraction_ = fraction_at(drag_start_x_ + dx);
        dragging_ = false;
        fraction_ = drag_fraction_;
        seek_.emit(drag_fraction_);
        animate_to(kThick);
        queue_draw();
    });
    add_controller(drag);
}

double SeekBar::fraction_at(double x) const {
    const int w = get_width();
    return w > 0 ? std::clamp(x / w, 0.0, 1.0) : 0.0;
}

void SeekBar::set_fraction(double f) {
    f = std::clamp(f, 0.0, 1.0);
    if (dragging_ || std::abs(f - fraction_) < 1e-4) return;
    fraction_ = f;
    queue_draw();
}

void SeekBar::animate_to(double thickness) {
    target_ = thickness;
    if (tick_) return;
    tick_ = add_tick_callback([this](const Glib::RefPtr<Gdk::FrameClock>&) {
        thickness_ += (target_ - thickness_) * 0.3;
        if (std::abs(target_ - thickness_) < 0.1) {
            thickness_ = target_;
            tick_ = 0;
            queue_draw();
            return false;
        }
        queue_draw();
        return true;
    });
}

void SeekBar::on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h) {
    const double f = dragging_ ? drag_fraction_ : fraction_;
    const double t = thickness_;
    // grows upward from a fixed bottom edge, 3px above the widget's bottom so
    // the knob fits without clipping.
    const double y = h - 3 - t;
    const Gdk::RGBA track = Theme::get().color("border");
    const Gdk::RGBA accent = Theme::get().color("accent");
    cr->set_source_rgba(track.get_red(), track.get_green(), track.get_blue(), 0.9);
    cr->rectangle(0, y, w, t);
    cr->fill();
    cr->set_source_rgba(accent.get_red(), accent.get_green(), accent.get_blue(), 1.0);
    cr->rectangle(0, y, w * f, t);
    cr->fill();
    // the knob appears as the line grows.
    const double grow = (t - kThin) / (kThick - kThin);
    if (grow > 0.05) {
        const double r = (t / 2 + 1.5) * grow;
        cr->arc(std::clamp(w * f, r, w - r), y + t / 2, r, 0, 2 * M_PI);
        cr->fill();
    }
}

}  // namespace dp::ui
