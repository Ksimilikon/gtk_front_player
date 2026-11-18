// a tab page. Every page has a unique key: opening a key that is already open
// focuses that tab instead of creating a second one.
#pragma once

#include <glibmm/main.h>
#include <gtkmm/box.h>
#include <sigc++/signal.h>

#include <functional>
#include <string>

#include "util/MainThread.hpp"

namespace dp::ui {

class Tab : public Gtk::Box {
public:
    Tab() : Gtk::Box(Gtk::Orientation::VERTICAL, 0) { add_css_class("tab-page"); }
    ~Tab() override = default;

    virtual std::string key() const = 0;
    virtual std::string title() const = 0;
    virtual bool closable() const { return true; }
    /// the tab became the visible one.
    virtual void on_activated() {}

    /// the title changed (the header re-reads `title()`).
    sigc::signal<void()> signal_title_changed;
    /// the page asks to be closed.
    sigc::signal<void()> signal_close_request;

protected:
    /// runs `fn` on the next main loop iteration if the page still exists
    /// (e.g. rebuilding a list from a handler of one of its own rows).
    void defer(std::function<void()> fn) {
        Glib::signal_idle().connect_once([ref = AliveRef(alive_), fn = std::move(fn)] {
            if (!ref.expired()) fn();
        });
    }

    /// background results check it before touching the page.
    Alive alive_ = make_alive();
};

}  // namespace dp::ui
