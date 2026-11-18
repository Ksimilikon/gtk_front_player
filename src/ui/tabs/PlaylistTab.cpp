#include "ui/tabs/PlaylistTab.hpp"

#include <gtkmm/button.h>

#include <algorithm>

#include "core/Covers.hpp"
#include "core/Library.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/Menus.hpp"
#include "ui/Nav.hpp"
#include "ui/widgets/TrackRow.hpp"
#include "util/Format.hpp"

namespace dp::ui {

std::string PlaylistTab::title() const { return name_.empty() ? tr("playlists.all_songs") : name_; }
std::string PlaylistTab::queue_name() const { return title(); }

PlaylistTab::PlaylistTab(std::string name) : name_(std::move(name)) {
    // header: cover, name, stats, actions.
    auto* head = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 18);
    head->add_css_class("page-header");
    head->append(cover_);
    auto* info = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    info->set_valign(Gtk::Align::CENTER);
    info->set_hexpand(true);
    title_.add_css_class("page-title");
    title_.set_xalign(0);
    title_.set_ellipsize(Pango::EllipsizeMode::END);
    stats_.add_css_class("muted");
    stats_.set_xalign(0);
    info->append(title_);
    info->append(stats_);

    auto* actions = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    auto* play = Gtk::make_managed<Gtk::Button>(tr("action.play"));
    play->add_css_class("suggested-action");
    play->signal_clicked().connect([this] {
        Player::get().set_shuffle(false);
        Player::get().play_ids(queue_name(), ids(), 0);
    });
    actions->append(*play);
    auto* shuffle = Gtk::make_managed<Gtk::Button>(tr("action.shuffle_play"));
    shuffle->signal_clicked().connect([this] {
        const auto v = ids();
        if (v.empty()) return;
        Player::get().set_shuffle(true);
        Player::get().play_ids(queue_name(), v, static_cast<size_t>(g_random_int_range(0, static_cast<int>(v.size()))));
    });
    actions->append(*shuffle);
    if (!name_.empty()) {
        auto* edit = Gtk::make_managed<Gtk::Button>(tr("action.edit"));
        edit->signal_clicked().connect([this] { nav().open_editor(name_); });
        actions->append(*edit);
        auto* exp = Gtk::make_managed<Gtk::Button>(tr("action.export"));
        exp->signal_clicked().connect([this] { nav().open_export_playlists({name_}); });
        actions->append(*exp);
    }
    info->append(*actions);
    head->append(*info);
    append(*head);

    search_.set_placeholder_text(tr("playlist.search"));
    search_.set_margin_start(16);
    search_.set_margin_end(16);
    search_.set_margin_bottom(6);
    append(search_);

    list_.add_css_class("plain-list");
    list_.set_selection_mode(Gtk::SelectionMode::NONE);
    list_.signal_row_activated().connect([this](Gtk::ListBoxRow* row) {
        if (auto* r = dynamic_cast<TrackRow*>(row))
            Player::get().play_ids(queue_name(), ids(), static_cast<size_t>(r->number() - 1));
    });
    list_.set_filter_func([this](Gtk::ListBoxRow* row) {
        auto* r = dynamic_cast<TrackRow*>(row);
        return !r || fmt::contains_ci(r->search_text(), search_.get_text());
    });
    search_.signal_search_changed().connect([this] { list_.invalidate_filter(); });
    scroll_.set_child(list_);
    scroll_.set_vexpand(true);
    append(scroll_);

    Library::get().signal_changed().connect(sigc::mem_fun(*this, &PlaylistTab::rebuild));
    Player::get().signal_track_changed().connect(sigc::mem_fun(*this, &PlaylistTab::mark_playing));
    rebuild();
}

std::vector<int64_t> PlaylistTab::ids() const {
    if (name_.empty()) {
        std::vector<int64_t> out;
        for (const auto& t : Library::get().tracks()) out.push_back(t.id);
        return out;
    }
    const auto* p = Library::get().playlist(name_);
    return p ? p->tracks : std::vector<int64_t>{};
}

void PlaylistTab::rebuild() {
    auto& lib = Library::get();
    if (!name_.empty() && !lib.playlist(name_)) {
        // the playlist is gone (deleted / renamed).
        signal_close_request.emit();
        return;
    }
    const auto v = ids();
    title_.set_text(title());
    stats_.set_text(tr("playlists.stats", {std::to_string(v.size()), fmt::duration(lib.total_duration(v))}));
    if (!name_.empty()) cover_.set(Covers::get().file(lib.playlist(name_)->cover));

    while (auto* child = list_.get_first_child()) list_.remove(*child);
    int n = 0;
    for (int64_t id : v) {
        ++n;
        const auto* t = lib.track(id);
        if (!t) continue;
        auto* row = Gtk::make_managed<TrackRow>(*t, n);
        const size_t index = static_cast<size_t>(n - 1);
        on_context_menu(*row, [this, id, index] { return track_menu(id, index); });
        list_.append(*row);
    }
    mark_playing();
}

void PlaylistTab::mark_playing() {
    const auto& p = Player::get();
    const auto cur = p.current();
    const bool ours = p.state().has_track && p.queue_name() == queue_name();
    for (auto* w = list_.get_first_child(); w; w = w->get_next_sibling())
        if (auto* r = dynamic_cast<TrackRow*>(w)) r->set_playing(ours && cur && cur->id == r->track_id());
}

std::vector<MenuItem> PlaylistTab::track_menu(int64_t id, size_t index) {
    std::vector<MenuItem> items;
    items.push_back({tr("action.play"), [this, index] { Player::get().play_ids(queue_name(), ids(), index); }});
    items.push_back({tr("action.play_next"), [id] { Player::get().play_next({id, {}}); }});
    items.push_back({tr("action.open_song"), [id] { nav().open_song(id); }});
    items.push_back({tr("action.export_song"), [id] { nav().open_export_tracks({id}); }});
    if (!name_.empty()) {
        items.push_back({tr("action.remove_from_playlist"),
                         [this, index] {
                             const auto* p = Library::get().playlist(name_);
                             if (!p || index >= p->tracks.size()) return;
                             auto v = p->tracks;
                             v.erase(v.begin() + static_cast<long>(index));
                             Library::get().save_playlist(name_, v, p->cover);
                         },
                         "destructive", true});
    }
    // add to another playlist.
    for (const auto& pl : Library::get().playlists()) {
        if (pl.name == name_) continue;
        const std::string target = pl.name;
        items.push_back({tr("action.add_to", {target}),
                         [id, target] {
                             const auto* p = Library::get().playlist(target);
                             if (!p) return;
                             auto v = p->tracks;
                             v.push_back(id);
                             Library::get().save_playlist(target, v, p->cover);
                         },
                         {}, &pl == &Library::get().playlists().front()});
        if (items.size() > 16) break;
    }
    append_mod_items(items, UI_PLACE_SONG, static_cast<uint64_t>(id));
    return items;
}

}  // namespace dp::ui
