#include "ui/MainWindow.hpp"

#include <gtkmm/eventcontrollerkey.h>
#include <sigc++/adaptors/track_obj.h>

#include "core/Library.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/Hotkeys.hpp"
#include "ui/tabs/AboutTab.hpp"
#include "ui/tabs/EditorTab.hpp"
#include "ui/tabs/ExportTab.hpp"
#include "ui/tabs/ImportTab.hpp"
#include "ui/tabs/LogsTab.hpp"
#include "ui/tabs/ModPageTab.hpp"
#include "ui/tabs/ModsTab.hpp"
#include "ui/tabs/PlaylistTab.hpp"
#include "ui/tabs/PlaylistsTab.hpp"
#include "ui/tabs/QueueTab.hpp"
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
    bottom_.append(busy_);
    bottom_.append(volume_zone_);
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
        sigc::track_obj([this](const LogEntry& e) { volume_zone_.show_status(e.level, e.text); }, *this));
    // a new queue: its `*` tab comes to the front with its scroll reset.
    Player::get().signal_queue_started().connect(sigc::track_obj([this](bool focus) { open_queue(focus); }, *this));
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
            if (closing_) return false;
            // unsaved editors: ask first, then close for real.
            tabs_.resolve_unsaved([this] {
                closing_ = true;
                close();
            });
            return true;
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

/// true if the keyboard focus is in a text field (Space and arrows must work
/// there as usual).
static bool focus_in_text(Gtk::Window& w) {
    Gtk::Widget* f = w.get_focus();
    if (!f) return false;
    GtkWidget* g = f->gobj();
    return GTK_IS_EDITABLE(g) || GTK_IS_TEXT_VIEW(g);
}

bool MainWindow::on_key(guint keyval, guint keycode, Gdk::ModifierType state) {
    const auto action = Hotkeys::get().match(*get_display(), keyval, keycode, state, focus_in_text(*this));
    if (!action) return false;
    run(static_cast<int>(*action));
    return true;
}

void MainWindow::run(int a) {
    auto& p = Player::get();
    auto& h = Hotkeys::get();
    switch (static_cast<Action>(a)) {
        case Action::PlayPause: p.toggle(); break;
        case Action::VolumeUp: p.change_volume(h.volume_step()); break;
        case Action::VolumeDown: p.change_volume(-h.volume_step()); break;
        case Action::NextTrack: p.next(); break;
        case Action::PrevTrack: p.previous(); break;
        case Action::TrackVolumeUp: p.change_track_volume(static_cast<float>(h.track_volume_step())); break;
        case Action::TrackVolumeDown: p.change_track_volume(-static_cast<float>(h.track_volume_step())); break;
        case Action::SeekForward: p.seek_by(h.seek_step()); break;
        case Action::SeekBack: p.seek_by(-h.seek_step()); break;
        case Action::Save:
            if (auto* t = tabs_.current()) t->save_changes();
            break;
        case Action::CloseTab: tabs_.close_current(); break;
        case Action::NextTab: tabs_.cycle(+1); break;
        case Action::PrevTab: tabs_.cycle(-1); break;
        case Action::NewPlaylist: open_editor(""); break;
        case Action::Reindex:
            // on the playlists tab only.
            if (auto* t = tabs_.current(); t && t->key() == "playlists") Library::get().index_dir_async(Settings::get().str(key::music_dir));
            break;
        case Action::OpenSettings: open_page("settings"); break;
        case Action::Quit: close(); break;
    }
}

void MainWindow::open_queue(bool focus) {
    Tab* current = tabs_.current();
    Tab* q = tabs_.open(QueueTab::kKey, [] { return std::make_unique<QueueTab>(); });
    q->signal_title_changed.emit();
    if (focus) {
        TabHost::reset_scroll(*q);
    } else if (current && current != q) {
        tabs_.focus(current->key());  // a preview: stay where the user is
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
