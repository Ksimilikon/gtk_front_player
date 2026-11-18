#include "ui/widgets/TrackRow.hpp"

#include <gtkmm/drawingarea.h>

#include <cmath>

#include "ui/widgets/Common.hpp"
#include "util/Format.hpp"

namespace dp::ui {

TrackRow::TrackRow(const core::Track& t, int number, bool show_album) : id_(t.id), number_(number) {
    add_css_class("track-row");
    if (t.invalid) add_css_class("invalid");
    num_.set_text(number > 0 ? std::to_string(number) : "");
    num_.add_css_class("track-num");
    num_.set_size_request(32, -1);
    num_.set_xalign(1);
    num_.set_visible(number > 0);  // unnumbered lists (the library in the editor)
    box_.append(num_);

    if (!t.color.empty()) {
        auto* dot = Gtk::make_managed<Gtk::DrawingArea>();
        dot->set_content_width(10);
        dot->set_content_height(10);
        dot->set_valign(Gtk::Align::CENTER);
        Gdk::RGBA rgba;
        if (!rgba.set(t.color)) rgba.set("#888888");
        dot->set_draw_func([rgba](const Cairo::RefPtr<Cairo::Context>& cr, int w, int h) {
            cr->arc(w / 2.0, h / 2.0, std::min(w, h) / 2.0, 0, 2 * M_PI);
            cr->set_source_rgba(rgba.get_red(), rgba.get_green(), rgba.get_blue(), 1.0);
            cr->fill();
        });
        box_.append(*dot);
    }

    auto* text = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 1);
    text->set_hexpand(true);
    auto* title = make_label(t.display_title(), "track-title");
    auto* artists = make_label(t.artists_text(), "muted");
    text->append(*title);
    text->append(*artists);
    box_.append(*text);

    if (!t.label.empty()) {
        auto* label = make_label(t.label, "track-label");
        label->set_valign(Gtk::Align::CENTER);
        box_.append(*label);
    }
    if (show_album) {
        auto* album = make_label(t.album, "muted");
        album->set_size_request(180, -1);
        album->set_max_width_chars(24);
        box_.append(*album);
    }
    auto* dur = make_label(t.duration ? fmt::duration(t.duration) : "", "muted");
    dur->set_size_request(52, -1);
    dur->set_xalign(1);
    box_.append(*dur);
    box_.append(trailing_);
    set_child(box_);

    search_ = t.display_title() + " " + fmt::join(t.artists, " ") + " " + t.album + " " + t.label;
}

void TrackRow::set_playing(bool playing) {
    if (playing) {
        add_css_class("playing");
        num_.set_text("▶");
    } else {
        remove_css_class("playing");
        num_.set_text(number_ > 0 ? std::to_string(number_) : "");
    }
}

}  // namespace dp::ui
