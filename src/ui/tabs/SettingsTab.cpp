#include "ui/tabs/SettingsTab.hpp"

#include <giomm/file.h>
#include <gtkmm/alertdialog.h>
#include <gtkmm/button.h>
#include <gtkmm/filelauncher.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/colordialog.h>
#include <gtkmm/colordialogbutton.h>
#include <gtkmm/eventcontrollerfocus.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/stringlist.h>
#include <gtkmm/switch.h>
#include <sigc++/adaptors/track_obj.h>

#include "app/Application.hpp"
#include "core/Core.hpp"
#include "core/Player.hpp"
#include "core/Library.hpp"
#include "i18n/I18n.hpp"
#include "theme/Theme.hpp"
#include "ui/Hotkeys.hpp"
#include "ui/widgets/Common.hpp"
#include "util/Log.hpp"
#include "util/Paths.hpp"
#include "util/Settings.hpp"

namespace dp::ui {

using core::Domain;

std::string SettingsTab::title() const { return tr("page.settings"); }

static Gtk::Box* setting_row(const std::string& name, const std::string& hint, Gtk::Widget& control) {
    auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 16);
    row->add_css_class("setting-row");
    auto* text = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
    text->set_hexpand(true);
    text->append(*make_label(name));
    if (!hint.empty()) text->append(*make_label(hint, "muted", true));
    row->append(*text);
    control.set_valign(Gtk::Align::CENTER);
    row->append(control);
    return row;
}

static void open_folder(Gtk::Widget& w, const std::string& path) {
    auto launcher = Gtk::FileLauncher::create(Gio::File::create_for_path(path));
    launcher->launch(*window_of(w), [launcher](const Glib::RefPtr<Gio::AsyncResult>& r) {
        try {
            launcher->launch_finish(r);
        } catch (const Glib::Error& e) {
            Log::get().warn("ui", e.what());
        }
    });
}

SettingsTab::SettingsTab() {
    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_vexpand(true);
    auto* page = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    page->add_css_class("form-page");

    // ── appearance ──
    page->append(*section_title(tr("settings.appearance")));
    {
        const auto langs = I18n::get().available();
        std::vector<Glib::ustring> names{tr("settings.language_auto")};
        guint selected = 0;
        const std::string current = Settings::get().str(key::language, "auto");
        for (size_t i = 0; i < langs.size(); ++i) {
            names.push_back(langs[i].name);
            if (langs[i].code == current) selected = static_cast<guint>(i + 1);
        }
        auto* dd = Gtk::make_managed<Gtk::DropDown>(names);
        dd->set_selected(selected);
        auto* note = banner(tr("settings.restart_needed"), false);
        note->set_visible(false);
        auto* restart = Gtk::make_managed<Gtk::Button>(tr("settings.restart_now"));
        restart->signal_clicked().connect([] { request_restart(); });
        note->append(*restart);
        dd->property_selected().signal_changed().connect([this, dd, langs, note] {
            const guint i = dd->get_selected();
            Settings::get().set(key::language, i == 0 ? "auto" : langs[i - 1].code);
            note->set_visible(true);
            Log::get().warn("ui", tr("settings.restart_needed"));
            if (auto* w = window_of(*this)) {
                auto d = Gtk::AlertDialog::create(tr("settings.restart_needed"));
                d->set_detail(tr("settings.restart_detail"));
                d->set_buttons({tr("settings.restart_later"), tr("settings.restart_now")});
                d->set_cancel_button(0);
                d->set_default_button(1);
                d->choose(*w, [d](const Glib::RefPtr<Gio::AsyncResult>& r) {
                    try {
                        if (d->choose_finish(r) == 1) request_restart();
                    } catch (const Glib::Error&) {
                    }
                });
            }
        });
        page->append(*setting_row(tr("settings.language"), tr("settings.language_hint"), *dd));
        page->append(*note);
    }
    {
        const auto& fonts = Theme::fonts();
        std::vector<Glib::ustring> names;
        guint selected = 0;
        for (size_t i = 0; i < fonts.size(); ++i) {
            names.push_back(tr(fonts[i].name_key));
            if (fonts[i].id == Theme::get().font()) selected = static_cast<guint>(i);
        }
        auto* dd = Gtk::make_managed<Gtk::DropDown>(names);
        dd->set_selected(selected);
        dd->property_selected().signal_changed().connect(
            [dd] { Theme::get().set_font(Theme::fonts()[dd->get_selected()].id); });
        page->append(*setting_row(tr("settings.font"), tr("settings.font_hint"), *dd));
    }
    {
        const auto themes = Theme::get().available();
        std::vector<Glib::ustring> names;
        guint selected = 0;
        for (size_t i = 0; i < themes.size(); ++i) {
            names.push_back(themes[i].name);
            if (themes[i].id == Theme::get().id()) selected = static_cast<guint>(i);
        }
        auto* dd = Gtk::make_managed<Gtk::DropDown>(names);
        dd->set_selected(selected);
        dd->property_selected().signal_changed().connect([dd, themes] {
            const auto& t = themes[dd->get_selected()];
            Settings::get().set(key::theme, t.id);
            Theme::get().apply(paths::resource_dir() / "themes", t.id);
        });
        page->append(*setting_row(tr("settings.theme"), tr("settings.theme_hint"), *dd));
    }

    // ── library ──
    page->append(*section_title(tr("settings.library")));
    {
        music_dir_.set_text(Settings::get().str(key::music_dir, core::dirs().music.string()));
        music_dir_.add_css_class("muted");
        music_dir_.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
        music_dir_.set_max_width_chars(40);
        auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
        box->append(music_dir_);
        auto* change = Gtk::make_managed<Gtk::Button>(tr("settings.change"));
        change->signal_clicked().connect([this] {
            if (auto* w = window_of(*this))
                pick_folder(*w, tr("settings.music_dir"), [this](const std::string& p) {
                    Settings::get().set(key::music_dir, p);
                    music_dir_.set_text(p);
                    Library::get().index_dir_async(p);
                });
        });
        box->append(*change);
        page->append(*setting_row(tr("settings.music_dir"), tr("settings.music_dir_hint"), *box));
    }
    {
        auto* b = Gtk::make_managed<Gtk::Button>(tr("settings.reindex"));
        b->signal_clicked().connect([] { Library::get().index_dir_async(Settings::get().str(key::music_dir)); });
        Library::get().signal_indexing().connect(
            sigc::track_obj([b](bool on) { b->set_sensitive(!on); }, *this));
        page->append(*setting_row(tr("settings.reindex_title"), tr("settings.reindex_hint"), *b));
    }
    {
        auto* b = Gtk::make_managed<Gtk::Button>(tr("settings.add_files"));
        b->signal_clicked().connect([this] {
            if (auto* w = window_of(*this))
                pick_files(*w, tr("settings.add_files"),
                           [](std::vector<std::string> files) { Library::get().add_files_async(files); });
        });
        page->append(*setting_row(tr("settings.add_files_title"), tr("settings.add_files_hint"), *b));
    }
    {
        auto* b = Gtk::make_managed<Gtk::Button>(tr("settings.remove_invalid"));
        b->add_css_class("destructive-action");
        b->signal_clicked().connect([] {
            uint64_t n = 0;
            if (uint32_t st = idx_remove_invalid_tracks(&n); st != 0)
                Log::get().error("core", core::status_text(Domain::Idx, st));
            else
                Log::get().ok("core", tr("settings.removed_invalid", {std::to_string(n)}));
            Library::get().reload();
        });
        page->append(*setting_row(tr("settings.remove_invalid_title"), tr("settings.remove_invalid_hint"), *b));
    }

    // ── playback ──
    page->append(*section_title(tr("settings.playback")));
    auto* repeat = Gtk::make_managed<Gtk::Switch>();
    repeat->set_active(Player::get().repeat_playlist());
    repeat->property_active().signal_changed().connect([repeat] {
        if (repeat->get_active() != Player::get().repeat_playlist())
            Player::get().set_repeat_playlist(repeat->get_active());
    });
    Player::get().signal_modes_changed().connect(sigc::track_obj(
        [repeat] { repeat->set_active(Player::get().repeat_playlist()); }, *repeat));
    page->append(*setting_row(tr("settings.repeat_playlist"), "", *repeat));
    auto* close = Gtk::make_managed<Gtk::Switch>();
    close->set_active(Settings::get().boolean(key::close_played_playlist, true));
    close->property_active().signal_changed().connect([close] {
        Settings::get().set_bool(key::close_played_playlist, close->get_active());
    });
    page->append(*setting_row(tr("settings.close_played_playlist"), "", *close));
    {
        devices_ = Gtk::make_managed<Gtk::DropDown>(std::vector<Glib::ustring>{});
        devices_->set_size_request(260, -1);
        devices_->property_selected().signal_changed().connect([this] {
            if (filling_) return;
            const guint i = devices_->get_selected();
            if (i >= device_names_.size()) return;
            const std::string& name = device_names_[i];
            if (uint32_t st = engine_set_output_device(name.c_str()); st != 0)
                Log::get().error("player", core::status_text(Domain::Engine, st));
            else
                Settings::get().set(key::output_device, name);
        });
        auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
        box->append(*devices_);
        auto* refresh = icon_button("view-refresh-symbolic", tr("settings.refresh"));
        refresh->signal_clicked().connect(sigc::mem_fun(*this, &SettingsTab::fill_devices));
        box->append(*refresh);
        page->append(*setting_row(tr("settings.output"), tr("settings.output_hint"), *box));
        fill_devices();
    }

    // ── paths ──
    page->append(*section_title(tr("settings.paths")));
    const std::pair<std::string, std::string> dirs[] = {
        {tr("settings.path_app"), paths::app_dir().string()},
        {tr("settings.path_mods"), paths::mods_dir().string()},
        {tr("settings.path_resources"), paths::resource_dir().string()},
    };
    for (const auto& [name, path] : dirs) {
        auto* open = Gtk::make_managed<Gtk::Button>(tr("settings.open"));
        open->add_css_class("flat");
        open->signal_clicked().connect([this, path] { open_folder(*this, path); });
        page->append(*setting_row(name, path, *open));
    }

    // ── palette ──
    page->append(*section_title(tr("settings.palette")));
    page->append(*make_label(tr("settings.palette_hint"), "muted", true));
    {
        auto* grid = Gtk::make_managed<Gtk::FlowBox>();
        grid->set_selection_mode(Gtk::SelectionMode::NONE);
        grid->set_max_children_per_line(5);
        grid->set_min_children_per_line(2);
        grid->set_column_spacing(8);
        grid->set_row_spacing(6);
        std::vector<std::pair<std::string, Gtk::ColorDialogButton*>> buttons;
        for (const auto& token : Theme::tokens()) {
            auto* cell = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
            auto* b = Gtk::make_managed<Gtk::ColorDialogButton>(Gtk::ColorDialog::create());
            b->set_rgba(Theme::get().color(token));
            b->property_rgba().signal_changed().connect([b, token] {
                const Gdk::RGBA c = b->get_rgba();
                char hex[8];
                std::snprintf(hex, sizeof hex, "#%02x%02x%02x", static_cast<int>(c.get_red() * 255 + 0.5),
                              static_cast<int>(c.get_green() * 255 + 0.5), static_cast<int>(c.get_blue() * 255 + 0.5));
                if (Theme::get().color(token).to_string() != c.to_string()) Theme::get().set_override(token, hex);
            });
            cell->append(*b);
            cell->append(*make_label(tr("color." + token)));
            grid->append(*cell);
            buttons.push_back({token, b});
        }
        page->append(*grid);
        auto* reset = Gtk::make_managed<Gtk::Button>(tr("settings.palette_reset"));
        reset->set_halign(Gtk::Align::START);
        reset->signal_clicked().connect([buttons] {
            Theme::get().reset_overrides();
            for (const auto& [token, b] : buttons) b->set_rgba(Theme::get().color(token));
        });
        page->append(*reset);
    }

    // ── keyboard ──
    page->append(*section_title(tr("settings.shortcuts")));
    page->append(*make_label(tr("settings.shortcuts_hint"), "muted", true));
    for (const auto& info : Hotkeys::actions()) {
        const Action action = info.action;
        auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
        auto* bind = Gtk::make_managed<Gtk::Button>();
        bind->add_css_class("accel-button");
        bind->set_size_request(200, -1);
        auto show = [bind, action] {
            const std::string l = Hotkeys::label(Hotkeys::get().accel(action));
            bind->set_label(l.empty() ? tr("settings.shortcut_none") : l);
        };
        show();
        // capturing: the next key press (with modifiers) becomes the shortcut.
        auto keys = Gtk::EventControllerKey::create();
        keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
        keys->signal_key_pressed().connect(
            [bind, action, show](guint kv, guint code, Gdk::ModifierType st) {
                if (!Hotkeys::get().capturing || !bind->has_css_class("capturing")) return false;
                if (kv == GDK_KEY_Escape) {
                    // cancel
                } else if (kv == GDK_KEY_BackSpace) {
                    Hotkeys::get().set_accel(action, "");
                } else {
                    const std::string a = Hotkeys::accel_of(*bind->get_display(), kv, code, st);
                    if (a.empty()) return true;  // only a modifier so far
                    Hotkeys::get().set_accel(action, a);
                }
                Hotkeys::get().capturing = false;
                bind->remove_css_class("capturing");
                show();
                return true;
            },
            false);
        bind->add_controller(keys);
        auto focus = Gtk::EventControllerFocus::create();
        focus->signal_leave().connect([bind, show] {
            if (!bind->has_css_class("capturing")) return;
            Hotkeys::get().capturing = false;
            bind->remove_css_class("capturing");
            show();
        });
        bind->add_controller(focus);
        bind->signal_clicked().connect([bind] {
            Hotkeys::get().capturing = true;
            bind->add_css_class("capturing");
            bind->set_label(tr("settings.shortcut_press"));
            bind->grab_focus();
        });
        auto* on = Gtk::make_managed<Gtk::Switch>();
        on->set_active(Hotkeys::get().enabled(action));
        on->set_valign(Gtk::Align::CENTER);
        on->set_tooltip_text(tr("settings.shortcut_enabled"));
        on->property_active().signal_changed().connect([on, action] { Hotkeys::get().set_enabled(action, on->get_active()); });
        auto* reset = icon_button("edit-undo-symbolic", tr("settings.shortcut_reset"));
        reset->signal_clicked().connect([action, show, on] {
            Hotkeys::get().reset(action);
            on->set_active(true);
            show();
        });
        box->append(*bind);
        box->append(*on);
        box->append(*reset);
        page->append(*setting_row(tr(info.label_key), {}, *box));
    }
    const std::tuple<const char*, const char*, double, double, double> steps[] = {
        {"hotkey.volume_step", "settings.volume_step", 5, 1, 50},
        {"hotkey.track_volume_step", "settings.track_volume_step", 5, 1, 50},
        {"hotkey.seek_step", "settings.seek_step", 5, 1, 120},
    };
    for (const auto& [key, label, def, lo, hi] : steps) {
        auto* spin = Gtk::make_managed<Gtk::SpinButton>();
        spin->set_range(lo, hi);
        spin->set_increments(1, 5);
        spin->set_value(Settings::get().number(key, def));
        no_wheel(*spin);
        spin->signal_value_changed().connect([spin, k = std::string(key)] { Settings::get().set_number(k, spin->get_value()); });
        page->append(*setting_row(tr(label), {}, *spin));
    }

    scroll->set_child(*page);
    append(*scroll);
}

void SettingsTab::fill_devices() {
    filling_ = true;
    device_names_ = core::output_devices();
    auto list = Gtk::StringList::create({});
    const std::string current = core::output_device();
    guint selected = 0;
    for (size_t i = 0; i < device_names_.size(); ++i) {
        list->append(device_names_[i]);
        if (device_names_[i] == current) selected = static_cast<guint>(i);
    }
    devices_->set_model(list);
    devices_->set_selected(selected);
    filling_ = false;
}

}  // namespace dp::ui
