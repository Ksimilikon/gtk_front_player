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
#include "ui/widgets/VolumeZone.hpp"

namespace dp::ui {

class MainWindow : public Gtk::ApplicationWindow, public Nav {
public:
    MainWindow();
    ~MainWindow() override;

    // Nav
    void open_playlists() override;
    void open_playlist(const std::string& name) override;
    void play_playlist(const std::string& name, const std::vector<int64_t>& ids, size_t start) override;
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
    void run(int action);
    void open_queue(bool focus);

    Gtk::Box root_{Gtk::Orientation::VERTICAL, 0};
    PlayerPanel panel_;
    TabHost tabs_;
    MainMenuButton menu_;
    Gtk::Box bottom_{Gtk::Orientation::HORIZONTAL, 10};
    Gtk::Spinner busy_;
    VolumeZone volume_zone_;
    Controls controls_;
    bool closing_ = false;
};

}  // namespace dp::ui
