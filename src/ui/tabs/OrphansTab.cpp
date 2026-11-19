#include "ui/tabs/OrphansTab.hpp"

#include <gtkmm/scrolledwindow.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include "core/Library.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/Menus.hpp"
#include "ui/widgets/TrackRow.hpp"
#include "util/Format.hpp"
#include "util/Log.hpp"

namespace dp::ui {

static std::vector<core::Track> orphan_tracks(uint32_t filter) {
    FFITracksList list{};
    std::vector<core::Track> result;
    if (auto status = idx_get_orphan_tracks(filter, &list)) {
        Log::get().error("core", core::status_text(core::Domain::Idx, status));
        return result;
    }
    result.reserve(list.len);
    for (size_t i = 0; i < list.len; ++i) result.push_back(core::to_track(list.ptr[i]));
    idx_free_tracks_list(list);
    return result;
}

std::string OrphansTab::title() const { return tr("page.orphans"); }

OrphansTab::OrphansTab()
    : filter_(std::vector<Glib::ustring>{tr("orphans.unmarked"), tr("orphans.marked"), tr("orphans.all")}),
      play_(tr("action.play")), remove_(tr("orphans.delete_shown")) {
    auto* header = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    header->add_css_class("page-header");
    header->append(*make_label(title(), "page-title"));
    stats_.set_hexpand(true);
    stats_.set_xalign(0);
    header->append(stats_);
    play_.add_css_class("suggested-action");
    play_.signal_clicked().connect([this] { Player::get().play_ids("", shown_, 0); });
    header->append(play_);
    remove_.add_css_class("destructive-action");
    remove_.signal_clicked().connect([this] { ask_delete(shown_); });
    header->append(remove_);
    append(*header);

    auto* tools = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    search_.set_placeholder_text(tr("playlist.search"));
    search_.set_hexpand(true);
    tools->append(search_);
    tools->append(filter_);
    tools->append(sort_);
    descending_.set_icon_name("view-sort-descending-symbolic");
    descending_.set_tooltip_text(tr("sort.descending"));
    tools->append(descending_);
    append(*tools);
    note_.set_text(tr("sort.db_updated_note"));
    note_.add_css_class("muted");
    append(note_);

    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    list_.add_css_class("plain-list");
    list_.set_selection_mode(Gtk::SelectionMode::NONE);
    scroll->set_child(list_);
    scroll->set_vexpand(true);
    append(*scroll);
    list_.signal_row_activated().connect([this](Gtk::ListBoxRow* row) {
        if (auto* track = dynamic_cast<TrackRow*>(row)) play(track->track_id());
    });
    filter_.property_selected().signal_changed().connect(sigc::mem_fun(*this, &OrphansTab::reload));
    sort_.property_selected().signal_changed().connect(sigc::mem_fun(*this, &OrphansTab::rebuild));
    descending_.signal_toggled().connect(sigc::mem_fun(*this, &OrphansTab::rebuild));
    search_.property_text().signal_changed().connect(sigc::mem_fun(*this, &OrphansTab::rebuild));
    Library::get().signal_changed().connect(sigc::mem_fun(*this, &OrphansTab::reload));
    reload();
}

void OrphansTab::reload() {
    sort_.clear_cache();
    candidates_.clear();
    for (const auto& track : orphan_tracks(filter_.get_selected())) candidates_.push_back(track.id);
    rebuild();
}

void OrphansTab::rebuild() {
    while (auto* child = list_.get_first_child()) list_.remove(*child);
    shown_.clear();
    auto& lib = Library::get();
    for (auto id : candidates_) {
        const auto* t = lib.track(id);
        if (t && fmt::contains_ci(TrackRow::search_text_for(*t), search_.get_text())) shown_.push_back(id);
    }
    std::stable_sort(shown_.begin(), shown_.end(), [this](auto a, auto b) {
        int order = sort_.compare(a, b);
        return descending_.get_active() ? order > 0 : order < 0;
    });
    for (size_t i = 0; i < shown_.size(); ++i) {
        auto id = shown_[i];
        const auto& track = *lib.track(id);
        auto* row = Gtk::make_managed<TrackRow>(track, static_cast<int>(i + 1));
        if (track.ignore_orphan) row->trailing().append(*make_label(tr("orphans.keep_mark"), "muted"));
        on_context_menu(*row, [this, row, id, kept = track.ignore_orphan] {
            std::vector<MenuItem> items{{tr("action.play"), [this, id] { play(id); }}};
            append_song_items(items, *row, id);
            items.push_back({tr(kept ? "orphans.unmark" : "orphans.mark"), [id, kept] {
                if (auto st = idx_set_track_ignore_orphan(id, !kept))
                    Log::get().error("core", core::status_text(core::Domain::Idx, st));
                else Library::get().reload();
            }});
            items.push_back({tr("orphans.delete_one"), [this, id] { ask_delete({id}); }, "destructive", true});
            return items;
        });
        list_.append(*row);
    }
    note_.set_visible(sort_.uses_added_date());
    stats_.set_text(tr("playlists.stats", {std::to_string(shown_.size()), fmt::duration(lib.total_duration(shown_))}));
    play_.set_sensitive(!shown_.empty());
    remove_.set_sensitive(!shown_.empty());
}

void OrphansTab::play(int64_t id) {
    auto pos = std::find(shown_.begin(), shown_.end(), id);
    if (pos != shown_.end()) Player::get().play_ids("", shown_, pos - shown_.begin());
}

void OrphansTab::ask_delete(const std::vector<int64_t>& ids) {
    auto* win = window_of(*this);
    if (!win || ids.empty()) return;
    std::unordered_map<int64_t, std::string> paths;
    for (auto id : ids)
        if (const auto* t = Library::get().track(id)) paths.emplace(id, t->path);
    if (paths.empty()) return;
    auto message = paths.size() == 1 ? tr("orphans.confirm_one", {paths.begin()->second})
                                     : tr("orphans.confirm_many", {std::to_string(paths.size())});
    confirm(*win, message, tr("action.delete"), [paths = std::move(paths), filter = filter_.get_selected()] {
        auto& player = Player::get();
        std::unordered_set<int64_t> removed;
        // Recheck membership, mark and path after the confirmation dialog.
        for (const auto& t : orphan_tracks(filter)) {
            const auto wanted = paths.find(t.id);
            if (wanted == paths.end() || wanted->second != t.path) continue;
            if (!t.path.empty() && unlink(t.path.c_str()) != 0 && errno != ENOENT) {
                Log::get().error("library", t.path + ": " + std::error_code(errno, std::generic_category()).message());
                continue;
            }
            if (auto st = idx_remove_track(t.id)) {
                Log::get().error("library", t.path + ": " + core::status_text(core::Domain::Idx, st));
                continue;
            }
            removed.insert(t.id);
        }
        if (auto current = player.current(); current && removed.contains(current->id)) player.stop();
        for (size_t i = player.entries().size(); i > 0; --i)
            if (removed.contains(player.entries()[i - 1].item.id)) player.remove_at(i - 1);
        Library::get().reload();
    });
}

}  // namespace dp::ui
