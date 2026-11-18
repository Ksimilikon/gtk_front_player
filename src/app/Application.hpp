// the GTK application: brings up settings, localization, the theme and the
// core, then the main window and the D-Bus (MPRIS) server.
#pragma once

#include <gtkmm/application.h>

#include <memory>

#include "dbus/Mpris.hpp"

namespace dp {

namespace ui {
class MainWindow;
}

/// quits (asking about unsaved changes first) and starts the app again.
void request_restart();
/// set by `request_restart`; `main` re-executes the program after `run`.
bool restart_requested();

class Application : public Gtk::Application {
public:
    static Glib::RefPtr<Application> create();
    ~Application() override;

protected:
    Application();
    void on_startup() override;
    void on_activate() override;
    void on_shutdown() override;

private:
    void show_fatal(const std::string& text);
    bool core_ok_ = false;
    ui::MainWindow* window_ = nullptr;
    Mpris mpris_;
};

}  // namespace dp
