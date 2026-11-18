#include "ui/MainWindow.hpp"

#include <gtkmm/eventcontrollerkey.h>
#include <sigc++/adaptors/track_obj.h>

#include "core/Library.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/tabs/AboutTab.hpp"
#include "ui/tabs/EditorTab.hpp"
#include "ui/tabs/ExportTab.hpp"
#include "ui/tabs/ImportTab.hpp"
#include "ui/tabs/LogsTab.hpp"
#include "ui/tabs/ModPageTab.hpp"
#include "ui/tabs/ModsTab.hpp"
#include "ui/tabs/PlaylistTab.hpp"
#include "ui/tabs/PlaylistsTab.hpp"
#include "ui/tabs/SettingsTab.hpp"
#include "ui/tabs/SongTab.hpp"
#include "ui/tabs/SyncTab.hpp"
#include "util/Format.hpp"
#include "util/Settings.hpp"

namespace dp::ui {

MainWindow::MainWindow() {
    set_title("Dream Player");
    set_icon_name("audio-x-generic");
    set_default_size(static_cast<int>(Settings::get().number(key::window_width, 1100)),
                     static_cast<int>(Settings::get().number(key::window_height, 720)));
    set_nav(this);

    tabs_.set_end_widget(menu_);
    tabs_.set_vexpand(true);

    bottom_.add_css_class("bottom-bar");
    busy_.set_tooltip_text(tr("status.indexing"));
    status_.set_xalign(0);
    status_.set_hexpand(true);
    status_.set_ellipsize(Pango::EllipsizeMode::END);
    status_.add_css_class("status-line");
    bottom_.append(busy_);
    bottom_.append(status_);
    bottom_.append(controls_);

    root_.append(panel_);
    root_.append(tabs_);
    root_.append(bottom_);
    set_child(root_);

    panel_.signal_open().connect(sigc::mem_fun(*this, &MainWindow::open_current_song));
    open_playlists();
    open_from_env();

    auto keys = Gtk::EventControllerKey::create();
    keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    keys->signal_key_pressed().connect(sigc::mem_fun(*this, &MainWindow::on_key), false);
    add_controller(keys);

    Log::get().signal_added().connect(
        sigc::track_obj([this](const LogEntry& e) { show_status(e.level, e.text); }, *this));
    auto busy = [this](bool on) {
        busy_.set_spinning(on);
        busy_.set_visible(on);
    };
    Library::get().signal_indexing().connect(sigc::track_obj(busy, *this));
    busy(Library::get().indexing());

    signal_close_request().connect(
        [this] {
            Settings::get().set_number(key::window_width, get_width());
            Settings::get().set_number(key::window_height, get_height());
            return false;
        },
        false);
}

MainWindow::~MainWindow() { set_nav(nullptr); }

// a development aid: DREAM_PLAYER_GTK_OPEN="settings,song:1,playlist:Name,
// editor:,library" opens these tabs on start (the last one is focused).
void MainWindow::open_from_env() {
    const char* env = g_getenv("DREAM_PLAYER_GTK_OPEN");
    if (!env) return;
    for (const auto& item : fmt::split(env, ',')) {
        const auto colon = item.find(':');
        const std::string kind = item.substr(0, colon), arg = colon == std::string::npos ? "" : item.substr(colon + 1);
        if (kind == "song") open_song(std::atoll(arg.c_str()));
        else if (kind == "playlist") open_playlist(arg);
        else if (kind == "library") open_playlist("");
        else if (kind == "editor") open_editor(arg);
        else if (kind == "mod" && arg.rfind(':') != std::string::npos)
            open_mod_tab(arg.substr(0, arg.rfind(':')), static_cast<uint32_t>(std::atoi(arg.substr(arg.rfind(':') + 1).c_str())));
        else open_page(kind);
    }
}

void MainWindow::show_status(Level level, const std::string& text) {
    static const char* classes[] = {"log-info", "log-ok", "log-warn", "log-error"};
    for (const char* c : classes) status_.remove_css_class(c);
    status_.add_css_class(classes[static_cast<int>(level)]);
    status_.set_text(text);
    status_.set_tooltip_text(text);
    status_timer_.disconnect();
    status_timer_ = Glib::signal_timeout().connect(
        [this] {
            status_.set_text("");
            return false;
        },
        level == Level::Error ? 12000 : 6000);
}

/// true if the keyboard focus is in a text field (Space must type there).
static bool focus_in_text(Gtk::Window& w) {
    Gtk::Widget* f = w.get_focus();
    if (!f) return false;
    GtkWidget* g = f->gobj();
    return GTK_IS_EDITABLE(g) || GTK_IS_TEXT_VIEW(g);
}

bool MainWindow::on_key(guint keyval, guint, Gdk::ModifierType state) {
    const bool ctrl = (state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK;
    if (keyval == GDK_KEY_space && !ctrl && !focus_in_text(*this)) {
        Player::get().toggle();
        return true;
    }
    if (!ctrl) return false;
    switch (keyval) {
        case GDK_KEY_w: tabs_.close_current(); return true;
        case GDK_KEY_Tab: tabs_.cycle(+1); return true;
        case GDK_KEY_ISO_Left_Tab: tabs_.cycle(-1); return true;
        case GDK_KEY_Page_Down: tabs_.cycle(+1); return true;
        case GDK_KEY_Page_Up: tabs_.cycle(-1); return true;
        case GDK_KEY_Right:
            if (focus_in_text(*this)) return false;
            Player::get().next();
            return true;
        case GDK_KEY_Left:
            if (focus_in_text(*this)) return false;
            Player::get().previous();
            return true;
        case GDK_KEY_n: open_editor(""); return true;
        case GDK_KEY_comma: open_page("settings"); return true;
        case GDK_KEY_q: close(); return true;
        default: return false;
    }
}

void MainWindow::open_playlists() {
    tabs_.open("playlists", [] { return std::make_unique<PlaylistsTab>(); });
}

void MainWindow::open_playlist(const std::string& name) {
    tabs_.open(PlaylistTab::key_for(name), [name] { return std::make_unique<PlaylistTab>(name); });
}

void MainWindow::open_editor(const std::string& name) {
    tabs_.open(EditorTab::key_for(name), [name] { return std::make_unique<EditorTab>(name); });
}

void MainWindow::open_song(int64_t id) {
    tabs_.open(SongTab::key_for(id), [id] { return std::make_unique<SongTab>(id); });
}

void MainWindow::open_current_song() {
    const auto& p = Player::get();
    const auto cur = p.current();
    if (!cur || !p.state().has_track) return;
    if (cur->id) open_song(cur->id);
}

void MainWindow::open_page(const std::string& page) {
    std::function<std::unique_ptr<Tab>()> make;
    if (page == "settings") make = [] { return std::make_unique<SettingsTab>(); };
    else if (page == "sync") make = [] { return std::make_unique<SyncTab>(); };
    else if (page == "import") make = [] { return std::make_unique<ImportTab>(); };
    else if (page == "export") make = [] { return std::make_unique<ExportTab>(); };
    else if (page == "logs") make = [] { return std::make_unique<LogsTab>(); };
    else if (page == "mods") make = [] { return std::make_unique<ModsTab>(); };
    else if (page == "about") make = [] { return std::make_unique<AboutTab>(); };
    else return;
    tabs_.open(page, make);
}

void MainWindow::open_mod_tab(const std::string& mod_path, uint32_t tab_id) {
    tabs_.open(ModPageTab::key_for(mod_path, tab_id),
               [mod_path, tab_id] { return std::make_unique<ModPageTab>(mod_path, tab_id); });
}

void MainWindow::open_export_playlists(const std::vector<std::string>& names) {
    open_page("export");
    if (auto* t = dynamic_cast<ExportTab*>(tabs_.find("export"))) t->preset_playlists(names);
}

void MainWindow::open_export_tracks(const std::vector<int64_t>& ids) {
    open_page("export");
    if (auto* t = dynamic_cast<ExportTab*>(tabs_.find("export"))) t->preset_tracks(ids);
}

}  // namespace dp::ui
