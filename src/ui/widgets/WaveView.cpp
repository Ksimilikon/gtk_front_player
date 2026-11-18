#include "ui/widgets/WaveView.hpp"

#include <gdkmm/frameclock.h>

#include <cmath>

#include <sigc++/adaptors/track_obj.h>

#include "theme/Theme.hpp"

namespace dp::ui {

static double uniform(std::mt19937& rng, double lo, double hi) {
    return std::uniform_real_distribution<double>(lo, hi)(rng);
}

void WaveView::Param::retarget(std::mt19937& rng) { target = uniform(rng, lo, hi); }

void WaveView::Param::step(double dt, double rate, std::mt19937& rng) {
    // exponential approach: smooth, framerate independent.
    value += (target - value) * (1.0 - std::exp(-rate * dt));
    if (std::abs(target - value) < (hi - lo) * 0.02) retarget(rng);
}

WaveView::WaveView() {
    set_can_target(false);
    set_hexpand(true);
    // a small range around a random center for every parameter of every wave.
    auto make = [this](double center_lo, double center_hi, double spread) {
        const double c = uniform(rng_, center_lo, center_hi);
        Param p{c - spread, c + spread, c, c};
        p.value = uniform(rng_, p.lo, p.hi);
        p.retarget(rng_);
        return p;
    };
    const double alphas[] = {0.10, 0.14, 0.20};
    for (size_t i = 0; i < waves_.size(); ++i) {
        Wave& w = waves_[i];
        w.amp = make(0.10, 0.22, 0.05);
        w.cycles = make(0.7, 2.0, 0.25);
        w.speed = make(0.35, 1.1, 0.2);
        w.level = make(0.45, 0.70, 0.06);
        w.phase = uniform(rng_, 0, 2 * M_PI);
        w.dir = std::bernoulli_distribution(0.5)(rng_) ? 1 : -1;
        w.alpha = alphas[i];
    }
    set_draw_func(sigc::mem_fun(*this, &WaveView::on_draw));
    Theme::get().signal_changed().connect(sigc::track_obj([this] { queue_draw(); }, *this));
}

void WaveView::set_playing(bool playing) {
    if (playing == playing_) return;
    playing_ = playing;
    if (playing_ && !tick_) {
        last_us_ = 0;
        tick_ = add_tick_callback(sigc::mem_fun(*this, &WaveView::on_tick));
    } else if (!playing_ && tick_) {
        remove_tick_callback(tick_);
        tick_ = 0;
    }
}

bool WaveView::on_tick(const Glib::RefPtr<Gdk::FrameClock>& clock) {
    const gint64 now = clock->get_frame_time();
    const double dt = last_us_ ? std::min(0.1, (now - last_us_) / 1e6) : 0.0;
    last_us_ = now;
    for (Wave& w : waves_) {
        w.amp.step(dt, 0.35, rng_);
        w.cycles.step(dt, 0.2, rng_);
        w.speed.step(dt, 0.3, rng_);
        w.level.step(dt, 0.25, rng_);
        w.phase = std::fmod(w.phase + w.dir * w.speed.value * dt, 2 * M_PI);
    }
    queue_draw();
    return true;
}

void WaveView::on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) {
    if (width <= 0 || height <= 0) return;
    const Gdk::RGBA c = Theme::get().color("accent");
    const double step = 4.0;
    for (const Wave& w : waves_) {
        const double k = 2 * M_PI * w.cycles.value / width;
        const double amp = w.amp.value * height, base = w.level.value * height;
        cr->move_to(0, base + amp * std::sin(w.phase));
        for (double x = step; x <= width + step; x += step) cr->line_to(x, base + amp * std::sin(k * x + w.phase));
        // the crest line, then the body down to the bottom edge.
        cr->set_source_rgba(c.get_red(), c.get_green(), c.get_blue(), std::min(1.0, w.alpha * 2.2));
        cr->set_line_width(1.2);
        cr->stroke_preserve();
        cr->line_to(width, height);
        cr->line_to(0, height);
        cr->close_path();
        cr->set_source_rgba(c.get_red(), c.get_green(), c.get_blue(), w.alpha);
        cr->fill();
    }
}

}  // namespace dp::ui
