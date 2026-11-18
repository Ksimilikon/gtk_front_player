#include "ui/tabs/ModPageTab.hpp"

#include <glibmm/bytes.h>
#include <glibmm/main.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/entry.h>
#include <gtkmm/listbox.h>
#include <gtkmm/picture.h>
#include <sigc++/adaptors/track_obj.h>

#include <cstring>
#include <filesystem>

#include "core/Core.hpp"
#include "core/Mods.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/widgets/Common.hpp"

namespace dp::ui {

namespace {

// FNV-1a over everything the mod may change (except input buffers, which the
// frontend writes itself).
struct Hasher {
    uint64_t h = 1469598103934665603ull;
    void bytes(const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    }
    template <typename T>
    void val(const T& v) { bytes(&v, sizeof v); }
    void str(const char* s) {
        if (s) bytes(s, std::strlen(s));
        val<uint8_t>(0xff);
    }
    void strs(const char* const* items, size_t len) {
        val(len);
        for (size_t i = 0; items && i < len; ++i) str(items[i]);
    }
};

void hash_elements(Hasher& h, const ModElement* els, size_t len) {
    h.val(len);
    for (size_t i = 0; els && i < len; ++i) {
        const ModElement& e = els[i];
        h.val(e.kind);
        switch (e.kind) {
            case MOD_EL_TEXT: h.str(e.text.text); break;
            case MOD_EL_IMAGE: h.val(e.image.data); h.val(e.image.len); break;
            case MOD_EL_INPUT: h.str(e.input.hint); h.val(e.input.cap); break;
            case MOD_EL_BUTTON: h.str(e.button.title); break;
            case MOD_EL_LIST: h.strs(e.list.items, e.list.len); h.val(e.list.on_select != nullptr); break;
            case MOD_EL_BOX: h.val(e.box.horizontal); hash_elements(h, e.box.children, e.box.len); break;
            case MOD_EL_INFO: h.strs(e.info.strings, e.info.len); break;
            case MOD_EL_TOGGLE: h.str(e.toggle.title); h.val(e.toggle.on); break;
            case MOD_EL_SELECT:
                h.str(e.select.title);
                h.strs(e.select.items, e.select.len);
                h.val(e.select.selected);
                break;
        }
    }
}

}  // namespace

ModPageTab::ModPageTab(std::string mod_path, uint32_t tab_id) : path_(std::move(mod_path)), id_(tab_id) {
    title_ = tr("mods.tab_n", {std::to_string(id_ + 1)});
    body_.add_css_class("form-page");
    scroll_.set_child(body_);
    scroll_.set_vexpand(true);
    append(scroll_);
    refresh();
    Glib::signal_timeout().connect(sigc::track_obj(sigc::mem_fun(*this, &ModPageTab::refresh), *this), 500);
}

bool ModPageTab::refresh() {
    const ModTab* tab = Mods::get().tab(path_, id_);
    Hasher h;
    if (tab) {
        h.str(tab->title);
        hash_elements(h, tab->elements, tab->len);
    } else {
        h.val<uint64_t>(0xdead);
    }
    if (h.h != signature_) {
        signature_ = h.h;
        rebuild(tab);
    }
    return true;
}

void ModPageTab::rebuild(const ModTab* tab) {
    while (auto* c = body_.get_first_child()) body_.remove(*c);
    if (!tab) {
        body_.append(*banner(tr("mods.not_running"), true));
        return;
    }
    const std::string t = core::str(tab->title);
    if (!t.empty() && t != title_) {
        title_ = t;
        signal_title_changed.emit();
    }
    for (size_t i = 0; tab->elements && i < tab->len; ++i)
        if (auto* w = render(tab->elements[i], {i})) body_.append(*w);
}

const ModElement* ModPageTab::resolve(const IndexPath& at) const {
    const ModTab* tab = Mods::get().tab(path_, id_);
    if (!tab || at.empty()) return nullptr;
    const ModElement* els = tab->elements;
    size_t len = tab->len;
    const ModElement* el = nullptr;
    for (size_t depth = 0; depth < at.size(); ++depth) {
        if (!els || at[depth] >= len) return nullptr;
        el = &els[at[depth]];
        if (depth + 1 < at.size()) {
            if (el->kind != MOD_EL_BOX) return nullptr;
            els = el->box.children;
            len = el->box.len;
        }
    }
    return el;
}

void ModPageTab::activate(const IndexPath& at, uint64_t target) {
    if (const ModElement* el = resolve(at)) Mods::get().activate(path_, el, target);
    // the mod changes its state in place (or refuses the change): re-read
    // and rebuild in any case, so a refused toggle snaps back.
    defer([this] {
        signature_ = 0;
        refresh();
    });
}

Gtk::Widget* ModPageTab::render(const ModElement& el, const IndexPath& at) {
    switch (el.kind) {
        case MOD_EL_TEXT: {
            auto* l = make_label(core::str(el.text.text), {}, true);
            l->set_selectable(true);
            return l;
        }
        case MOD_EL_IMAGE: {
            if (!el.image.data || !el.image.len) return nullptr;
            try {
                auto tex = Gdk::Texture::create_from_bytes(Glib::Bytes::create(el.image.data, el.image.len));
                auto* pic = Gtk::make_managed<Gtk::Picture>(tex);
                pic->set_content_fit(Gtk::ContentFit::CONTAIN);
                pic->set_size_request(-1, std::min(240, tex->get_height()));
                pic->set_halign(Gtk::Align::START);
                return pic;
            } catch (const Glib::Error&) {
                return make_label(tr("mods.bad_image"), "muted");
            }
        }
        case MOD_EL_INPUT: {
            auto* e = Gtk::make_managed<Gtk::Entry>();
            e->set_placeholder_text(core::str(el.input.hint));
            if (el.input.buf) e->set_text(el.input.buf);
            e->signal_changed().connect([this, e, at] {
                // the frontend writes the typed text into the mod's buffer.
                const ModElement* cur = resolve(at);
                if (!cur || cur->kind != MOD_EL_INPUT || !cur->input.buf || cur->input.cap == 0) return;
                std::string text = e->get_text();
                size_t n = std::min(text.size(), cur->input.cap - 1);
                while (n > 0 && n < text.size() && (static_cast<unsigned char>(text[n]) & 0xC0) == 0x80) --n;
                std::memcpy(cur->input.buf, text.data(), n);
                cur->input.buf[n] = '\0';
            });
            return e;
        }
        case MOD_EL_BUTTON: {
            auto* b = Gtk::make_managed<Gtk::Button>(core::str(el.button.title));
            b->set_halign(Gtk::Align::START);
            b->signal_clicked().connect([this, at] { activate(at, 0); });
            return b;
        }
        case MOD_EL_LIST: {
            auto* list = Gtk::make_managed<Gtk::ListBox>();
            list->add_css_class("card");
            list->set_selection_mode(Gtk::SelectionMode::NONE);
            for (size_t i = 0; el.list.items && i < el.list.len; ++i) {
                auto* l = make_label(core::str(el.list.items[i]), {}, true);
                l->set_margin(6);
                list->append(*l);
            }
            if (el.list.on_select)
                list->signal_row_activated().connect([this, at](Gtk::ListBoxRow* row) {
                    activate(at, static_cast<uint64_t>(row->get_index()));
                });
            return list;
        }
        case MOD_EL_BOX: {
            auto* box = Gtk::make_managed<Gtk::Box>(
                el.box.horizontal ? Gtk::Orientation::HORIZONTAL : Gtk::Orientation::VERTICAL, 8);
            for (size_t i = 0; el.box.children && i < el.box.len; ++i) {
                IndexPath child = at;
                child.push_back(i);
                if (auto* w = render(el.box.children[i], child)) box->append(*w);
            }
            return box;
        }
        case MOD_EL_INFO: {
            // a convention between mods and frontends: paths of songs the
            // frontend can play.
            auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
            for (size_t i = 0; el.info.strings && i < el.info.len; ++i) {
                const std::string s = core::str(el.info.strings[i]);
                std::error_code ec;
                if (std::filesystem::is_regular_file(s, ec)) {
                    auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
                    auto* play = icon_button("media-playback-start-symbolic", tr("action.play"));
                    play->signal_clicked().connect([s] { Player::get().play_path(s); });
                    row->append(*play);
                    row->append(*make_label(std::filesystem::path(s).filename().string()));
                    box->append(*row);
                } else {
                    box->append(*make_label(s, "muted", true));
                }
            }
            return box;
        }
        case MOD_EL_TOGGLE: {
            auto* c = Gtk::make_managed<Gtk::CheckButton>(core::str(el.toggle.title));
            c->set_active(el.toggle.on);
            c->signal_toggled().connect([this, c, at] { activate(at, c->get_active() ? 1 : 0); });
            return c;
        }
        case MOD_EL_SELECT: {
            auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
            box->append(*make_label(core::str(el.select.title)));
            std::vector<Glib::ustring> items;
            for (size_t i = 0; el.select.items && i < el.select.len; ++i) items.push_back(core::str(el.select.items[i]));
            auto* dd = Gtk::make_managed<Gtk::DropDown>(items);
            dd->set_selected(el.select.selected < el.select.len ? static_cast<guint>(el.select.selected)
                                                                : GTK_INVALID_LIST_POSITION);
            dd->property_selected().signal_changed().connect([this, dd, at] {
                if (dd->get_selected() != GTK_INVALID_LIST_POSITION) activate(at, dd->get_selected());
            });
            box->append(*dd);
            return box;
        }
    }
    return make_label(tr("mods.unknown_element"), "muted");
}

}  // namespace dp::ui
