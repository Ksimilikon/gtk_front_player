// small shared widgets and helpers of the UI.
#pragma once

#include <gdkmm/texture.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/picture.h>
#include <gtkmm/snapshot.h>
#include <gtkmm/widget.h>
#include <gtkmm/popover.h>
#include <gtkmm/window.h>

#include <functional>
#include <string>
#include <vector>

namespace dp::ui {

/// a square cover of a fixed size: the picture is cropped to fill the square
/// (like CSS `object-fit: cover`) with rounded corners; a note glyph when
/// there is no picture.
class CoverView : public Gtk::Widget {
public:
    explicit CoverView(int size);
    void set(const Glib::RefPtr<Gdk::Texture>& tex);
    void set_size(int size);

protected:
    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void measure_vfunc(Gtk::Orientation, int, int& min, int& nat, int& min_base, int& nat_base) const override;
    void snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot) override;

private:
    int size_;
    Glib::RefPtr<Gdk::Texture> tex_;
};

Gtk::Label* make_label(const std::string& text, const std::string& css_class = {}, bool wrap = false);
Gtk::Label* section_title(const std::string& text);
/// a row `name: value` for info blocks.
Gtk::Box* info_row(const std::string& name, const std::string& value, bool selectable = false);
Gtk::Button* icon_button(const std::string& icon, const std::string& tooltip, const std::string& css = "flat");
/// a yellow / red banner.
Gtk::Box* banner(const std::string& text, bool error);

/// a context menu: a popover with flat buttons.
struct MenuItem {
    MenuItem(std::string l, std::function<void()> a, std::string c = {}, bool sep = false)
        : label(std::move(l)), action(std::move(a)), css(std::move(c)), separator(sep) {}
    std::string label;
    std::function<void()> action;
    std::string css;         // e.g. "destructive"
    bool separator = false;  // draw a separator before the item
};
/// shows `items` under `parent` at (x, y) in its coordinates.
void show_menu(Gtk::Widget& parent, double x, double y, const std::vector<MenuItem>& items);
/// attaches right-click (and long-press) → `build()` items.
void on_context_menu(Gtk::Widget& w, std::function<std::vector<MenuItem>()> build);

// file dialogs (async; `done` is not called on cancel).
using PathCb = std::function<void(const std::string&)>;
void pick_file(Gtk::Window& parent, const std::string& title, const std::vector<std::string>& patterns,
               const std::string& filter_name, PathCb done);
void pick_files(Gtk::Window& parent, const std::string& title, std::function<void(std::vector<std::string>)> done);
void pick_folder(Gtk::Window& parent, const std::string& title, PathCb done);
void save_file(Gtk::Window& parent, const std::string& title, const std::string& initial_name, PathCb done);

/// the window a widget is in.
Gtk::Window* window_of(Gtk::Widget& w);

/// a simple yes / no confirmation.
void confirm(Gtk::Window& parent, const std::string& message, const std::string& yes, std::function<void()> on_yes);

}  // namespace dp::ui
