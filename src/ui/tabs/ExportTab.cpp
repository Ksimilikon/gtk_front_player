#include "ui/tabs/ExportTab.hpp"

#include <gtkmm/scrolledwindow.h>

#include <algorithm>

#include "core/Core.hpp"
#include "core/Library.hpp"
#include "i18n/I18n.hpp"
#include "ui/widgets/Common.hpp"
#include "util/Format.hpp"
#include "util/Log.hpp"

namespace dp::ui {

using core::Domain;

std::string ExportTab::title() const { return tr("page.export"); }

ExportTab::ExportTab() {
    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_vexpand(true);
    auto* page = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    page->add_css_class("form-page");
    page->append(*make_label(tr("export.intro"), "muted", true));

    page->append(*section_title(tr("export.what")));
    what_ = Gtk::make_managed<Gtk::DropDown>(
        std::vector<Glib::ustring>{tr("export.library"), tr("export.playlists"), tr("export.tracks")});
    what_->set_halign(Gtk::Align::START);
    what_->property_selected().signal_changed().connect(sigc::mem_fun(*this, &ExportTab::update));
    page->append(*what_);
    playlists_.add_css_class("card");
    page->append(playlists_);
    tracks_label_.add_css_class("muted");
    tracks_label_.set_xalign(0);
    page->append(tracks_label_);

    page->append(*section_title(tr("export.options")));
    audio_.set_label(tr("export.audio"));
    audio_.set_tooltip_text(tr("export.audio_hint"));
    audio_.set_active(true);
    names_.set_label(tr("export.names"));
    names_.set_active(true);
    covers_.set_label(tr("export.covers"));
    covers_.set_active(true);
    page->append(audio_);
    page->append(names_);
    page->append(covers_);

    auto* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    export_.set_label(tr("export.run"));
    export_.add_css_class("suggested-action");
    export_.signal_clicked().connect([this] {
        if (auto* w = window_of(*this))
            save_file(*w, tr("export.run"), exchange::default_file_name(),
                      [this](const std::string& f) { run(f); });
    });
    buttons->append(export_);
    buttons->append(spinner_);
    page->append(*buttons);
    result_.set_xalign(0);
    result_.set_wrap(true);
    page->append(result_);

    scroll->set_child(*page);
    append(*scroll);
    Library::get().signal_changed().connect(sigc::mem_fun(*this, &ExportTab::fill_playlists));
    fill_playlists();
    update();
}

void ExportTab::fill_playlists() {
    std::vector<std::string> checked;
    for (const auto& [n, c] : checks_)
        if (c->get_active()) checked.push_back(n);
    while (auto* c = playlists_.get_first_child()) playlists_.remove(*c);
    checks_.clear();
    for (const auto& p : Library::get().playlists()) {
        auto* c = Gtk::make_managed<Gtk::CheckButton>(p.name + "  (" + std::to_string(p.tracks.size()) + ")");
        c->set_active(std::find(checked.begin(), checked.end(), p.name) != checked.end());
        c->signal_toggled().connect(sigc::mem_fun(*this, &ExportTab::update));
        playlists_.append(*c);
        checks_.push_back({p.name, c});
    }
    if (checks_.empty()) playlists_.append(*make_label(tr("playlists.empty"), "muted"));
}

void ExportTab::preset_playlists(const std::vector<std::string>& names) {
    for (auto& [n, c] : checks_) c->set_active(std::find(names.begin(), names.end(), n) != names.end());
    what_->set_selected(1);
    update();
}

void ExportTab::preset_tracks(const std::vector<int64_t>& ids) {
    tracks_ = ids;
    what_->set_selected(2);
    update();
}

void ExportTab::update() {
    const guint w = what_->get_selected();
    playlists_.set_visible(w == 1);
    tracks_label_.set_visible(w == 2);
    std::vector<std::string> names;
    for (const auto& t : tracks_)
        if (const auto* tr_ = Library::get().track(t)) names.push_back(tr_->display_title());
    tracks_label_.set_text(tracks_.empty() ? tr("export.no_tracks")
                                           : tr("export.tracks_chosen", {std::to_string(tracks_.size())}) + ": " +
                                                 fmt::join(names, ", "));
    bool any = true;
    if (w == 1) any = std::any_of(checks_.begin(), checks_.end(), [](const auto& p) { return p.second->get_active(); });
    if (w == 2) any = !tracks_.empty();
    export_.set_sensitive(any);
}

void ExportTab::run(const std::string& file) {
    exchange::Selection sel;
    sel.kind = static_cast<exchange::Selection::Kind>(what_->get_selected());
    for (const auto& [n, c] : checks_)
        if (c->get_active()) sel.playlists.push_back(n);
    sel.tracks = tracks_;
    exchange::Flags flags{audio_.get_active(), names_.get_active(), covers_.get_active()};
    std::string path = file;
    if (std::filesystem::path(path).extension() != exchange::kExtension) path += exchange::kExtension;

    export_.set_sensitive(false);
    spinner_.set_spinning(true);
    result_.set_text("");
    run_async(
        alive_, [sel, flags, path] { return exchange::export_to_file(sel, flags, path); },
        [this, path](exchange::Result r) {
            spinner_.set_spinning(false);
            export_.set_sensitive(true);
            result_.remove_css_class("error-text");
            result_.remove_css_class("ok-text");
            if (r.io_error) {
                result_.set_text(tr("export.err.write", {r.message}));
                result_.add_css_class("error-text");
            } else if (r.status != 0) {
                result_.set_text(tr("export.err.core", {core::status_text(Domain::Cdc, r.status)}));
                result_.add_css_class("error-text");
            } else {
                result_.set_text(tr("export.done", {path, fmt::bytes(r.bytes)}));
                result_.add_css_class("ok-text");
            }
            Log::get().add(r.io_error || r.status ? Level::Error : Level::Ok, "export", result_.get_text());
        });
}

}  // namespace dp::ui
