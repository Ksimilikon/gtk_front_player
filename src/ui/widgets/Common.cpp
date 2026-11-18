#include "ui/widgets/Common.hpp"

#include <giomm/liststore.h>
#include <glibmm/main.h>
#include <gtkmm/alertdialog.h>
#include <gtkmm/button.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/gesturelongpress.h>
#include <gtkmm/separator.h>

#include "core/Covers.hpp"
#include "i18n/I18n.hpp"
#include "theme/Theme.hpp"

namespace dp::ui {

CoverView::CoverView(int size) : Glib::ObjectBase("DpCoverView"), size_(size) {
    add_css_class("cover");
    set_halign(Gtk::Align::CENTER);
    set_valign(Gtk::Align::CENTER);
    set_overflow(Gtk::Overflow::HIDDEN);
}

void CoverView::set_size(int size) {
    size_ = size;
    queue_resize();
}

void CoverView::set(const Glib::RefPtr<Gdk::Texture>& tex) {
    tex_ = tex;
    queue_draw();
}

Gtk::SizeRequestMode CoverView::get_request_mode_vfunc() const { return Gtk::SizeRequestMode::CONSTANT_SIZE; }

void CoverView::measure_vfunc(Gtk::Orientation, int, int& min, int& nat, int& min_base, int& nat_base) const {
    min = nat = size_;
    min_base = nat_base = -1;
}

void CoverView::set_track_async(int64_t id) {
    const uint64_t req = ++request_;
    Covers::get().thumb_track(id, size_, alive_, [this, req](const Glib::RefPtr<Gdk::Texture>& t) {
        if (req == request_) set(t);
    });
}

void CoverView::set_file_async(const std::string& path, int64_t fallback_track) {
    const uint64_t req = ++request_;
    auto with_fallback = [this, req, fallback_track](const Glib::RefPtr<Gdk::Texture>& t) {
        if (req != request_) return;
        if (t || !fallback_track) return set(t);
        set_track_async(fallback_track);
    };
    if (path.empty()) return with_fallback({});
    Covers::get().thumb_file(path, size_, alive_, with_fallback);
}

void CoverView::snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot) {
    const float w = static_cast<float>(get_width()), h = static_cast<float>(get_height());
    // a plain square: the picture fills it, cropped.
    snapshot->push_clip(Gdk::Graphene::Rect(0, 0, w, h));
    const Gdk::RGBA bg = Theme::get().color("surface_alt");
    snapshot->append_color(bg, Gdk::Graphene::Rect(0, 0, w, h));
    if (tex_) {
        // crop to fill: scale by the larger ratio, center.
        const float tw = static_cast<float>(tex_->get_width()), th = static_cast<float>(tex_->get_height());
        const float scale = std::max(w / tw, h / th);
        const float dw = tw * scale, dh = th * scale;
        snapshot->append_texture(tex_, Gdk::Graphene::Rect((w - dw) / 2, (h - dh) / 2, dw, dh));
    } else {
        auto layout = create_pango_layout("♪");
        Pango::FontDescription font;
        font.set_absolute_size(std::max(10.0, h * 0.42) * PANGO_SCALE);
        layout->set_font_description(font);
        int lw = 0, lh = 0;
        layout->get_pixel_size(lw, lh);
        Gdk::RGBA fg = Theme::get().color("muted");
        fg.set_alpha(0.55);
        snapshot->save();
        snapshot->translate(Gdk::Graphene::Point((w - lw) / 2, (h - lh) / 2));
        snapshot->append_layout(layout, fg);
        snapshot->restore();
    }
    snapshot->pop();
}

void no_wheel(Gtk::Widget& w) {
    GListModel* list = gtk_widget_observe_controllers(w.gobj());
    std::vector<GtkEventController*> scroll;
    for (guint i = 0; i < g_list_model_get_n_items(list); ++i) {
        auto* c = static_cast<GtkEventController*>(g_list_model_get_item(list, i));
        if (GTK_IS_EVENT_CONTROLLER_SCROLL(c)) scroll.push_back(c);
        g_object_unref(c);
    }
    g_object_unref(list);
    for (auto* c : scroll) gtk_widget_remove_controller(w.gobj(), c);
}

Gtk::Label* make_label(const std::string& text, const std::string& css_class, bool wrap) {
    auto* l = Gtk::make_managed<Gtk::Label>(text);
    l->set_xalign(0);
    if (!css_class.empty()) l->add_css_class(css_class);
    if (wrap) {
        l->set_wrap(true);
        l->set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    } else {
        l->set_ellipsize(Pango::EllipsizeMode::END);
    }
    return l;
}

Gtk::Label* section_title(const std::string& text) {
    auto* l = make_label(text, "section-title");
    l->set_margin_top(14);
    l->set_margin_bottom(6);
    return l;
}

Gtk::Box* info_row(const std::string& name, const std::string& value, bool selectable) {
    auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    auto* n = make_label(name, "muted");
    n->set_size_request(160, -1);
    auto* v = make_label(value, {}, true);
    v->set_selectable(selectable);
    v->set_hexpand(true);
    row->append(*n);
    row->append(*v);
    return row;
}

Gtk::Button* icon_button(const std::string& icon, const std::string& tooltip, const std::string& css) {
    auto* b = Gtk::make_managed<Gtk::Button>();
    b->set_icon_name(icon);
    b->set_tooltip_text(tooltip);
    if (!css.empty()) b->add_css_class(css);
    return b;
}

Gtk::Box* banner(const std::string& text, bool error) {
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    box->add_css_class(error ? "banner-error" : "banner-warning");
    auto* icon = Gtk::make_managed<Gtk::Image>();
    icon->set_from_icon_name(error ? "dialog-error-symbolic" : "dialog-warning-symbolic");
    box->append(*icon);
    auto* l = make_label(text, {}, true);
    l->set_hexpand(true);
    box->append(*l);
    return box;
}

void autohide_on_leave(Gtk::Popover& pop) {
    auto timer = std::make_shared<sigc::connection>();
    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_enter().connect([timer](double, double) { timer->disconnect(); });
    motion->signal_leave().connect([&pop, timer] {
        timer->disconnect();
        *timer = Glib::signal_timeout().connect(
            [&pop] {
                pop.popdown();
                return false;
            },
            200);
    });
    pop.add_controller(motion);
    pop.signal_closed().connect([timer] { timer->disconnect(); });
}

void show_menu(Gtk::Widget& parent, double x, double y, const std::vector<MenuItem>& items) {
    if (items.empty()) return;
    auto* pop = new Gtk::Popover();
    pop->add_css_class("context-menu");
    pop->set_has_arrow(false);
    pop->set_parent(parent);
    Gdk::Rectangle r(static_cast<int>(x), static_cast<int>(y), 1, 1);
    pop->set_pointing_to(r);
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    for (const auto& it : items) {
        if (it.separator) box->append(*Gtk::make_managed<Gtk::Separator>());
        auto* b = Gtk::make_managed<Gtk::Button>();
        auto* l = make_label(it.label);
        b->set_child(*l);
        b->add_css_class("flat");
        b->add_css_class("menu-item");
        if (!it.css.empty()) b->add_css_class(it.css);
        b->signal_clicked().connect([pop, action = it.action] {
            pop->popdown();
            // after the popover is gone: the action may rebuild its parent.
            if (action) Glib::signal_idle().connect_once(action, Glib::PRIORITY_LOW);
        });
        box->append(*b);
    }
    pop->set_child(*box);
    autohide_on_leave(*pop);
    pop->signal_closed().connect([pop] {
        // deferred: the popover may still be inside its own signal emission.
        Glib::signal_idle().connect_once([pop] {
            pop->unparent();
            delete pop;
        });
    });
    pop->popup();
}

void on_context_menu(Gtk::Widget& w, std::function<std::vector<MenuItem>()> build) {
    auto click = Gtk::GestureClick::create();
    click->set_button(GDK_BUTTON_SECONDARY);
    click->signal_pressed().connect([&w, build](int, double x, double y) { show_menu(w, x, y, build()); });
    w.add_controller(click);
    auto press = Gtk::GestureLongPress::create();
    press->set_touch_only(true);
    press->signal_pressed().connect([&w, build](double x, double y) { show_menu(w, x, y, build()); });
    w.add_controller(press);
}

static Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>> filters(const std::vector<std::string>& patterns,
                                                              const std::string& name) {
    auto store = Gio::ListStore<Gtk::FileFilter>::create();
    if (patterns.empty()) return store;
    auto f = Gtk::FileFilter::create();
    f->set_name(name);
    for (const auto& p : patterns) f->add_pattern(p);
    store->append(f);
    auto all = Gtk::FileFilter::create();
    all->set_name(tr("dialog.all_files"));
    all->add_pattern("*");
    store->append(all);
    return store;
}

void pick_file(Gtk::Window& parent, const std::string& title, const std::vector<std::string>& patterns,
               const std::string& filter_name, PathCb done) {
    auto d = Gtk::FileDialog::create();
    d->set_title(title);
    d->set_modal(true);
    if (!patterns.empty()) d->set_filters(filters(patterns, filter_name));
    d->open(parent, [d, done](const Glib::RefPtr<Gio::AsyncResult>& r) {
        try {
            if (auto f = d->open_finish(r)) done(f->get_path());
        } catch (const Glib::Error&) {
        }
    });
}

void pick_files(Gtk::Window& parent, const std::string& title, std::function<void(std::vector<std::string>)> done) {
    auto d = Gtk::FileDialog::create();
    d->set_title(title);
    d->set_modal(true);
    d->open_multiple(parent, [d, done](const Glib::RefPtr<Gio::AsyncResult>& r) {
        try {
            std::vector<std::string> out;
            for (const auto& f : d->open_multiple_finish(r)) out.push_back(f->get_path());
            if (!out.empty()) done(out);
        } catch (const Glib::Error&) {
        }
    });
}

void pick_folder(Gtk::Window& parent, const std::string& title, PathCb done) {
    auto d = Gtk::FileDialog::create();
    d->set_title(title);
    d->set_modal(true);
    d->select_folder(parent, [d, done](const Glib::RefPtr<Gio::AsyncResult>& r) {
        try {
            if (auto f = d->select_folder_finish(r)) done(f->get_path());
        } catch (const Glib::Error&) {
        }
    });
}

void save_file(Gtk::Window& parent, const std::string& title, const std::string& initial_name, PathCb done) {
    auto d = Gtk::FileDialog::create();
    d->set_title(title);
    d->set_modal(true);
    d->set_initial_name(initial_name);
    d->save(parent, [d, done](const Glib::RefPtr<Gio::AsyncResult>& r) {
        try {
            if (auto f = d->save_finish(r)) done(f->get_path());
        } catch (const Glib::Error&) {
        }
    });
}

Gtk::Window* window_of(Gtk::Widget& w) { return dynamic_cast<Gtk::Window*>(w.get_root()); }

void confirm(Gtk::Window& parent, const std::string& message, const std::string& yes, std::function<void()> on_yes) {
    auto d = Gtk::AlertDialog::create(message);
    d->set_buttons({tr("common.cancel"), yes});
    d->set_cancel_button(0);
    d->set_default_button(1);
    d->choose(parent, [d, on_yes](const Glib::RefPtr<Gio::AsyncResult>& r) {
        try {
            if (d->choose_finish(r) == 1) on_yes();
        } catch (const Glib::Error&) {
        }
    });
}

}  // namespace dp::ui
