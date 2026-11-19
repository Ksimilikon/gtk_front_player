#include "ui/tabs/PlaylistTab.hpp"

#include <gtkmm/adjustment.h>
#include <gtkmm/button.h>
#include <gtkmm/menubutton.h>

#include <algorithm>

#include "core/Library.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/Menus.hpp"
#include "ui/Nav.hpp"
#include "ui/widgets/Dnd.hpp"
#include "ui/widgets/TrackRow.hpp"
#include "util/Format.hpp"

namespace dp::ui {

std::string PlaylistTab::title() const { return name_.empty() ? tr("playlists.all_songs") : name_; }

PlaylistTab::PlaylistTab(std::string name) : name_(std::move(name)) {
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
        nav().play_playlist(name_, ids(), 0);
    });
    actions->append(*play);
    auto* shuffle = Gtk::make_managed<Gtk::Button>(tr("action.shuffle_play"));
    shuffle->signal_clicked().connect([this] {
        const auto v = ids();
        if (v.empty()) return;
        Player::get().set_shuffle(true);
        nav().play_playlist(name_, v, static_cast<size_t>(g_random_int_range(0, static_cast<int>(v.size()))));
    });
    actions->append(*shuffle);
    auto* queue = Gtk::make_managed<Gtk::Button>(tr("action.to_queue"));
    queue->set_tooltip_text(tr("action.to_queue_hint"));
    queue->signal_clicked().connect([this, queue] {
        std::vector<MenuItem> items;
        append_queue_items(items, [this] { return ids(); }, title());
        items.front().separator = false;
        show_menu(*queue, 0, queue->get_height(), items);
    });
    actions->append(*queue);
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
            nav().play_playlist(name_, ids(), static_cast<size_t>(r->number() - 1));
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

void PlaylistTab::move_track(size_t from, size_t to) {
    const auto* p = Library::get().playlist(name_);
    if (!p || from >= p->tracks.size() || to > p->tracks.size() || from == to) return;
    auto v = p->tracks;
    const int64_t id = v[from];
    v.erase(v.begin() + static_cast<long>(from));
    if (to > from) --to;
    v.insert(v.begin() + static_cast<long>(to), id);
    const std::string cover = p->cover;
    // after the drop finishes: saving rebuilds the rows being dragged.
    defer([this, v, cover] { Library::get().save_playlist(name_, v, cover); });
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
    // no own cover — the first song's.
    cover_.set_file_async(name_.empty() ? "" : lib.playlist(name_)->cover, v.empty() ? 0 : v.front());

    const double scroll = scroll_.get_vadjustment()->get_value();
    while (auto* child = list_.get_first_child()) list_.remove(*child);
    int n = 0;
    for (int64_t id : v) {
        ++n;
        const auto* t = lib.track(id);
        if (!t) continue;
        auto* row = Gtk::make_managed<TrackRow>(*t, n);
        const size_t index = static_cast<size_t>(n - 1);
        on_context_menu(*row, [this, row, id, index] { return track_menu(*row, id, index); });
        if (!name_.empty()) {
            // reorder by dragging; songs of other lists can be dropped in too.
            drag_source(*row, [index] { return "row:" + std::to_string(index); });
            drop_on_row(*row, [this, index](const std::string& payload, bool after) {
                const size_t to = index + (after ? 1 : 0);
                if (long from = payload_num(payload, "row:"); from >= 0) {
                    move_track(static_cast<size_t>(from), to);
                    return true;
                }
                if (long id = payload_num(payload, "track:"); id > 0) {
                    const auto* p = Library::get().playlist(name_);
                    if (!p) return false;
                    auto v2 = p->tracks;
                    v2.insert(v2.begin() + static_cast<long>(std::min(to, v2.size())), id);
                    const std::string cover = p->cover;
                    defer([this, v2, cover] { Library::get().save_playlist(name_, v2, cover); });
                    return true;
                }
                return false;
            });
        }
        list_.append(*row);
    }
    // keep the scroll position across rebuilds (e.g. after a reorder).
    defer([this, scroll] { scroll_.get_vadjustment()->set_value(scroll); });
}

std::vector<MenuItem> PlaylistTab::track_menu(Gtk::Widget& row, int64_t id, size_t index) {
    std::vector<MenuItem> items;
    items.push_back({tr("action.play"), [this, index] { nav().play_playlist(name_, ids(), index); }});
    append_song_items(items, row, id);
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
    return items;
}

}  // namespace dp::ui
