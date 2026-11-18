// the main window: the player panel on top, the tabs, and a bottom bar with
// the status line and the playback controls (bottom right).
#pragma once

#include <gtkmm/applicationwindow.h>
#include <gtkmm/label.h>
#include <gtkmm/spinner.h>

#include "ui/Menus.hpp"
#include "util/Log.hpp"
#include "ui/Nav.hpp"
#include "ui/TabHost.hpp"
#include "ui/widgets/Controls.hpp"
#include "ui/widgets/PlayerPanel.hpp"

namespace dp::ui {

class MainWindow : public Gtk::ApplicationWindow, public Nav {
public:
    MainWindow();
    ~MainWindow() override;

    // Nav
    void open_playlists() override;
    void open_playlist(const std::string& name) override;
    void open_editor(const std::string& name) override;
    void open_song(int64_t id) override;
    void open_current_song() override;
    void open_page(const std::string& page) override;
    void open_mod_tab(const std::string& mod_path, uint32_t tab_id) override;
    void open_export_playlists(const std::vector<std::string>& names) override;
    void open_export_tracks(const std::vector<int64_t>& ids) override;

private:
    void open_from_env();
    bool on_key(guint keyval, guint keycode, Gdk::ModifierType state);
    void show_status(Level level, const std::string& text);

    Gtk::Box root_{Gtk::Orientation::VERTICAL, 0};
    PlayerPanel panel_;
    TabHost tabs_;
    MainMenuButton menu_;
    Gtk::Box bottom_{Gtk::Orientation::HORIZONTAL, 10};
    Gtk::Spinner busy_;
    Gtk::Label status_;
    Controls controls_;
    sigc::connection status_timer_;
};

}  // namespace dp::ui
