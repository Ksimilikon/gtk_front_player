#include "ui/widgets/VolumeZone.hpp"

#include <glibmm/main.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gesturedrag.h>
#include <pangomm/layout.h>
#include <sigc++/adaptors/track_obj.h>

#include <algorithm>
#include <cmath>

#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "theme/Theme.hpp"

namespace dp::ui {

VolumeZone::VolumeZone() {
    add_css_class("volume-zone");
    set_hexpand(true);
    set_content_height(28);
    set_cursor("ew-resize");
    set_draw_func(sigc::mem_fun(*this, &VolumeZone::on_draw));

    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_enter().connect([this](double, double) {
        hover_ = true;
        queue_draw();
    });
    motion->signal_leave().connect([this] {
        hover_ = false;
        queue_draw();
    });
    add_controller(motion);

    auto drag = Gtk::GestureDrag::create();
    drag->signal_drag_begin().connect([this](double x, double) {
        dragging_ = true;
        drag_x_ = x;
        set_from_x(x);
    });
    drag->signal_drag_update().connect([this](double dx, double) { set_from_x(drag_x_ + dx); });
    drag->signal_drag_end().connect([this](double dx, double) {
        set_from_x(drag_x_ + dx);
        dragging_ = false;
        queue_draw();
    });
    add_controller(drag);

    Player::get().signal_volume_changed().connect(sigc::track_obj([this] { queue_draw(); }, *this));
    Theme::get().signal_changed().connect(sigc::track_obj([this] { queue_draw(); }, *this));
}

void VolumeZone::set_from_x(double x) {
    const int w = get_width();
    if (w <= 0) return;
    // snap to whole percents.
    Player::get().set_volume(std::round(std::clamp(x / w, 0.0, 1.0) * 100) / 100);
    queue_draw();
}

void VolumeZone::show_status(Level level, const std::string& text) {
    level_ = level;
    status_ = text;
    queue_draw();
    timer_.disconnect();
    timer_ = Glib::signal_timeout().connect(
        [this] {
            status_.clear();
            queue_draw();
            return false;
        },
        level == Level::Error ? 12000 : 6000);
}

void VolumeZone::on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int w, int h) {
    const double v = Player::get().volume();
    const Gdk::RGBA accent = Theme::get().color("accent");
    const Gdk::RGBA border = Theme::get().color("border");
    const bool active = hover_ || dragging_;

    // the level: a faint fill and a bright bottom line up to it; ticks every 10%.
    cr->set_source_rgba(accent.get_red(), accent.get_green(), accent.get_blue(), active ? 0.14 : 0.07);
    cr->rectangle(0, 0, w * v, h);
    cr->fill();
    cr->set_source_rgba(border.get_red(), border.get_green(), border.get_blue(), 1.0);
    cr->rectangle(0, h - 2, w, 2);
    cr->fill();
    cr->set_source_rgba(accent.get_red(), accent.get_green(), accent.get_blue(), active ? 1.0 : 0.6);
    cr->rectangle(0, h - 2, w * v, 2);
    cr->fill();
    for (int i = 1; i < 10; ++i) {
        cr->rectangle(std::floor(w * i / 10.0), h - 5, 1, 3);
    }
    cr->set_source_rgba(border.get_red(), border.get_green(), border.get_blue(), 1.0);
    cr->fill();

    // text: the volume while pointed at, otherwise the latest status.
    std::string text;
    Gdk::RGBA color;
    if (active) {
        text = tr("controls.volume_value", {std::to_string(static_cast<int>(std::lround(v * 100)))});
        color = accent;
    } else if (!status_.empty()) {
        text = status_;
        static const char* tokens[] = {"text", "accent", "warning", "error"};
        color = Theme::get().color(tokens[static_cast<int>(level_)]);
    }
    if (text.empty()) return;
    auto layout = create_pango_layout(text);
    layout->set_ellipsize(Pango::EllipsizeMode::END);
    layout->set_width((w - 16) * PANGO_SCALE);
    int lw = 0, lh = 0;
    layout->get_pixel_size(lw, lh);
    cr->set_source_rgba(color.get_red(), color.get_green(), color.get_blue(), 0.95);
    cr->move_to(8, (h - 2 - lh) / 2.0);
    layout->show_in_cairo_context(cr);
}

}  // namespace dp::ui
