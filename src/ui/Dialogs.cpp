#include "ui/Dialogs.hpp"

#include <glibmm/main.h>
#include <gtkmm/alertdialog.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/searchentry.h>

#include "core/Library.hpp"
#include "i18n/I18n.hpp"
#include "ui/widgets/Common.hpp"
#include "util/Format.hpp"

namespace dp::ui {

/// a modal window that deletes itself when closed.
static Gtk::Window* make_dialog(Gtk::Window& parent, const std::string& title, int width) {
    auto* w = new Gtk::Window();
    w->set_title(title);
    w->set_transient_for(parent);
    w->set_modal(true);
    w->set_default_size(width, -1);
    w->set_resizable(false);
    w->add_css_class("dialog-window");
    w->signal_close_request().connect(
        [w] {
            Glib::signal_idle().connect_once([w] { delete w; });
            return false;
        },
        false);
    auto keys = Gtk::EventControllerKey::create();
    keys->signal_key_pressed().connect(
        [w](guint kv, guint, Gdk::ModifierType) {
            if (kv != GDK_KEY_Escape) return false;
            w->close();
            return true;
        },
        false);
    w->add_controller(keys);
    return w;
}

void pick_playlist(Gtk::Window& parent, const std::string& title, std::function<void(const std::string&)> done) {
    auto* w = make_dialog(parent, title, 380);
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    box->add_css_class("dialog-body");
    auto* search = Gtk::make_managed<Gtk::SearchEntry>();
    search->set_placeholder_text(tr("playlists.search"));
    box->append(*search);
    auto* list = Gtk::make_managed<Gtk::ListBox>();
    list->add_css_class("plain-list");
    list->set_selection_mode(Gtk::SelectionMode::NONE);
    for (const auto& p : Library::get().playlists()) {
        auto* l = make_label(p.name + "   " + std::to_string(p.tracks.size()), {});
        l->set_margin(4);
        l->set_name(p.name);
        list->append(*l);
    }
    list->set_filter_func([search](Gtk::ListBoxRow* row) {
        return fmt::contains_ci(row->get_child()->get_name(), search->get_text());
    });
    search->signal_search_changed().connect([list] { list->invalidate_filter(); });
    list->signal_row_activated().connect([w, done](Gtk::ListBoxRow* row) {
        const std::string name = row->get_child()->get_name();
        w->close();
        done(name);
    });
    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_child(*list);
    scroll->set_min_content_height(260);
    scroll->set_propagate_natural_height(true);
    scroll->set_max_content_height(420);
    box->append(*scroll);

    // a new playlist.
    auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    auto* entry = Gtk::make_managed<Gtk::Entry>();
    entry->set_placeholder_text(tr("dialog.new_playlist_name"));
    entry->set_hexpand(true);
    auto* create = Gtk::make_managed<Gtk::Button>(tr("dialog.create"));
    auto make_new = [w, entry, done] {
        const std::string n = fmt::trim(entry->get_text());
        if (n.empty()) return;
        w->close();
        done(n);
    };
    create->signal_clicked().connect(make_new);
    entry->signal_activate().connect(make_new);
    row->append(*entry);
    row->append(*create);
    box->append(*row);
    w->set_child(*box);
    w->present();
    search->grab_focus();
}

void ask_playlist_name(Gtk::Window& parent, const std::string& title, const std::string& initial,
                       std::function<void(const std::string&)> done) {
    auto* w = make_dialog(parent, title, 420);
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    box->add_css_class("dialog-body");
    box->append(*make_label(tr("dialog.playlist_name"), "muted"));
    auto* entry = Gtk::make_managed<Gtk::Entry>();
    entry->set_text(initial);
    box->append(*entry);
    auto* note = make_label("", "error-text", true);
    box->append(*note);
    auto update = [entry, note] {
        const std::string n = fmt::trim(entry->get_text());
        if (n.empty())
            note->set_text(tr("editor.err.empty_name"));
        else if (Library::get().playlist(n))
            note->set_text(tr("dialog.will_overwrite", {n}));
        else
            note->set_text(tr("dialog.will_create", {n}));
    };
    entry->signal_changed().connect(update);
    update();
    auto* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    buttons->set_halign(Gtk::Align::END);
    auto* cancel = Gtk::make_managed<Gtk::Button>(tr("common.cancel"));
    cancel->signal_clicked().connect([w] { w->close(); });
    auto* save = Gtk::make_managed<Gtk::Button>(tr("common.save"));
    save->add_css_class("suggested-action");
    auto finish = [w, entry, done] {
        const std::string n = fmt::trim(entry->get_text());
        if (n.empty()) return;
        w->close();
        done(n);
    };
    save->signal_clicked().connect(finish);
    entry->signal_activate().connect(finish);
    buttons->append(*cancel);
    buttons->append(*save);
    box->append(*buttons);
    w->set_child(*box);
    w->present();
    entry->grab_focus();
}

void ask_unsaved(Gtk::Window& parent, const std::string& what, std::function<void()> save,
                 std::function<void()> discard) {
    auto d = Gtk::AlertDialog::create(tr("dialog.unsaved", {what}));
    d->set_detail(tr("dialog.unsaved_detail"));
    d->set_buttons({tr("common.cancel"), tr("dialog.discard"), tr("common.save")});
    d->set_cancel_button(0);
    d->set_default_button(2);
    d->choose(parent, [d, save, discard](const Glib::RefPtr<Gio::AsyncResult>& r) {
        try {
            const int b = d->choose_finish(r);
            if (b == 2) save();
            if (b == 1) discard();
        } catch (const Glib::Error&) {
        }
    });
}

}  // namespace dp::ui
