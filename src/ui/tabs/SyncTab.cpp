#include "ui/tabs/SyncTab.hpp"

#include <gtkmm/adjustment.h>
#include <gtkmm/button.h>
#include <gtkmm/paned.h>
#include <sigc++/adaptors/track_obj.h>

#include <ctime>

#include "core/Core.hpp"
#include "core/Library.hpp"
#include "i18n/I18n.hpp"
#include "ui/widgets/Common.hpp"
#include "util/Format.hpp"
#include "util/Settings.hpp"

namespace dp::ui {

using core::Domain;

std::string SyncTab::title() const { return tr("page.sync"); }

static Gtk::Box* labeled(const std::string& name, Gtk::Widget& w) {
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    auto* l = make_label(name, "muted");
    l->set_size_request(150, -1);
    box->append(*l);
    w.set_hexpand(true);
    box->append(w);
    return box;
}

SyncTab::SyncTab() {
    auto* paned = Gtk::make_managed<Gtk::Paned>(Gtk::Orientation::HORIZONTAL);
    paned->set_vexpand(true);

    // ── left: the session form + log ──
    auto* left = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    left->add_css_class("form-page");
    left->append(*make_label(tr("sync.intro"), "muted", true));
    const std::string fp = device_fingerprint();
    left->append(*info_row(tr("sync.this_device_label"), fp.empty() ? "—" : fp, true));

    mode_ = Gtk::make_managed<Gtk::DropDown>(
        std::vector<Glib::ustring>{tr("sync.mode_sync"), tr("sync.mode_receive"), tr("sync.mode_send")});
    left->append(*labeled(tr("sync.mode"), *mode_));
    transport_ = Gtk::make_managed<Gtk::DropDown>(std::vector<Glib::ustring>{
        tr("sync.tr_listen"), tr("sync.tr_connect"), tr("sync.tr_adb_connect"), tr("sync.tr_adb_listen")});
    left->append(*labeled(tr("sync.transport"), *transport_));
    port_.set_range(1, 65535);
    port_.set_increments(1, 100);
    port_.set_value(Settings::get().number(key::sync_port, kSyncPort));
    left->append(*labeled(tr("sync.port"), port_));
    address_.set_placeholder_text("192.168.1.5[:47800]");
    address_.set_text(Settings::get().str(key::sync_address));
    address_row_ = labeled(tr("sync.address"), address_);
    left->append(*address_row_);
    force_.set_label(tr("sync.force"));
    force_.set_tooltip_text(tr("sync.force_hint"));
    left->append(force_);

    auto* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    start_ = Gtk::make_managed<Gtk::Button>(tr("sync.start"));
    start_->add_css_class("suggested-action");
    start_->signal_clicked().connect(sigc::mem_fun(*this, &SyncTab::start));
    cancel_ = Gtk::make_managed<Gtk::Button>(tr("common.cancel"));
    cancel_->signal_clicked().connect([this] {
        if (worker_) worker_->cancel();
    });
    buttons->append(*start_);
    buttons->append(*cancel_);
    buttons->append(spinner_);
    left->append(*buttons);

    left->append(*section_title(tr("sync.session")));
    log_.add_css_class("log-view");
    log_scroll_.set_child(log_);
    log_scroll_.set_vexpand(true);
    log_scroll_.set_min_content_height(160);
    left->append(log_scroll_);
    paned->set_start_child(*left);

    // ── right: paired devices ──
    auto* right = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    right->add_css_class("form-page");
    right->append(*section_title(tr("sync.paired_title")));
    right->append(*make_label(tr("sync.paired_hint"), "muted", true));
    auto* pscroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    pscroll->set_child(paired_);
    pscroll->set_vexpand(true);
    right->append(*pscroll);
    paned->set_end_child(*right);
    paned->set_position(560);
    append(*paned);

    mode_->property_selected().signal_changed().connect(sigc::mem_fun(*this, &SyncTab::update_controls));
    transport_->property_selected().signal_changed().connect(sigc::mem_fun(*this, &SyncTab::update_controls));
    update_controls();
    fill_paired();
}

void SyncTab::update_controls() {
    const bool running = worker_ && worker_->running();
    address_row_->set_visible(transport_->get_selected() == 1);
    force_.set_visible(mode_->get_selected() == 1);
    start_->set_sensitive(!running);
    cancel_->set_sensitive(running);
    mode_->set_sensitive(!running);
    transport_->set_sensitive(!running);
    spinner_.set_spinning(running);
}

void SyncTab::add_line(Level lv, const std::string& text) {
    static const char* classes[] = {"log-info", "log-ok", "log-warn", "log-error"};
    auto* l = make_label(fmt::clock_time(std::time(nullptr)) + "  " + text, classes[static_cast<int>(lv)], true);
    l->set_selectable(true);
    log_.append(*l);
    defer([this] {
        auto adj = log_scroll_.get_vadjustment();
        adj->set_value(adj->get_upper());
    });
    Log::get().add(lv, "sync", text);
}

void SyncTab::start() {
    if (worker_ && worker_->running()) return;
    while (auto* c = log_.get_first_child()) log_.remove(*c);
    SyncTarget target;
    target.kind = static_cast<SyncTarget::Kind>(transport_->get_selected());
    target.port = static_cast<uint16_t>(port_.get_value_as_int());
    target.address = fmt::trim(address_.get_text());
    if (target.kind == SyncTarget::TcpConnect && target.address.empty()) {
        add_line(Level::Warn, tr("sync.err.no_address"));
        return;
    }
    Settings::get().set_number(key::sync_port, target.port);
    if (!target.address.empty()) Settings::get().set(key::sync_address, target.address);

    SyncMode mode;
    mode.kind = static_cast<SyncMode::Kind>(mode_->get_selected());
    mode.force = force_.get_active();
    mode.what.kind = exchange::Selection::Library;

    const std::string music = Settings::get().str(key::music_dir, core::dirs().music.string());
    worker_ = std::make_unique<SyncWorker>(target, mode, music,
                                           [this](const SyncMessage& m) { on_message(m); });
    update_controls();
}

void SyncTab::on_message(const SyncMessage& m) {
    switch (m.kind) {
        case SyncMessage::Text: add_line(m.level, m.text); break;
        case SyncMessage::LibraryChanged: Library::get().reload(); break;
        case SyncMessage::Finished:
            add_line(m.complete ? Level::Ok : Level::Warn, tr(m.complete ? "sync.finished_ok" : "sync.finished"));
            // the worker sets its flag right after this message is queued.
            Glib::signal_timeout().connect_once(sigc::track_obj([this] {
                update_controls();
                fill_paired();
            }, *this), 100);
            break;
    }
}

void SyncTab::fill_paired() {
    while (auto* c = paired_.get_first_child()) paired_.remove(*c);
    FFIConPairedList l{};
    if (uint32_t st = con_get_paired(&l); st != 0) {
        paired_.append(*make_label(core::status_text(Domain::Con, st), "error-text", true));
        return;
    }
    struct P {
        int64_t id;
        std::string name, fp;
    };
    std::vector<P> list;
    for (size_t i = 0; i < l.len; ++i)
        list.push_back({l.ptr[i].id, core::str(l.ptr[i].name), fmt::fingerprint(l.ptr[i].pub_key)});
    con_free_paired_list(l);
    if (list.empty()) paired_.append(*make_label(tr("sync.no_paired"), "muted"));
    for (const auto& p : list) {
        auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
        row->add_css_class("card");
        auto* text = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
        text->set_hexpand(true);
        auto* name = Gtk::make_managed<Gtk::Entry>();
        name->set_text(p.name);
        name->set_tooltip_text(tr("sync.rename_hint"));
        const int64_t id = p.id;
        name->signal_activate().connect([this, id, name] {
            const std::string n = fmt::trim(name->get_text());
            if (n.empty()) return;
            if (uint32_t st = con_rename(id, n.c_str()); st != 0)
                add_line(Level::Error, core::status_text(Domain::Con, st));
        });
        text->append(*name);
        text->append(*make_label("#" + std::to_string(p.id) + "  " + p.fp, "muted"));
        row->append(*text);
        auto* forget = icon_button("user-trash-symbolic", tr("sync.forget"));
        forget->signal_clicked().connect([this, id, n = p.name] {
            if (auto* w = window_of(*this))
                confirm(*w, tr("sync.confirm_forget", {n}), tr("sync.forget"), [this, id] {
                    if (uint32_t st = con_forget(id); st != 0)
                        add_line(Level::Error, core::status_text(Domain::Con, st));
                    fill_paired();
                });
        });
        row->append(*forget);
        paired_.append(*row);
    }
}

}  // namespace dp::ui
