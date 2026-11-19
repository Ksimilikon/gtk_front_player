#include "ui/tabs/EditorTab.hpp"

#include <gtkmm/adjustment.h>
#include <gtkmm/button.h>

#include <algorithm>
#include <filesystem>

#include "core/Covers.hpp"
#include "core/Library.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/Nav.hpp"
#include "ui/widgets/Dnd.hpp"
#include "ui/widgets/TrackRow.hpp"
#include "util/Format.hpp"
#include "util/Log.hpp"

namespace dp::ui {

std::string EditorTab::title() const {
    return original_.empty() ? "+" : tr("editor.title", {original_});
}

EditorTab::EditorTab(std::string name) : key_(key_for(name)), original_(std::move(name)) {
    if (const auto* p = Library::get().playlist(original_)) {
        tracks_ = original_tracks_ = p->tracks;
        cover_path_ = original_cover_ = p->cover;
    }

    // ── header: cover + name + buttons ──
    auto* head = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 14);
    head->add_css_class("page-header");
    auto* cover_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    cover_box->append(cover_);
    auto* cover_btns = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 2);
    auto* pick = icon_button("image-x-generic-symbolic", tr("editor.choose_cover"));
    pick->signal_clicked().connect([this] {
        if (auto* w = window_of(*this))
            pick_file(*w, tr("editor.choose_cover"), {"*.png", "*.jpg", "*.jpeg", "*.webp"}, tr("dialog.images"),
                      [this](const std::string& p) { set_cover(p); });
    });
    auto* clear = icon_button("edit-clear-symbolic", tr("editor.clear_cover"));
    clear->signal_clicked().connect([this] { set_cover(""); });
    cover_btns->append(*pick);
    cover_btns->append(*clear);
    cover_btns->set_halign(Gtk::Align::CENTER);
    cover_box->append(*cover_btns);
    head->append(*cover_box);

    auto* fields = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    fields->set_hexpand(true);
    fields->set_valign(Gtk::Align::CENTER);
    fields->append(*make_label(tr("editor.name"), "muted"));
    name_.set_text(original_);
    name_.set_placeholder_text(tr("editor.name_hint"));
    name_.signal_activate().connect([this] { save_changes(); });
    fields->append(name_);
    error_.add_css_class("error-text");
    error_.set_xalign(0);
    error_.set_visible(false);
    fields->append(error_);
    head->append(*fields);

    auto* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    buttons->set_valign(Gtk::Align::CENTER);
    auto* save_btn = Gtk::make_managed<Gtk::Button>(tr("common.save_ctrl_s"));
    save_btn->add_css_class("suggested-action");
    save_btn->signal_clicked().connect([this] { save_changes(); });
    buttons->append(*save_btn);
    auto* preview = Gtk::make_managed<Gtk::Button>(tr("editor.preview"));
    preview->set_tooltip_text(tr("editor.preview_hint"));
    preview->signal_clicked().connect([this] {
        std::vector<QueueItem> items;
        for (int64_t id : tracks_) items.push_back({id, {}});
        Player::get().play_queue(title(), items, 0, false);
    });
    buttons->append(*preview);
    if (!original_.empty()) {
        auto* del = Gtk::make_managed<Gtk::Button>(tr("action.delete"));
        del->add_css_class("destructive-action");
        del->signal_clicked().connect([this] {
            if (auto* w = window_of(*this))
                confirm(*w, tr("playlists.confirm_delete", {original_}), tr("action.delete"), [this] {
                    if (Library::get().remove_playlist(original_) == 0) {
                        deleted_ = true;
                        signal_close_request.emit();
                    }
                });
        });
        buttons->append(*del);
    }
    head->append(*buttons);
    append(*head);

    // ── two equal halves: the playlist | the pool ──
    auto* halves = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 0);
    halves->set_homogeneous(true);
    halves->set_vexpand(true);

    auto* left = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    left->add_css_class("half-pane");
    count_.add_css_class("section-title");
    count_.set_xalign(0);
    count_.set_margin_start(12);
    left->append(count_);
    auto* hint = make_label(tr("editor.drag_hint"), "muted", true);
    hint->set_margin_start(12);
    hint->set_margin_end(12);
    left->append(*hint);
    tracks_list_.add_css_class("plain-list");
    tracks_list_.set_selection_mode(Gtk::SelectionMode::NONE);
    tracks_scroll_.set_child(tracks_list_);
    tracks_scroll_.set_vexpand(true);
    // a drop below the last song (or into an empty playlist) appends.
    drop_on_area(tracks_scroll_, [this](const std::string& payload) {
        if (long id = payload_num(payload, "track:"); id > 0) {
            defer([this, id] { add(id); });
            return true;
        }
        if (long from = payload_num(payload, "row:"); from >= 0) {
            defer([this, from] { move(static_cast<size_t>(from), tracks_.size()); });
            return true;
        }
        return false;
    });
    left->append(tracks_scroll_);
    halves->append(*left);

    auto* right = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    right->add_css_class("half-pane");
    auto* lib_title = make_label(tr("editor.library"), "section-title");
    lib_title->set_margin_start(12);
    right->append(*lib_title);
    auto* tools = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    tools->set_margin_start(8);
    tools->set_margin_end(8);
    search_.set_placeholder_text(tr("playlist.search"));
    search_.set_hexpand(true);
    tools->append(search_);
    tools->append(sort_);
    desc_.set_icon_name("view-sort-descending-symbolic");
    desc_.set_tooltip_text(tr("sort.descending"));
    tools->append(desc_);
    right->append(*tools);
    hide_added_.set_label(tr("editor.hide_added"));
    hide_added_.set_active(true);
    hide_added_.set_margin_start(8);
    right->append(hide_added_);
    sort_note_.add_css_class("muted");
    sort_note_.set_xalign(0);
    sort_note_.set_wrap(true);
    sort_note_.set_margin_start(8);
    sort_note_.set_visible(false);
    right->append(sort_note_);

    pool_list_.add_css_class("plain-list");
    pool_list_.set_selection_mode(Gtk::SelectionMode::NONE);
    pool_list_.signal_row_activated().connect([this](Gtk::ListBoxRow* row) {
        if (auto* r = dynamic_cast<TrackRow*>(row)) add(r->track_id());
    });
    pool_list_.set_filter_func([this](Gtk::ListBoxRow* row) {
        auto* r = dynamic_cast<TrackRow*>(row);
        if (!r) return true;
        if (hide_added_.get_active() && in_playlist_.count(r->track_id())) return false;
        return fmt::contains_ci(r->search_text(), search_.get_text());
    });
    pool_list_.set_sort_func([this](Gtk::ListBoxRow* a, Gtk::ListBoxRow* b) {
        auto* ra = dynamic_cast<TrackRow*>(a);
        auto* rb = dynamic_cast<TrackRow*>(b);
        if (!ra || !rb) return 0;
        const int c = sort_.compare(ra->track_id(), rb->track_id());
        return desc_.get_active() ? -c : c;
    });
    search_.signal_search_changed().connect([this] { pool_list_.invalidate_filter(); });
    hide_added_.signal_toggled().connect([this] { pool_list_.invalidate_filter(); });
    auto resort = [this] {
        sort_note_.set_visible(sort_.uses_added_date());
        sort_note_.set_text(tr("sort.db_updated_note"));
        pool_list_.invalidate_sort();
    };
    sort_.property_selected().signal_changed().connect(resort);
    desc_.signal_toggled().connect(resort);
    pool_scroll_.set_child(pool_list_);
    pool_scroll_.set_vexpand(true);
    right->append(pool_scroll_);
    halves->append(*right);
    append(*halves);

    set_cover(cover_path_);
    tracks_changed();
    rebuild_pool();
    Library::get().signal_changed().connect(sigc::mem_fun(*this, &EditorTab::rebuild_pool));
}

bool EditorTab::has_unsaved() const {
    if (deleted_) return false;
    return fmt::trim(name_.get_text()) != original_ || tracks_ != original_tracks_ || cover_path_ != original_cover_;
}

void EditorTab::set_cover(const std::string& path) {
    cover_path_ = path;
    cover_.set_file_async(path, tracks_.empty() ? 0 : tracks_.front());
}

void EditorTab::tracks_changed() {
    in_playlist_ = {tracks_.begin(), tracks_.end()};
    rebuild_tracks();
    pool_list_.invalidate_filter();
    if (cover_path_.empty()) set_cover("");
}

void EditorTab::insert(int64_t id, size_t at) {
    tracks_.insert(tracks_.begin() + static_cast<long>(std::min(at, tracks_.size())), id);
    tracks_changed();
}

void EditorTab::move(size_t from, size_t to) {
    if (from >= tracks_.size()) return;
    to = std::min(to, tracks_.size());
    const int64_t id = tracks_[from];
    tracks_.erase(tracks_.begin() + static_cast<long>(from));
    if (to > from) --to;
    tracks_.insert(tracks_.begin() + static_cast<long>(to), id);
    tracks_changed();
}

void EditorTab::rebuild_tracks() {
    const double scroll = tracks_scroll_.get_vadjustment()->get_value();
    while (auto* c = tracks_list_.get_first_child()) tracks_list_.remove(*c);
    auto& lib = Library::get();
    count_.set_text(tr("editor.in_playlist", {std::to_string(tracks_.size()), fmt::duration(lib.total_duration(tracks_))}));
    for (size_t i = 0; i < tracks_.size(); ++i) {
        const auto* t = lib.track(tracks_[i]);
        if (!t) continue;
        auto* row = Gtk::make_managed<TrackRow>(*t, static_cast<int>(i + 1), false);
        auto* rm = icon_button("list-remove-symbolic", tr("editor.remove"));
        rm->signal_clicked().connect([this, i] {
            defer([this, i] {
                if (i < tracks_.size()) {
                    tracks_.erase(tracks_.begin() + static_cast<long>(i));
                    tracks_changed();
                }
            });
        });
        row->trailing().append(*rm);
        const int64_t id = tracks_[i];
        on_context_menu(*row, [this, i, id] {
            return std::vector<MenuItem>{
                {tr("editor.preview_song"), [id] { Player::get().preview(id); }},
                {tr("action.open_song"), [id] { nav().open_song(id); }},
                {tr("editor.remove"),
                 [this, i] {
                     if (i < tracks_.size()) {
                         tracks_.erase(tracks_.begin() + static_cast<long>(i));
                         tracks_changed();
                     }
                 },
                 "destructive", true},
            };
        });
        drag_source(*row, [i] { return "row:" + std::to_string(i); });
        drop_on_row(*row, [this, i](const std::string& payload, bool after) {
            const size_t to = i + (after ? 1 : 0);
            if (long from = payload_num(payload, "row:"); from >= 0) {
                defer([this, from, to] { move(static_cast<size_t>(from), to); });
                return true;
            }
            if (long id = payload_num(payload, "track:"); id > 0) {
                defer([this, id, to] { insert(id, to); });
                return true;
            }
            return false;
        });
        tracks_list_.append(*row);
    }
    defer([this, scroll] { tracks_scroll_.get_vadjustment()->set_value(scroll); });
}

void EditorTab::rebuild_pool() {
    sort_.clear_cache();
    while (auto* c = pool_list_.get_first_child()) pool_list_.remove(*c);
    for (const auto& t : Library::get().tracks()) {
        auto* row = Gtk::make_managed<TrackRow>(t, 0, false);
        auto* plus = icon_button("list-add-symbolic", tr("editor.add"));
        const int64_t id = t.id;
        plus->signal_clicked().connect([this, id] { defer([this, id] { add(id); }); });
        row->trailing().append(*plus);
        on_context_menu(*row, [this, id] {
            return std::vector<MenuItem>{
                {tr("editor.preview_song"), [id] { Player::get().preview(id); }},
                {tr("editor.add"), [this, id] { add(id); }},
                {tr("action.open_song"), [id] { nav().open_song(id); }},
            };
        });
        drag_source(*row, [id] { return "track:" + std::to_string(id); });
        pool_list_.append(*row);
    }
}

bool EditorTab::save_changes() {
    const std::string name = fmt::trim(name_.get_text());
    auto fail = [this](const std::string& msg) {
        error_.set_text(msg);
        error_.set_visible(true);
        return false;
    };
    if (name.empty()) return fail(tr("editor.err.empty_name"));
    if (name != original_ && Library::get().playlist(name)) return fail(tr("editor.err.exists", {name}));
    error_.set_visible(false);
    auto& lib = Library::get();
    if (lib.save_playlist(name, tracks_, cover_path_) != 0) return fail(tr("editor.err.save_failed"));
    if (!original_.empty() && name != original_) lib.remove_playlist(original_);
    Log::get().ok("ui", tr("editor.saved", {name}));
    original_ = name;
    original_tracks_ = tracks_;
    original_cover_ = cover_path_;
    signal_title_changed.emit();
    if (key_ == key_for("")) {
        // a new playlist is created: continue in its own editor, so "new
        // playlist" opens an empty one again.
        defer([this, name] {
            signal_close_request.emit();
            nav().open_editor(name);
        });
    }
    return true;
}

}  // namespace dp::ui
