#include "ui/tabs/SettingsTab.hpp"

#include <giomm/file.h>
#include <gtkmm/button.h>
#include <gtkmm/filelauncher.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stringlist.h>
#include <sigc++/adaptors/track_obj.h>

#include "core/Core.hpp"
#include "core/Library.hpp"
#include "i18n/I18n.hpp"
#include "theme/Theme.hpp"
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
        dd->property_selected().signal_changed().connect([dd, langs] {
            const guint i = dd->get_selected();
            Settings::get().set(key::language, i == 0 ? "auto" : langs[i - 1].code);
            Log::get().info("ui", tr("settings.restart_needed"));
        });
        page->append(*setting_row(tr("settings.language"), tr("settings.language_hint"), *dd));
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
        {tr("settings.path_config"), paths::config_dir().string()},
        {tr("settings.path_data"), paths::data_dir().string()},
        {tr("settings.path_cache"), paths::cache_dir().string()},
        {tr("settings.path_mods"), paths::mods_dir().string()},
        {tr("settings.path_resources"), paths::resource_dir().string()},
    };
    for (const auto& [name, path] : dirs) {
        auto* open = Gtk::make_managed<Gtk::Button>(tr("settings.open"));
        open->add_css_class("flat");
        open->signal_clicked().connect([this, path] { open_folder(*this, path); });
        page->append(*setting_row(name, path, *open));
    }

    // ── shortcuts ──
    page->append(*section_title(tr("settings.shortcuts")));
    const char* keys[][2] = {{"Space", "shortcut.play_pause"}, {"Ctrl+→ / Ctrl+←", "shortcut.next_prev"},
                             {"Ctrl+W", "shortcut.close_tab"},   {"Ctrl+Tab", "shortcut.next_tab"},
                             {"Ctrl+N", "shortcut.new_playlist"}, {"Ctrl+,", "shortcut.settings"},
                             {"Ctrl+Q", "shortcut.quit"}};
    for (const auto& k : keys) page->append(*info_row(k[0], tr(k[1])));

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
