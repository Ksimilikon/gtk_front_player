#include "ui/widgets/PlayerPanel.hpp"

#include <gtkmm/gestureclick.h>

#include "core/Covers.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "util/Format.hpp"

namespace dp::ui {

PlayerPanel::PlayerPanel() : Gtk::Box(Gtk::Orientation::VERTICAL, 0) {
    add_css_class("player-panel");

    title_.add_css_class("player-title");
    artists_.add_css_class("player-artists");
    time_.add_css_class("player-time");
    for (auto* l : {&title_, &artists_}) {
        l->set_xalign(0);
        l->set_ellipsize(Pango::EllipsizeMode::END);
    }
    text_.set_valign(Gtk::Align::CENTER);
    text_.set_hexpand(true);
    text_.append(title_);
    text_.append(artists_);

    time_.set_valign(Gtk::Align::END);
    time_.set_margin_bottom(4);

    content_.set_margin_start(14);
    content_.set_margin_end(16);
    content_.set_margin_top(10);
    content_.set_margin_bottom(8);
    content_.append(cover_);
    content_.append(text_);
    content_.append(time_);
    content_.set_cursor("pointer");

    auto click = Gtk::GestureClick::create();
    click->set_button(GDK_BUTTON_PRIMARY);
    click->signal_released().connect([this](int, double, double) { open_.emit(); });
    content_.add_controller(click);

    overlay_.set_child(waves_);
    overlay_.add_overlay(content_);
    overlay_.set_measure_overlay(content_, true);
    append(overlay_);
    append(seek_);

    auto& p = Player::get();
    seek_.signal_scrub().connect([this](double f) {
        scrubbing_ = true;
        const auto& s = Player::get().state();
        time_.set_text(fmt::duration(static_cast<uint64_t>(f * s.duration)) + " / " + fmt::duration(s.duration));
    });
    seek_.signal_seek().connect([this](double f) {
        scrubbing_ = false;
        Player::get().seek(static_cast<uint64_t>(f * Player::get().state().duration));
    });
    p.signal_track_changed().connect(sigc::mem_fun(*this, &PlayerPanel::on_track));
    p.signal_state_changed().connect(sigc::mem_fun(*this, &PlayerPanel::on_state));
    on_track();
    on_state();
}

void PlayerPanel::on_track() {
    auto& p = Player::get();
    if (!p.state().has_track) {
        title_.set_text(tr("player.nothing"));
        artists_.set_text(tr("player.nothing_hint"));
        cover_.set(nullptr);
        return;
    }
    title_.set_text(p.current_title());
    artists_.set_text(p.current_artists());
    const auto cur = p.current();
    cover_.set(cur->id ? Covers::get().track(cur->id) : Covers::get().track_path(cur->path));
}

void PlayerPanel::on_state() {
    const auto& s = Player::get().state();
    waves_.set_playing(s.playing);
    seek_.set_fraction(s.duration ? static_cast<double>(s.position) / s.duration : 0.0);
    if (!scrubbing_)
        time_.set_text(s.has_track ? fmt::duration(s.position) + " / " + fmt::duration(s.duration) : "");
}

}  // namespace dp::ui
