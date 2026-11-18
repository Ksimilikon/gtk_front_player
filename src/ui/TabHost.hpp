// the tab mechanism: a header row of tab buttons (with an end slot for the
// menu button) and a stack of pages. Owns the pages.
#pragma once

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stack.h>

#include <functional>
#include <memory>
#include <vector>

#include "ui/Tab.hpp"

namespace dp::ui {

class TabHost : public Gtk::Box {
public:
    TabHost();
    ~TabHost() override;

    /// focuses the tab with `key`, or creates it with `make` and focuses it.
    Tab* open(const std::string& key, const std::function<std::unique_ptr<Tab>()>& make);
    Tab* find(const std::string& key) const;
    void focus(const std::string& key);
    /// closes a tab; with unsaved changes asks save / don't save / cancel.
    void close(const std::string& key);
    /// closes without asking.
    void close_now(const std::string& key);
    /// asks about every tab with unsaved changes; `then()` once none is left
    /// (not called if the user cancels).
    void resolve_unsaved(std::function<void()> then);
    /// scrolls every scrolled area of the tab to the top / left.
    static void reset_scroll(Gtk::Widget& w);
    void close_current();
    void cycle(int dir);
    Tab* current() const;

    /// a widget at the end of the header (the menu button).
    void set_end_widget(Gtk::Widget& w);

private:
    struct Entry {
        std::unique_ptr<Tab> page;
        Gtk::Box* header = nullptr;
        Gtk::Label* label = nullptr;
    };
    Gtk::Box* make_header(Tab& tab);
    void update_headers();

    Gtk::Box bar_{Gtk::Orientation::HORIZONTAL, 0};
    Gtk::ScrolledWindow scroller_;
    Gtk::Box buttons_{Gtk::Orientation::HORIZONTAL, 2};
    Gtk::Stack stack_;
    std::vector<Entry> tabs_;
};

}  // namespace dp::ui
