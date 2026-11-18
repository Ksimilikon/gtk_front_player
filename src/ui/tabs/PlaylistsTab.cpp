#include "ui/tabs/PlaylistsTab.hpp"

#include <gtkmm/button.h>
#include <gtkmm/listboxrow.h>

#include "core/Covers.hpp"
#include "core/Library.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/Menus.hpp"
#include "ui/Nav.hpp"
#include "util/Format.hpp"
#include "util/Settings.hpp"

namespace dp::ui {

namespace {
/// a playlist row; `name` empty — the whole library.
class PlaylistRow : public Gtk::ListBoxRow {
public:
    std::string name;
    std::string search;
};
}  // namespace

std::string PlaylistsTab::title() const { return tr("page.playlists"); }

PlaylistsTab::PlaylistsTab() {
    auto* bar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    bar->add_css_class("toolbar");
    search_.set_placeholder_text(tr("playlists.search"));
    search_.set_hexpand(true);
    bar->append(search_);
    auto* reindex = icon_button("view-refresh-symbolic", tr("playlists.reindex"));
    reindex->signal_clicked().connect([] { Library::get().index_dir_async(Settings::get().str(key::music_dir)); });
    bar->append(*reindex);
    auto* add = Gtk::make_managed<Gtk::Button>("+");
    add->set_tooltip_text(tr("playlists.new"));
    add->add_css_class("plus-button");
    add->add_css_class("suggested-action");
    add->signal_clicked().connect([] { nav().open_editor(""); });
    bar->append(*add);
    append(*bar);

    list_.add_css_class("plain-list");
    list_.set_selection_mode(Gtk::SelectionMode::NONE);
    list_.set_activate_on_single_click(true);
    list_.signal_row_activated().connect([](Gtk::ListBoxRow* row) {
        if (auto* r = dynamic_cast<PlaylistRow*>(row)) nav().open_playlist(r->name);
    });
    list_.set_filter_func([this](Gtk::ListBoxRow* row) {
        auto* r = dynamic_cast<PlaylistRow*>(row);
        return !r || fmt::contains_ci(r->search, search_.get_text());
    });
    search_.signal_search_changed().connect([this] { list_.invalidate_filter(); });
    scroll_.set_child(list_);
    scroll_.set_vexpand(true);
    append(scroll_);

    empty_.set_text(tr("playlists.empty"));
    empty_.add_css_class("muted");
    empty_.set_margin(24);
    empty_.set_wrap(true);
    append(empty_);

    Library::get().signal_changed().connect(sigc::mem_fun(*this, &PlaylistsTab::rebuild));
    Player::get().signal_track_changed().connect(sigc::mem_fun(*this, &PlaylistsTab::rebuild));
    rebuild();
}

void PlaylistsTab::rebuild() {
    while (auto* child = list_.get_first_child()) list_.remove(*child);
    auto& lib = Library::get();
    const std::string playing_queue = Player::get().state().has_track ? Player::get().queue_name() : "";

    auto add_row = [&](const std::string& name, const std::string& display, size_t count, uint64_t dur,
                       const std::string& cover, int64_t first_track) {
        auto* row = Gtk::make_managed<PlaylistRow>();
        row->name = name;
        row->search = display;
        row->add_css_class("playlist-row");
        auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 14);
        auto* cv = Gtk::make_managed<CoverView>(48);
        // no own cover — the first song's.
        cv->set_file_async(cover, first_track);
        box->append(*cv);
        auto* text = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
        text->set_valign(Gtk::Align::CENTER);
        text->set_hexpand(true);
        auto* title = make_label(display, "playlist-name");
        text->append(*title);
        text->append(*make_label(tr("playlists.stats", {std::to_string(count), fmt::duration(dur)}), "muted"));
        box->append(*text);
        if (!playing_queue.empty() && playing_queue == display) {
            row->add_css_class("playing");
            auto* mark = Gtk::make_managed<Gtk::Image>();
            mark->set_from_icon_name("media-playback-start-symbolic");
            mark->add_css_class("accent");
            box->append(*mark);
        }
        row->set_child(*box);
        on_context_menu(*row, [this, name, display] {
            std::vector<MenuItem> items;
            auto ids = [name] {
                if (name.empty()) {
                    std::vector<int64_t> out;
                    for (const auto& t : Library::get().tracks()) out.push_back(t.id);
                    return out;
                }
                const auto* p = Library::get().playlist(name);
                return p ? p->tracks : std::vector<int64_t>{};
            };
            items.push_back({tr("action.play"), [ids, display] { Player::get().play_ids(display, ids(), 0); }});
            items.push_back({tr("action.open"), [name] { nav().open_playlist(name); }});
            append_queue_items(items, ids, display);
            if (!name.empty()) {
                items.push_back({tr("action.edit"), [name] { nav().open_editor(name); }});
                items.push_back({tr("action.export"), [name] { nav().open_export_playlists({name}); }});
                items.push_back({tr("action.delete"),
                                 [this, name] {
                                     if (auto* w = window_of(*this))
                                         confirm(*w, tr("playlists.confirm_delete", {name}), tr("action.delete"),
                                                 [name] { Library::get().remove_playlist(name); });
                                 },
                                 "destructive", true});
                append_mod_items(items, UI_PLACE_PLAYLIST, 0);
            }
            return items;
        });
        list_.append(*row);
    };

    std::vector<int64_t> all;
    all.reserve(lib.tracks().size());
    for (const auto& t : lib.tracks()) all.push_back(t.id);
    add_row("", tr("playlists.all_songs"), all.size(), lib.total_duration(all), "", 0);
    for (const auto& p : lib.playlists())
        add_row(p.name, p.name, p.tracks.size(), lib.total_duration(p.tracks), p.cover,
                p.tracks.empty() ? 0 : p.tracks.front());
    empty_.set_visible(lib.playlists().empty());
}

}  // namespace dp::ui
