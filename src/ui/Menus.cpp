#include "ui/Menus.hpp"

#include <gtkmm/button.h>
#include <gtkmm/separator.h>

#include "core/Library.hpp"
#include "core/Mods.hpp"
#include "core/Player.hpp"
#include "ui/Dialogs.hpp"
#include "util/Log.hpp"
#include "i18n/I18n.hpp"
#include "ui/Nav.hpp"

namespace dp::ui {

MainMenuButton::MainMenuButton() {
    set_icon_name("open-menu-symbolic");
    set_tooltip_text(tr("menu.tooltip"));
    add_css_class("flat");
    add_css_class("main-menu");
    set_focusable(false);
    popover_.add_css_class("context-menu");
    popover_.set_has_arrow(false);
    set_popover(popover_);
    autohide_on_leave(popover_);
    // rebuilt every time it opens.
    popover_.signal_show().connect(sigc::mem_fun(*this, &MainMenuButton::rebuild));
    rebuild();
}

void MainMenuButton::rebuild() {
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    auto add = [this, box](const std::string& label, const std::string& icon, std::function<void()> fn) {
        auto* b = Gtk::make_managed<Gtk::Button>();
        auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
        if (!icon.empty()) {
            auto* img = Gtk::make_managed<Gtk::Image>();
            img->set_from_icon_name(icon);
            row->append(*img);
        }
        row->append(*make_label(label));
        b->set_child(*row);
        b->add_css_class("flat");
        b->add_css_class("menu-item");
        b->signal_clicked().connect([this, fn] {
            popover_.popdown();
            fn();
        });
        box->append(*b);
    };
    add(tr("page.playlists"), "view-list-symbolic", [] { nav().open_playlists(); });
    add(tr("page.new_playlist"), "list-add-symbolic", [] { nav().open_editor(""); });
    box->append(*Gtk::make_managed<Gtk::Separator>());
    add(tr("page.sync"), "network-transmit-receive-symbolic", [] { nav().open_page("sync"); });
    add(tr("page.import"), "document-open-symbolic", [] { nav().open_page("import"); });
    add(tr("page.export"), "document-save-symbolic", [] { nav().open_page("export"); });
    box->append(*Gtk::make_managed<Gtk::Separator>());
    add(tr("page.settings"), "emblem-system-symbolic", [] { nav().open_page("settings"); });
    add(tr("page.mods"), "application-x-addon-symbolic", [] { nav().open_page("mods"); });
    add(tr("page.logs"), "utilities-terminal-symbolic", [] { nav().open_page("logs"); });
    add(tr("page.about"), "help-about-symbolic", [] { nav().open_page("about"); });

    // actions and tabs of running mods.
    const auto actions = Mods::get().actions(UI_PLACE_MENU);
    std::vector<std::pair<std::string, std::pair<std::string, uint32_t>>> tabs;
    for (const auto& m : Mods::get().list()) {
        if (m.state != FFI_MOD_RUNNING) continue;
        const uint32_t n = Mods::get().tab_count(m.path);
        for (uint32_t i = 0; i < n; ++i) {
            const ModTab* t = Mods::get().tab(m.path, i);
            const std::string title = t && t->title ? t->title : m.name;
            tabs.push_back({title, {m.path, i}});
        }
    }
    if (!actions.empty() || !tabs.empty()) {
        box->append(*Gtk::make_managed<Gtk::Separator>());
        auto* head = make_label(tr("menu.mods_section"), "menu-section");
        box->append(*head);
        for (const auto& [title, ref] : tabs)
            add(title, "tab-new-symbolic", [ref] { nav().open_mod_tab(ref.first, ref.second); });
        for (const auto& a : actions) add(a.title, "system-run-symbolic", [a] { Mods::get().call(a, 0); });
    }
    popover_.set_child(*box);
}

void append_mod_items(std::vector<MenuItem>& items, uint32_t place, uint64_t target) {
    bool first = true;
    for (const auto& a : Mods::get().actions(place)) {
        MenuItem it{a.title, [a, target] { Mods::get().call(a, target); }, "mod-item", first};
        items.push_back(std::move(it));
        first = false;
    }
}

}  // namespace dp::ui

namespace dp::ui {

void add_to_playlist(Gtk::Window& parent, const std::vector<int64_t>& ids) {
    pick_playlist(parent, tr("action.add_to_title"), [ids](const std::string& name) {
        const auto* p = Library::get().playlist(name);
        std::vector<int64_t> v = p ? p->tracks : std::vector<int64_t>{};
        v.insert(v.end(), ids.begin(), ids.end());
        if (Library::get().save_playlist(name, v, p ? p->cover : "") == 0)
            Log::get().ok("ui", tr("action.added_to", {name}));
    });
}

void append_song_items(std::vector<MenuItem>& items, Gtk::Widget& anchor, int64_t id) {
    Gtk::Window* win = window_of(anchor);
    items.push_back({tr("action.play_next"), [id] { Player::get().play_next({id, {}}); }});
    items.push_back({tr("action.open_song"), [id] { nav().open_song(id); }});
    items.push_back({tr("action.add_to"), [win, id] {
                         if (win) add_to_playlist(*win, {id});
                     }});
    items.push_back({tr("action.export_song"), [id] { nav().open_export_tracks({id}); }});
    append_mod_items(items, UI_PLACE_SONG, static_cast<uint64_t>(id));
}

void append_queue_items(std::vector<MenuItem>& items, std::function<std::vector<int64_t>()> ids,
                        const std::string& name) {
    items.push_back({tr("action.queue_next"),
                     [ids, name] { Player::get().enqueue(ids(), Player::Where::AfterCurrent, name); }, {}, true});
    items.push_back({tr("action.queue_end"), [ids, name] { Player::get().enqueue(ids(), Player::Where::AtEnd, name); }});
}

}  // namespace dp::ui
