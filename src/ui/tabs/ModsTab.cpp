#include "ui/tabs/ModsTab.hpp"

#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/switch.h>
#include <sigc++/adaptors/track_obj.h>

#include "core/Core.hpp"
#include "core/Mods.hpp"
#include "i18n/I18n.hpp"
#include "ui/Nav.hpp"
#include "ui/widgets/Common.hpp"
#include "util/Paths.hpp"

namespace dp::ui {

using core::Domain;

std::string ModsTab::title() const { return tr("page.mods"); }

ModsTab::ModsTab() {
    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_vexpand(true);
    auto* page = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    page->add_css_class("form-page");
    page->append(*banner(tr("mods.prealpha"), false));
    page->append(*make_label(tr("mods.intro", {paths::mods_dir().string()}), "muted", true));

    auto* bar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    auto* add = Gtk::make_managed<Gtk::Button>(tr("mods.add"));
    add->add_css_class("suggested-action");
    add->signal_clicked().connect([this] {
        if (auto* w = window_of(*this))
            pick_file(*w, tr("mods.add"), {"*.so"}, tr("dialog.libraries"),
                      [](const std::string& p) { Mods::get().add(p); });
    });
    bar->append(*add);
    auto* rescan = Gtk::make_managed<Gtk::Button>(tr("mods.rescan"));
    rescan->signal_clicked().connect([] { Mods::get().rescan(); });
    bar->append(*rescan);
    auto* start_all = Gtk::make_managed<Gtk::Button>(tr("mods.stop_all"));
    start_all->signal_clicked().connect([] {
        for (const auto& m : Mods::get().list())
            if (m.state != FFI_MOD_STOPPED) Mods::get().set_enabled(m.path, false);
    });
    bar->append(*start_all);
    page->append(*bar);

    page->append(list_);
    scroll->set_child(*page);
    append(*scroll);
    Mods::get().signal_changed().connect(sigc::track_obj([this] { defer([this] { rebuild(); }); }, *this));
    rebuild();
}

void ModsTab::rebuild() {
    while (auto* c = list_.get_first_child()) list_.remove(*c);
    const auto mods = Mods::get().list();
    if (mods.empty()) list_.append(*make_label(tr("mods.none"), "muted", true));
    for (const auto& m : mods) {
        auto* card = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
        card->add_css_class("card");

        auto* head = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
        auto* text = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
        text->set_hexpand(true);
        text->append(*make_label(m.name + "  " + m.version, "playlist-name"));
        std::string sub = m.author.empty() ? "" : tr("mods.by", {m.author});
        if (m.is_static) sub += (sub.empty() ? "" : " · ") + tr("mods.static");
        if (!sub.empty()) text->append(*make_label(sub, "muted"));
        head->append(*text);

        static const char* state_keys[] = {"mods.stopped", "mods.running", "mods.failed"};
        static const char* state_css[] = {"muted", "ok-text", "error-text"};
        const int si = std::min(static_cast<int>(m.state), 2);
        auto* state = make_label(tr(state_keys[si]), state_css[si]);
        head->append(*state);

        auto* sw = Gtk::make_managed<Gtk::Switch>();
        sw->set_active(m.state == FFI_MOD_RUNNING);
        sw->set_valign(Gtk::Align::CENTER);
        sw->set_tooltip_text(tr("mods.toggle_hint"));
        sw->property_active().signal_changed().connect([sw, path = m.path] {
            Mods::get().set_enabled(path, sw->get_active());
        });
        head->append(*sw);
        card->append(*head);

        if (!m.description.empty()) card->append(*make_label(m.description, {}, true));
        if (m.error) card->append(*banner(core::status_text(Domain::Mod, m.error) +
                                              (m.error_msg.empty() ? "" : ": " + m.error_msg),
                                          true));
        card->append(*make_label(m.path, "muted"));

        auto* actions = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
        if (m.state == FFI_MOD_RUNNING) {
            const uint32_t n = Mods::get().tab_count(m.path);
            for (uint32_t i = 0; i < n; ++i) {
                const ModTab* t = Mods::get().tab(m.path, i);
                auto* b = Gtk::make_managed<Gtk::Button>(t && t->title ? t->title : tr("mods.tab_n", {std::to_string(i + 1)}));
                b->signal_clicked().connect([path = m.path, i] { nav().open_mod_tab(path, i); });
                actions->append(*b);
            }
            for (const auto& a : Mods::get().actions(UI_PLACE_MENU)) {
                if (a.mod_path != m.path) continue;
                auto* b = Gtk::make_managed<Gtk::Button>(a.title);
                b->add_css_class("flat");
                b->signal_clicked().connect([a] { Mods::get().call(a, 0); });
                actions->append(*b);
            }
        }
        if (!m.is_static) {
            auto* spacer = Gtk::make_managed<Gtk::Box>();
            spacer->set_hexpand(true);
            actions->append(*spacer);
            auto* rm = Gtk::make_managed<Gtk::Button>(tr("mods.remove"));
            rm->add_css_class("destructive-action");
            rm->signal_clicked().connect([this, path = m.path, name = m.name] {
                if (auto* w = window_of(*this))
                    confirm(*w, tr("mods.confirm_remove", {name}), tr("mods.remove"),
                            [path] { Mods::get().remove(path); });
            });
            actions->append(*rm);
        }
        card->append(*actions);
        list_.append(*card);
    }
}

}  // namespace dp::ui
