#include "ui/widgets/Dnd.hpp"

#include <gdkmm/contentprovider.h>
#include <gtkmm/dragsource.h>
#include <gtkmm/droptarget.h>
#include <gtkmm/widgetpaintable.h>

#include <cstdlib>

namespace dp::ui {

using StrValue = Glib::Value<Glib::ustring>;

void drag_source(Gtk::Widget& w, std::function<std::string()> payload) {
    auto src = Gtk::DragSource::create();
    src->set_actions(Gdk::DragAction::MOVE | Gdk::DragAction::COPY);
    src->signal_prepare().connect(
        [payload](double, double) {
            StrValue v;
            v.init(StrValue::value_type());
            v.set(payload());
            return Gdk::ContentProvider::create(v);
        },
        false);
    src->signal_drag_begin().connect([&w, s = src.get()](const Glib::RefPtr<Gdk::Drag>&) {
        // the row itself follows the pointer.
        s->set_icon(Gtk::WidgetPaintable::create(w), 16, 16);
        w.add_css_class("dragging");
    });
    src->signal_drag_end().connect([&w](const Glib::RefPtr<Gdk::Drag>&, bool) { w.remove_css_class("dragging"); });
    w.add_controller(src);
}

static void clear_marks(Gtk::Widget& w) {
    w.remove_css_class("drop-above");
    w.remove_css_class("drop-below");
}

void drop_on_row(Gtk::Widget& w, std::function<bool(const std::string&, bool)> on_drop) {
    auto target = Gtk::DropTarget::create(StrValue::value_type(), Gdk::DragAction::MOVE | Gdk::DragAction::COPY);
    target->signal_motion().connect(
        [&w](double, double y) {
            const bool after = y > w.get_height() / 2.0;
            w.remove_css_class(after ? "drop-above" : "drop-below");
            w.add_css_class(after ? "drop-below" : "drop-above");
            return Gdk::DragAction::MOVE;
        },
        false);
    target->signal_leave().connect([&w] { clear_marks(w); });
    target->signal_drop().connect(
        [&w, on_drop](const Glib::ValueBase& value, double, double y) {
            clear_marks(w);
            if (!G_VALUE_HOLDS_STRING(value.gobj())) return false;
            const char* s = g_value_get_string(value.gobj());
            return on_drop(s ? s : "", y > w.get_height() / 2.0);
        },
        false);
    w.add_controller(target);
}

void drop_on_area(Gtk::Widget& w, std::function<bool(const std::string&)> on_drop) {
    auto target = Gtk::DropTarget::create(StrValue::value_type(), Gdk::DragAction::MOVE | Gdk::DragAction::COPY);
    target->signal_enter().connect(
        [&w](double, double) {
            w.add_css_class("drop-area");
            return Gdk::DragAction::MOVE;
        },
        false);
    target->signal_leave().connect([&w] { w.remove_css_class("drop-area"); });
    target->signal_drop().connect(
        [&w, on_drop](const Glib::ValueBase& value, double, double) {
            w.remove_css_class("drop-area");
            if (!G_VALUE_HOLDS_STRING(value.gobj())) return false;
            const char* s = g_value_get_string(value.gobj());
            return on_drop(s ? s : "");
        },
        false);
    w.add_controller(target);
}

long payload_num(const std::string& payload, const std::string& prefix) {
    if (payload.rfind(prefix, 0) != 0) return -1;
    return std::strtol(payload.c_str() + prefix.size(), nullptr, 10);
}

}  // namespace dp::ui
