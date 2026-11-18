#include "ui/tabs/SongTab.hpp"

#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/togglebutton.h>

#include "core/Covers.hpp"
#include "core/Library.hpp"
#include "core/Mods.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/Nav.hpp"
#include "util/Format.hpp"
#include "util/Log.hpp"

namespace dp::ui {

using core::Domain;

// the color marks offered (any CSS color string is accepted by the core).
static const char* kColors[] = {"#1ed760", "#f5c400", "#ff4d4f", "#3b82f6", "#a855f7", "#ec4899", "#f97316"};

std::string SongTab::title() const {
    return track_.title.empty() && track_.path.empty() ? tr("page.song") : track_.display_title();
}

static Gtk::Box* field(const std::string& name, Gtk::Widget& w) {
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
    box->append(*make_label(name, "muted"));
    box->append(w);
    return box;
}

SongTab::SongTab(int64_t id) : id_(id) {
    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_vexpand(true);
    auto* page = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 28);
    page->add_css_class("song-page");

    // left: cover + its actions + playback.
    auto* left = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    left->append(cover_);
    auto* play = Gtk::make_managed<Gtk::Button>(tr("action.play"));
    play->add_css_class("suggested-action");
    play->signal_clicked().connect([this] { Player::get().play_ids(track_.display_title(), {id_}, 0); });
    left->append(*play);
    auto* next = Gtk::make_managed<Gtk::Button>(tr("action.play_next"));
    next->signal_clicked().connect([this] { Player::get().play_next({id_, {}}); });
    left->append(*next);
    auto* cover_file = Gtk::make_managed<Gtk::Button>(tr("song.cover_file"));
    cover_file->set_tooltip_text(tr("song.cover_file_hint"));
    cover_file->signal_clicked().connect([this] {
        if (auto* w = window_of(*this))
            pick_file(*w, tr("song.cover_file"), {"*.png", "*.jpg", "*.jpeg", "*.webp"}, tr("dialog.images"),
                      [this](const std::string& p) {
                          if (uint32_t st = idx_set_track_cover(id_, p.c_str()); st != 0)
                              Log::get().error("core", core::status_text(Domain::Idx, st));
                          Covers::get().invalidate_track(id_);
                          Library::get().reload();
                      });
    });
    left->append(*cover_file);
    auto* embed = Gtk::make_managed<Gtk::Button>(tr("song.cover_embed"));
    embed->set_tooltip_text(tr("song.cover_embed_hint"));
    embed->signal_clicked().connect([this] {
        if (auto* w = window_of(*this))
            pick_file(*w, tr("song.cover_embed"), {"*.png", "*.jpg", "*.jpeg"}, tr("dialog.images"),
                      [this](const std::string& p) {
                          if (uint32_t st = idx_embed_track_cover(id_, p.c_str()); st != 0)
                              Log::get().error("core", core::status_text(Domain::Idx, st));
                          else
                              Log::get().ok("core", tr("song.cover_embedded"));
                          Covers::get().invalidate_track(id_);
                          Library::get().reload();
                      });
    });
    left->append(*embed);
    auto* clear = Gtk::make_managed<Gtk::Button>(tr("song.cover_clear"));
    clear->signal_clicked().connect([this] {
        idx_set_track_cover(id_, nullptr);
        Covers::get().invalidate_track(id_);
        Library::get().reload();
    });
    left->append(*clear);
    page->append(*left);

    // right: tags, marks, info.
    auto* right = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    right->set_hexpand(true);
    heading_.add_css_class("page-title");
    heading_.set_xalign(0);
    heading_.set_wrap(true);
    artists_heading_.add_css_class("accent");
    artists_heading_.set_xalign(0);
    artists_heading_.set_wrap(true);
    right->append(heading_);
    right->append(artists_heading_);

    right->append(*section_title(tr("song.tags")));
    artists_.set_placeholder_text(tr("song.comma_hint"));
    genres_.set_placeholder_text(tr("song.comma_hint"));
    right->append(*field(tr("song.title"), title_));
    right->append(*field(tr("song.artists"), artists_));
    right->append(*field(tr("song.album"), album_));
    right->append(*field(tr("song.genres"), genres_));
    auto* save = Gtk::make_managed<Gtk::Button>(tr("song.save_tags"));
    save->add_css_class("suggested-action");
    save->set_halign(Gtk::Align::START);
    save->set_tooltip_text(tr("song.save_tags_hint"));
    save->signal_clicked().connect(sigc::mem_fun(*this, &SongTab::save_tags));
    right->append(*save);

    right->append(*section_title(tr("song.marks")));
    right->append(*make_label(tr("song.color"), "muted"));
    right->append(colors_);
    label_.set_placeholder_text(tr("song.label_hint"));
    label_.signal_activate().connect([this] {
        const std::string l = fmt::trim(label_.get_text());
        idx_set_track_label(id_, l.empty() ? nullptr : l.c_str());
        Library::get().reload();
    });
    right->append(*field(tr("song.label"), label_));
    volume_.set_range(0, 2);
    volume_.set_increments(0.05, 0.1);
    volume_.set_draw_value(true);
    volume_.set_digits(2);
    volume_.add_mark(1.0, Gtk::PositionType::BOTTOM, "");
    volume_.signal_value_changed().connect([this] {
        if (loading_) return;
        const float v = static_cast<float>(volume_.get_value());
        if (const auto c = Player::get().current(); c && c->id == id_ && Player::get().state().has_track)
            Player::get().set_track_volume(v);
        else
            idx_set_track_volume(id_, v);
    });
    right->append(*field(tr("song.volume"), volume_));
    keep_.set_label(tr("song.keep"));
    keep_.set_tooltip_text(tr("song.keep_hint"));
    keep_.signal_toggled().connect([this] {
        if (!loading_) idx_set_track_ignore_orphan(id_, keep_.get_active());
    });
    right->append(keep_);

    right->append(*section_title(tr("song.info")));
    right->append(info_);
    mod_actions_.set_margin_top(10);
    right->append(mod_actions_);
    page->append(*right);

    scroll->set_child(*page);
    append(*scroll);
    Library::get().signal_changed().connect(sigc::mem_fun(*this, &SongTab::reload));
    Mods::get().signal_changed().connect(sigc::mem_fun(*this, &SongTab::reload));
    reload();
}

void SongTab::reload() {
    auto t = core::fetch_track(id_);
    if (!t) {
        signal_close_request.emit();
        return;
    }
    loading_ = true;
    track_ = *t;
    heading_.set_text(track_.display_title());
    artists_heading_.set_text(track_.artists_text() + (track_.album.empty() ? "" : "  ·  " + track_.album));
    cover_.set(Covers::get().track(id_));
    title_.set_text(track_.title);
    artists_.set_text(fmt::join(track_.artists, ", "));
    album_.set_text(track_.album);
    genres_.set_text(fmt::join(track_.genres, ", "));
    label_.set_text(track_.label);
    volume_.set_value(track_.volume);
    keep_.set_active(track_.ignore_orphan);
    signal_title_changed.emit();

    while (auto* c = colors_.get_first_child()) colors_.remove(*c);
    auto* none = Gtk::make_managed<Gtk::ToggleButton>(tr("song.no_color"));
    none->set_active(track_.color.empty());
    none->signal_clicked().connect([this] { set_color(""); });
    colors_.append(*none);
    for (const char* c : kColors) {
        auto* b = Gtk::make_managed<Gtk::ToggleButton>();
        b->add_css_class("swatch");
        auto* dot = Gtk::make_managed<Gtk::Label>("●");
        auto attrs = Pango::AttrList();
        Gdk::RGBA rgba(c);
        auto fg = Pango::Attribute::create_attr_foreground(static_cast<guint16>(rgba.get_red_u()),
                                                           static_cast<guint16>(rgba.get_green_u()),
                                                           static_cast<guint16>(rgba.get_blue_u()));
        attrs.insert(fg);
        dot->set_attributes(attrs);
        b->set_child(*dot);
        b->set_active(track_.color == c);
        const std::string color = c;
        b->signal_clicked().connect([this, color] { set_color(color); });
        colors_.append(*b);
    }

    while (auto* c = info_.get_first_child()) info_.remove(*c);
    info_.append(*info_row(tr("song.duration"), fmt::duration(track_.duration)));
    if (track_.bitrate) info_.append(*info_row(tr("song.bitrate"), std::to_string(track_.bitrate) + " kbps"));
    if (track_.sample_rate) info_.append(*info_row(tr("song.sample_rate"), std::to_string(track_.sample_rate) + " Hz"));
    info_.append(*info_row(tr("song.listens"), std::to_string(track_.listen_count)));
    info_.append(*info_row(tr("song.path"), track_.path, true));
    if (!track_.cover_art.empty()) info_.append(*info_row(tr("song.cover_path"), track_.cover_art, true));
    info_.append(*info_row(tr("song.id"), std::to_string(track_.id), true));
    if (track_.invalid) info_.append(*banner(tr("song.invalid"), true));

    while (auto* c = mod_actions_.get_first_child()) mod_actions_.remove(*c);
    for (const auto& a : Mods::get().actions(UI_PLACE_SONG)) {
        auto* b = Gtk::make_managed<Gtk::Button>(a.title);
        b->set_tooltip_text(a.mod_name);
        b->add_css_class("mod-item");
        b->signal_clicked().connect([a, id = id_] { Mods::get().call(a, static_cast<uint64_t>(id)); });
        mod_actions_.append(*b);
    }
    loading_ = false;
}

void SongTab::set_color(const std::string& c) {
    idx_set_track_color(id_, c.empty() ? nullptr : c.c_str());
    // deferred: the clicked swatch is rebuilt by the reload.
    defer([] { Library::get().reload(); });
}

void SongTab::save_tags() {
    const std::string title = fmt::trim(title_.get_text());
    if (title.empty()) {
        Log::get().warn("ui", tr("song.err.empty_title"));
        return;
    }
    const auto artists = fmt::split(artists_.get_text(), ',');
    const auto genres = fmt::split(genres_.get_text(), ',');
    const std::string album = fmt::trim(album_.get_text());
    core::CStrings a(artists), g(genres);
    // non-null with len 0 clears a list.
    static const char* empty = nullptr;
    const uint32_t st = idx_set_track_meta(id_, title.c_str(), artists.empty() ? &empty : a.data(), artists.size(),
                                           album.c_str(), genres.empty() ? &empty : g.data(), genres.size());
    if (st != 0) {
        Log::get().error("core", tr("song.err.save", {core::status_text(Domain::Idx, st)}));
        return;
    }
    Log::get().ok("core", tr("song.saved", {title}));
    Library::get().reload();
}

}  // namespace dp::ui
