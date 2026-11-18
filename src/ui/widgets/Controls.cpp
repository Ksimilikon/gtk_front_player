#include "ui/widgets/Controls.hpp"

#include "core/Player.hpp"
#include "i18n/I18n.hpp"

namespace dp::ui {

Controls::Controls() : Gtk::Box(Gtk::Orientation::HORIZONTAL, 2) {
    add_css_class("controls");
    set_halign(Gtk::Align::END);
    set_valign(Gtk::Align::CENTER);

    shuffle_.set_icon_name("media-playlist-shuffle-symbolic");
    shuffle_.set_tooltip_text(tr("controls.shuffle"));
    prev_.set_icon_name("media-skip-backward-symbolic");
    prev_.set_tooltip_text(tr("controls.previous"));
    play_.set_icon_name("media-playback-start-symbolic");
    play_.set_tooltip_text(tr("controls.play_pause"));
    play_.add_css_class("play-button");
    next_.set_icon_name("media-skip-forward-symbolic");
    next_.set_tooltip_text(tr("controls.next"));
    repeat_.set_icon_name("media-playlist-repeat-song-symbolic");
    repeat_.set_tooltip_text(tr("controls.repeat_one"));
    volume_icon_.set_icon_name("audio-volume-high-symbolic");
    volume_icon_.set_tooltip_text(tr("controls.mute"));
    for (Gtk::Widget* w : std::initializer_list<Gtk::Widget*>{&shuffle_, &prev_, &next_, &repeat_, &volume_icon_})
        w->add_css_class("flat");
    for (Gtk::Widget* w : std::initializer_list<Gtk::Widget*>{&shuffle_, &prev_, &play_, &next_, &repeat_, &volume_icon_})
        w->set_focusable(false);  // Space must stay play / pause, not "press the focused button"

    volume_.set_range(0, 1);
    volume_.set_increments(0.02, 0.1);
    volume_.set_draw_value(false);
    volume_.set_size_request(96, -1);
    volume_.set_focusable(false);
    volume_.add_css_class("volume");
    volume_.set_tooltip_text(tr("controls.volume"));

    append(shuffle_);
    append(prev_);
    append(play_);
    append(next_);
    append(repeat_);
    append(volume_icon_);
    append(volume_);

    auto& p = Player::get();
    prev_.signal_clicked().connect([] { Player::get().previous(); });
    play_.signal_clicked().connect([] { Player::get().toggle(); });
    next_.signal_clicked().connect([] { Player::get().next(); });
    shuffle_.signal_toggled().connect([this] {
        if (!updating_) Player::get().set_shuffle(shuffle_.get_active());
    });
    repeat_.signal_toggled().connect([this] {
        if (!updating_) Player::get().set_repeat_one(repeat_.get_active());
    });
    volume_.signal_value_changed().connect([this] {
        if (!updating_) Player::get().set_volume(volume_.get_value());
    });
    volume_icon_.signal_clicked().connect([this] {
        auto& pl = Player::get();
        if (pl.volume() > 0) {
            muted_from_ = pl.volume();
            pl.set_volume(0);
        } else {
            pl.set_volume(muted_from_ > 0 ? muted_from_ : 1.0);
        }
    });
    p.signal_state_changed().connect(sigc::mem_fun(*this, &Controls::sync));
    p.signal_modes_changed().connect(sigc::mem_fun(*this, &Controls::sync));
    p.signal_volume_changed().connect(sigc::mem_fun(*this, &Controls::sync));
    sync();
}

void Controls::sync() {
    auto& p = Player::get();
    updating_ = true;
    play_.set_icon_name(p.state().playing ? "media-playback-pause-symbolic" : "media-playback-start-symbolic");
    shuffle_.set_active(p.shuffle());
    repeat_.set_active(p.repeat_one());
    if (std::abs(volume_.get_value() - p.volume()) > 1e-3) volume_.set_value(p.volume());
    const double v = p.volume();
    volume_icon_.set_icon_name(v <= 0      ? "audio-volume-muted-symbolic"
                               : v < 0.34 ? "audio-volume-low-symbolic"
                               : v < 0.67 ? "audio-volume-medium-symbolic"
                                          : "audio-volume-high-symbolic");
    updating_ = false;
}

}  // namespace dp::ui
