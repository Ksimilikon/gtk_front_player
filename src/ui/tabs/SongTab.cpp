#include "ui/tabs/SongTab.hpp"

#include <gtkmm/button.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/togglebutton.h>

#include <cmath>
#include <filesystem>

#include "core/Covers.hpp"
#include "core/Library.hpp"
#include "core/Mods.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/Menus.hpp"
#include "ui/Nav.hpp"
#include "util/Format.hpp"
#include "util/Log.hpp"

namespace dp::ui {

using core::Domain;
namespace fs = std::filesystem;

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

static Gtk::Widget* color_dot(const std::string& color, int size) {
    auto* dot = Gtk::make_managed<Gtk::DrawingArea>();
    dot->set_content_width(size);
    dot->set_content_height(size);
    dot->set_valign(Gtk::Align::CENTER);
    Gdk::RGBA rgba;
    if (!rgba.set(color)) rgba.set("#888888");
    dot->set_draw_func([rgba](const Cairo::RefPtr<Cairo::Context>& cr, int w, int h) {
        cr->rectangle(0, 0, w, h);
        cr->set_source_rgba(rgba.get_red(), rgba.get_green(), rgba.get_blue(), 1.0);
        cr->fill();
    });
    return dot;
}

SongTab::SongTab(int64_t id) : id_(id) {
    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_vexpand(true);
    // a centered column.
    auto* page = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    page->add_css_class("song-page");
    page->set_halign(Gtk::Align::CENTER);
    page->set_size_request(640, -1);

    // the cover in the center with the song's volume along its side.
    auto* top = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    top->set_halign(Gtk::Align::CENTER);
    auto* vol = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    volume_.set_range(0, 2);
    volume_.set_increments(0.05, 0.1);
    volume_.set_inverted(true);  // louder upward
    volume_.set_draw_value(false);
    volume_.add_mark(1.0, Gtk::PositionType::LEFT, "");
    volume_.set_size_request(-1, 236);
    volume_.set_vexpand(true);
    volume_.add_css_class("song-volume");
    volume_.set_tooltip_text(tr("song.volume"));
    no_wheel(volume_);
    volume_.signal_value_changed().connect([this] {
        volume_value_.set_text(std::to_string(static_cast<int>(std::lround(volume_.get_value() * 100))) + "%");
        if (loading_) return;
        const float v = static_cast<float>(volume_.get_value());
        if (const auto c = Player::get().current(); c && c->id == id_ && Player::get().state().has_track)
            Player::get().set_track_volume(v);
        else
            idx_set_track_volume(id_, v);
    });
    volume_value_.add_css_class("mono");
    volume_value_.add_css_class("muted");
    vol->append(volume_);
    vol->append(volume_value_);
    top->append(*vol);
    top->append(cover_);
    // a spacer as wide as the slider column keeps the cover centered.
    auto* spacer = Gtk::make_managed<Gtk::Box>();
    spacer->set_size_request(44, -1);
    top->append(*spacer);
    page->append(*top);

    heading_.add_css_class("page-title");
    heading_.set_justify(Gtk::Justification::CENTER);
    heading_.set_wrap(true);
    subheading_.add_css_class("accent");
    subheading_.set_justify(Gtk::Justification::CENTER);
    subheading_.set_wrap(true);
    page->append(heading_);
    page->append(subheading_);
    actions_.set_halign(Gtk::Align::CENTER);
    page->append(actions_);
    page->append(body_);

    scroll->set_child(*page);
    append(*scroll);

    for (auto* e : {&title_, &artists_, &album_, &genres_, &label_, &path_})
        e->signal_activate().connect([this] { save_changes(); });
    artists_.set_placeholder_text(tr("song.comma_hint"));
    genres_.set_placeholder_text(tr("song.comma_hint"));
    label_.set_placeholder_text(tr("song.label_hint"));
    keep_.set_label(tr("song.keep"));
    keep_.set_tooltip_text(tr("song.keep_hint"));
    error_.add_css_class("error-text");
    error_.set_xalign(0);
    error_.set_wrap(true);

    Library::get().signal_changed().connect(sigc::mem_fun(*this, &SongTab::reload));
    Mods::get().signal_changed().connect(sigc::mem_fun(*this, &SongTab::reload));
    reload();
}

void SongTab::on_activated() {
    if (!editing_) reload();
}

void SongTab::reload() {
    if (editing_) return;  // don't overwrite what is being typed
    auto t = core::fetch_track(id_);
    if (!t) {
        signal_close_request.emit();
        return;
    }
    loading_ = true;
    track_ = *t;
    heading_.set_text(track_.display_title());
    subheading_.set_text(track_.artists_text() + (track_.album.empty() ? "" : "  ·  " + track_.album));
    cover_.set(Covers::get().track(id_));
    volume_.set_value(track_.volume);
    loading_ = false;
    signal_title_changed.emit();
    build_view();
}

void SongTab::set_editing(bool on) {
    editing_ = on;
    if (on) {
        build_edit();
    } else {
        reload();
    }
}

void SongTab::build_view() {
    while (auto* c = actions_.get_first_child()) actions_.remove(*c);
    while (auto* c = body_.get_first_child()) body_.remove(*c);

    auto* play = Gtk::make_managed<Gtk::Button>(tr("action.play"));
    play->add_css_class("suggested-action");
    play->signal_clicked().connect([this] { Player::get().play_queue(track_.display_title(), {{id_, {}}}, 0); });
    actions_.append(*play);
    auto* next = Gtk::make_managed<Gtk::Button>(tr("action.play_next"));
    next->signal_clicked().connect([this] { Player::get().play_next({id_, {}}); });
    actions_.append(*next);
    auto* add = Gtk::make_managed<Gtk::Button>(tr("action.add_to"));
    add->signal_clicked().connect([this] {
        if (auto* w = window_of(*this)) add_to_playlist(*w, {id_});
    });
    actions_.append(*add);
    auto* edit = Gtk::make_managed<Gtk::Button>(tr("song.edit"));
    edit->signal_clicked().connect([this] { defer([this] { set_editing(true); }); });
    actions_.append(*edit);

    body_.append(*section_title(tr("song.tags")));
    body_.append(*info_row(tr("song.title"), track_.title, true));
    body_.append(*info_row(tr("song.artists"), fmt::join(track_.artists, ", "), true));
    body_.append(*info_row(tr("song.album"), track_.album, true));
    body_.append(*info_row(tr("song.genres"), fmt::join(track_.genres, ", "), true));
    if (!track_.label.empty()) body_.append(*info_row(tr("song.label"), track_.label, true));
    if (!track_.color.empty()) {
        auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
        auto* n = make_label(tr("song.color"), "muted");
        n->set_size_request(160, -1);
        row->append(*n);
        row->append(*color_dot(track_.color, 12));
        body_.append(*row);
    }
    if (track_.ignore_orphan) body_.append(*info_row(tr("song.keep"), tr("common.yes")));

    // the playlists the song is in.
    body_.append(*section_title(tr("song.in_playlists")));
    auto* pls = Gtk::make_managed<Gtk::FlowBox>();
    pls->set_selection_mode(Gtk::SelectionMode::NONE);
    pls->set_max_children_per_line(6);
    int n = 0;
    for (const auto& p : Library::get().playlists()) {
        if (std::find(p.tracks.begin(), p.tracks.end(), id_) == p.tracks.end()) continue;
        auto* b = Gtk::make_managed<Gtk::Button>();
        b->add_css_class("chip");
        auto* chip = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
        auto* cv = Gtk::make_managed<CoverView>(28);
        cv->set_file_async(p.cover, p.tracks.empty() ? 0 : p.tracks.front());
        chip->append(*cv);
        chip->append(*make_label(p.name));
        b->set_child(*chip);
        const std::string name = p.name;
        b->signal_clicked().connect([name] { nav().open_playlist(name); });
        pls->append(*b);
        ++n;
    }
    if (n == 0)
        body_.append(*make_label(tr("song.in_no_playlist"), "muted"));
    else
        body_.append(*pls);

    body_.append(*section_title(tr("song.info")));
    body_.append(*info_row(tr("song.duration"), fmt::duration(track_.duration)));
    if (track_.bitrate) body_.append(*info_row(tr("song.bitrate"), std::to_string(track_.bitrate) + " kbps"));
    if (track_.sample_rate) body_.append(*info_row(tr("song.sample_rate"), std::to_string(track_.sample_rate) + " Hz"));
    body_.append(*info_row(tr("song.listens"), std::to_string(track_.listen_count)));
    body_.append(*info_row(tr("song.path"), track_.path, true));
    if (!track_.cover_art.empty()) body_.append(*info_row(tr("song.cover_path"), track_.cover_art, true));
    body_.append(*info_row(tr("song.id"), std::to_string(track_.id), true));
    if (track_.invalid) body_.append(*banner(tr("song.invalid"), true));

    auto mods = Mods::get().actions(UI_PLACE_SONG);
    if (!mods.empty()) {
        auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
        row->set_margin_top(10);
        for (const auto& a : mods) {
            auto* b = Gtk::make_managed<Gtk::Button>(a.title);
            b->set_tooltip_text(a.mod_name);
            b->add_css_class("mod-item");
            b->signal_clicked().connect([a, id = id_] { Mods::get().call(a, static_cast<uint64_t>(id)); });
            row->append(*b);
        }
        body_.append(*row);
    }
}

void SongTab::build_edit() {
    while (auto* c = actions_.get_first_child()) actions_.remove(*c);
    while (auto* c = body_.get_first_child()) body_.remove(*c);

    auto* save = Gtk::make_managed<Gtk::Button>(tr("common.save_ctrl_s"));
    save->add_css_class("suggested-action");
    save->signal_clicked().connect([this] { save_changes(); });
    actions_.append(*save);
    auto* cancel = Gtk::make_managed<Gtk::Button>(tr("common.cancel"));
    cancel->signal_clicked().connect([this] { defer([this] { set_editing(false); }); });
    actions_.append(*cancel);

    title_.set_text(track_.title);
    artists_.set_text(fmt::join(track_.artists, ", "));
    album_.set_text(track_.album);
    genres_.set_text(fmt::join(track_.genres, ", "));
    label_.set_text(track_.label);
    path_.set_text(track_.path);
    keep_.set_active(track_.ignore_orphan);
    edit_color_ = track_.color;
    error_.set_text("");

    body_.append(*section_title(tr("song.tags")));
    body_.append(*field(tr("song.title"), title_));
    body_.append(*field(tr("song.artists"), artists_));
    body_.append(*field(tr("song.album"), album_));
    body_.append(*field(tr("song.genres"), genres_));

    body_.append(*section_title(tr("song.marks")));
    body_.append(*make_label(tr("song.color"), "muted"));
    auto* colors = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    std::vector<Gtk::ToggleButton*> swatches;
    auto* none = Gtk::make_managed<Gtk::ToggleButton>(tr("song.no_color"));
    none->set_active(edit_color_.empty());
    colors->append(*none);
    swatches.push_back(none);
    for (const char* c : kColors) {
        auto* b = Gtk::make_managed<Gtk::ToggleButton>();
        b->add_css_class("swatch");
        b->set_child(*color_dot(c, 12));
        b->set_active(edit_color_ == c);
        b->set_group(*none);
        colors->append(*b);
        swatches.push_back(b);
    }
    for (size_t i = 0; i < swatches.size(); ++i) {
        const std::string color = i == 0 ? "" : kColors[i - 1];
        swatches[i]->signal_toggled().connect([this, color, b = swatches[i]] {
            if (b->get_active()) edit_color_ = color;
        });
    }
    body_.append(*colors);
    body_.append(*field(tr("song.label"), label_));
    body_.append(keep_);

    body_.append(*section_title(tr("song.file")));
    body_.append(*field(tr("song.path_edit"), path_));
    body_.append(*make_label(tr("song.path_hint"), "muted", true));
    auto* cover_row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    auto* cover_file = Gtk::make_managed<Gtk::Button>(tr("song.cover_file"));
    cover_file->set_tooltip_text(tr("song.cover_file_hint"));
    cover_file->signal_clicked().connect([this] {
        if (auto* w = window_of(*this))
            pick_file(*w, tr("song.cover_file"), {"*.png", "*.jpg", "*.jpeg", "*.webp"}, tr("dialog.images"),
                      [this](const std::string& p) {
                          if (uint32_t st = idx_set_track_cover(id_, p.c_str()); st != 0)
                              Log::get().error("core", core::status_text(Domain::Idx, st));
                          Covers::get().invalidate_track(id_);
                          cover_.set(Covers::get().track(id_));
                      });
    });
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
                          cover_.set(Covers::get().track(id_));
                      });
    });
    auto* clear = Gtk::make_managed<Gtk::Button>(tr("song.cover_clear"));
    clear->signal_clicked().connect([this] {
        idx_set_track_cover(id_, nullptr);
        Covers::get().invalidate_track(id_);
        cover_.set(Covers::get().track(id_));
    });
    cover_row->append(*cover_file);
    cover_row->append(*embed);
    cover_row->append(*clear);
    body_.append(*cover_row);
    body_.append(error_);
    title_.grab_focus();
}

bool SongTab::has_unsaved() const {
    if (!editing_) return false;
    return fmt::trim(title_.get_text()) != track_.title || fmt::split(artists_.get_text(), ',') != track_.artists ||
           fmt::trim(album_.get_text()) != track_.album || fmt::split(genres_.get_text(), ',') != track_.genres ||
           fmt::trim(label_.get_text()) != track_.label || fmt::trim(path_.get_text()) != track_.path ||
           keep_.get_active() != track_.ignore_orphan || edit_color_ != track_.color;
}

/// moves the audio file and tells the core its new path.
bool SongTab::rename_file(const std::string& to) {
    std::error_code ec;
    const fs::path from(track_.path), target(to);
    if (fs::exists(target, ec)) {
        error_.set_text(tr("song.err.path_exists"));
        return false;
    }
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    fs::rename(from, target, ec);
    if (ec) {
        // another filesystem: copy, then remove the original.
        ec.clear();
        fs::copy_file(from, target, ec);
        if (!ec) fs::remove(from, ec);
    }
    if (ec) {
        error_.set_text(tr("song.err.move", {ec.message()}));
        return false;
    }
    if (uint32_t st = idx_set_track_path(id_, to.c_str()); st != 0) {
        fs::rename(target, from, ec);  // keep the file where the db expects it
        error_.set_text(core::status_text(Domain::Idx, st));
        return false;
    }
    return true;
}

bool SongTab::save_changes() {
    if (!editing_) return true;
    const std::string title = fmt::trim(title_.get_text());
    if (title.empty()) {
        error_.set_text(tr("song.err.empty_title"));
        return false;
    }
    const auto artists = fmt::split(artists_.get_text(), ',');
    const auto genres = fmt::split(genres_.get_text(), ',');
    const std::string album = fmt::trim(album_.get_text());
    if (title != track_.title || artists != track_.artists || album != track_.album || genres != track_.genres) {
        core::CStrings a(artists), g(genres);
        static const char* empty = nullptr;  // non-null with len 0 clears a list
        const uint32_t st = idx_set_track_meta(id_, title.c_str(), artists.empty() ? &empty : a.data(),
                                               artists.size(), album.c_str(), genres.empty() ? &empty : g.data(),
                                               genres.size());
        if (st != 0) {
            error_.set_text(tr("song.err.save", {core::status_text(Domain::Idx, st)}));
            return false;
        }
    }
    if (const std::string l = fmt::trim(label_.get_text()); l != track_.label)
        idx_set_track_label(id_, l.empty() ? nullptr : l.c_str());
    if (edit_color_ != track_.color) idx_set_track_color(id_, edit_color_.empty() ? nullptr : edit_color_.c_str());
    if (keep_.get_active() != track_.ignore_orphan) idx_set_track_ignore_orphan(id_, keep_.get_active());
    if (const std::string p = fmt::trim(path_.get_text()); !p.empty() && p != track_.path && !rename_file(p))
        return false;
    Log::get().ok("core", tr("song.saved", {title}));
    editing_ = false;
    Library::get().reload();  // also rebuilds this page (view mode)
    reload();
    return true;
}

}  // namespace dp::ui
