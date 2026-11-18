#include "ui/tabs/EditorTab.hpp"

#include <gtkmm/button.h>
#include <gtkmm/paned.h>

#include <algorithm>

#include "core/Covers.hpp"
#include "core/Library.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/Nav.hpp"
#include "ui/widgets/TrackRow.hpp"
#include "util/Format.hpp"
#include "util/Log.hpp"

namespace dp::ui {

std::string EditorTab::title() const {
    return original_.empty() ? tr("editor.new_title") : tr("editor.title", {original_});
}

EditorTab::EditorTab(std::string name) : original_(std::move(name)) {
    if (const auto* p = Library::get().playlist(original_)) {
        tracks_ = p->tracks;
        cover_path_ = p->cover;
    }

    // header: cover + name + buttons.
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
    name_.signal_activate().connect(sigc::mem_fun(*this, &EditorTab::save));
    fields->append(name_);
    error_.add_css_class("error-text");
    error_.set_xalign(0);
    error_.set_visible(false);
    fields->append(error_);
    head->append(*fields);

    auto* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    buttons->set_valign(Gtk::Align::CENTER);
    auto* save_btn = Gtk::make_managed<Gtk::Button>(tr("common.save"));
    save_btn->add_css_class("suggested-action");
    save_btn->signal_clicked().connect(sigc::mem_fun(*this, &EditorTab::save));
    buttons->append(*save_btn);
    auto* preview = Gtk::make_managed<Gtk::Button>(tr("editor.preview"));
    preview->set_tooltip_text(tr("editor.preview_hint"));
    preview->signal_clicked().connect([this] { Player::get().play_ids(title(), tracks_, 0); });
    buttons->append(*preview);
    if (!original_.empty()) {
        auto* del = Gtk::make_managed<Gtk::Button>(tr("action.delete"));
        del->add_css_class("destructive-action");
        del->signal_clicked().connect([this] {
            if (auto* w = window_of(*this))
                confirm(*w, tr("playlists.confirm_delete", {original_}), tr("action.delete"), [this] {
                    if (Library::get().remove_playlist(original_) == 0) signal_close_request.emit();
                });
        });
        buttons->append(*del);
    }
    head->append(*buttons);
    append(*head);

    // two panes: the playlist | the library.
    auto* paned = Gtk::make_managed<Gtk::Paned>(Gtk::Orientation::HORIZONTAL);
    paned->set_vexpand(true);

    auto* left = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    count_.add_css_class("section-title");
    count_.set_xalign(0);
    count_.set_margin_start(12);
    left->append(count_);
    tracks_list_.add_css_class("plain-list");
    tracks_list_.set_selection_mode(Gtk::SelectionMode::NONE);
    tracks_scroll_.set_child(tracks_list_);
    tracks_scroll_.set_vexpand(true);
    left->append(tracks_scroll_);
    paned->set_start_child(*left);

    auto* right = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    auto* lib_title = make_label(tr("editor.library"), "section-title");
    lib_title->set_margin_start(12);
    right->append(*lib_title);
    search_.set_placeholder_text(tr("playlist.search"));
    search_.set_margin_start(8);
    search_.set_margin_end(8);
    right->append(search_);
    library_list_.add_css_class("plain-list");
    library_list_.set_selection_mode(Gtk::SelectionMode::NONE);
    library_list_.signal_row_activated().connect([this](Gtk::ListBoxRow* row) {
        if (auto* r = dynamic_cast<TrackRow*>(row)) add(r->track_id());
    });
    library_list_.set_filter_func([this](Gtk::ListBoxRow* row) {
        auto* r = dynamic_cast<TrackRow*>(row);
        return !r || fmt::contains_ci(r->search_text(), search_.get_text());
    });
    search_.signal_search_changed().connect([this] { library_list_.invalidate_filter(); });
    library_scroll_.set_child(library_list_);
    library_scroll_.set_vexpand(true);
    right->append(library_scroll_);
    paned->set_end_child(*right);
    paned->set_position(520);
    append(*paned);

    set_cover(cover_path_);
    rebuild_tracks();
    rebuild_library();
    Library::get().signal_changed().connect(sigc::mem_fun(*this, &EditorTab::rebuild_library));
}

void EditorTab::set_cover(const std::string& path) {
    cover_path_ = path;
    cover_.set(Covers::get().file(path));
}

void EditorTab::add(int64_t id) {
    tracks_.push_back(id);
    rebuild_tracks();
}

void EditorTab::rebuild_tracks() {
    while (auto* c = tracks_list_.get_first_child()) tracks_list_.remove(*c);
    auto& lib = Library::get();
    count_.set_text(tr("editor.in_playlist", {std::to_string(tracks_.size()), fmt::duration(lib.total_duration(tracks_))}));
    for (size_t i = 0; i < tracks_.size(); ++i) {
        const auto* t = lib.track(tracks_[i]);
        if (!t) continue;
        auto* row = Gtk::make_managed<TrackRow>(*t, static_cast<int>(i + 1), false);
        auto* up = icon_button("go-up-symbolic", tr("editor.move_up"));
        auto* down = icon_button("go-down-symbolic", tr("editor.move_down"));
        auto* rm = icon_button("list-remove-symbolic", tr("editor.remove"));
        up->set_sensitive(i > 0);
        down->set_sensitive(i + 1 < tracks_.size());
        up->signal_clicked().connect([this, i] {
            std::swap(tracks_[i], tracks_[i - 1]);
            defer([this] { rebuild_tracks(); });
        });
        down->signal_clicked().connect([this, i] {
            std::swap(tracks_[i], tracks_[i + 1]);
            defer([this] { rebuild_tracks(); });
        });
        rm->signal_clicked().connect([this, i] {
            tracks_.erase(tracks_.begin() + static_cast<long>(i));
            defer([this] { rebuild_tracks(); });
        });
        row->trailing().append(*up);
        row->trailing().append(*down);
        row->trailing().append(*rm);
        tracks_list_.append(*row);
    }
}

void EditorTab::rebuild_library() {
    while (auto* c = library_list_.get_first_child()) library_list_.remove(*c);
    for (const auto& t : Library::get().tracks()) {
        auto* row = Gtk::make_managed<TrackRow>(t, 0, false);
        auto* plus = icon_button("list-add-symbolic", tr("editor.add"));
        const int64_t id = t.id;
        plus->signal_clicked().connect([this, id] { add(id); });
        row->trailing().append(*plus);
        library_list_.append(*row);
    }
}

void EditorTab::save() {
    const std::string name = fmt::trim(name_.get_text());
    auto fail = [this](const std::string& msg) {
        error_.set_text(msg);
        error_.set_visible(true);
    };
    if (name.empty()) return fail(tr("editor.err.empty_name"));
    if (name != original_ && Library::get().playlist(name)) return fail(tr("editor.err.exists", {name}));
    error_.set_visible(false);
    auto& lib = Library::get();
    if (lib.save_playlist(name, tracks_, cover_path_) != 0) return fail(tr("editor.err.save_failed"));
    if (!original_.empty() && name != original_) lib.remove_playlist(original_);
    Log::get().ok("ui", tr("editor.saved", {name}));
    const std::string target = name;
    signal_close_request.emit();
    nav().open_playlist(target);
}

}  // namespace dp::ui
