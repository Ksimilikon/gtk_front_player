#include "app/Application.hpp"

#include <gtkmm/alertdialog.h>
#include <gtkmm/window.h>

#include "config.h"
#include "core/Core.hpp"
#include "core/Library.hpp"
#include "core/Mods.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "theme/Theme.hpp"
#include "ui/MainWindow.hpp"
#include "util/Log.hpp"
#include "util/Paths.hpp"
#include "util/Settings.hpp"

namespace dp {

Glib::RefPtr<Application> Application::create() { return Glib::make_refptr_for_instance(new Application()); }

Application::Application() : Gtk::Application(DP_APP_ID, Gio::Application::Flags::DEFAULT_FLAGS) {}

Application::~Application() = default;

void Application::on_startup() {
    Gtk::Application::on_startup();
    paths::ensure_dirs();
    Settings::get().load(paths::config_dir() / "settings.conf");
    const auto res = paths::resource_dir();
    I18n::get().load(res / "lang", Settings::get().str(key::language, "auto"));
    Theme::get().apply(res / "themes", Settings::get().str(key::theme, "dark"));

    core::Dirs dirs{paths::cache_dir(), paths::config_dir(), paths::data_dir(),
                    Settings::get().str(key::music_dir, paths::default_music_dir().string())};
    const uint32_t st = core::init(dirs);
    if (st != 0) {
        Log::get().error("core", tr("app.core_init_failed", {core::status_text(core::Domain::Init, st)}));
        return;
    }
    core_ok_ = true;
    Log::get().info("core", tr("app.core_ready", {DP_CORE_VERSION}));

    // the output device chosen last time.
    if (auto dev = Settings::get().str(key::output_device); !dev.empty()) engine_set_output_device(dev.c_str());

    Library::get().reload();
    Player::get().start();
    Mods::get().start();
    // first launch: an empty library — index the music directory.
    if (Library::get().tracks().empty()) Library::get().index_dir_async(dirs.music.string());
}

void Application::show_fatal(const std::string& text) {
    auto* w = new Gtk::Window();
    w->set_title("Dream Player");
    w->set_default_size(420, 120);
    add_window(*w);
    w->present();
    auto d = Gtk::AlertDialog::create(text);
    d->set_detail(core::last_panic());
    d->choose(*w, [w, d](const Glib::RefPtr<Gio::AsyncResult>&) { w->close(); });
}

void Application::on_activate() {
    if (!core_ok_) {
        show_fatal(tr("app.core_init_failed", {""}));
        return;
    }
    if (!window_) {
        window_ = new ui::MainWindow();
        add_window(*window_);
        mpris_.start([this] { if (window_) window_->present(); }, [this] { quit(); });
    }
    window_->present();
}

void Application::on_shutdown() {
    mpris_.stop();
    if (core_ok_) {
        engine_stop();
        mods_stop_all();
    }
    delete window_;
    window_ = nullptr;
    Gtk::Application::on_shutdown();
}

}  // namespace dp
