#include "ui/tabs/ImportTab.hpp"

#include "core/Core.hpp"
#include "core/Exchange.hpp"
#include "core/Library.hpp"
#include "i18n/I18n.hpp"
#include "ui/widgets/Common.hpp"
#include "util/Format.hpp"
#include "util/Log.hpp"
#include "util/Settings.hpp"

namespace dp::ui {

using core::Domain;

std::string ImportTab::title() const { return tr("page.import"); }

static std::string target_dir() { return Settings::get().str(key::music_dir, core::dirs().music.string()); }

ImportTab::ImportTab() {
    auto* page = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    page->add_css_class("form-page");
    page->append(*make_label(tr("import.intro"), "muted", true));

    page->append(*section_title(tr("import.file")));
    auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    file_label_.set_text(tr("import.no_file"));
    file_label_.add_css_class("muted");
    file_label_.set_hexpand(true);
    file_label_.set_xalign(0);
    file_label_.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
    row->append(file_label_);
    auto* pick = Gtk::make_managed<Gtk::Button>(tr("import.choose"));
    pick->signal_clicked().connect([this] {
        if (auto* w = window_of(*this))
            pick_file(*w, tr("import.choose"), {"*.dplr"}, tr("dialog.packets"), [this](const std::string& p) {
                file_ = p;
                file_label_.set_text(p);
                file_label_.remove_css_class("muted");
                import_.set_sensitive(true);
            });
    });
    row->append(*pick);
    page->append(*row);

    target_label_.set_text(tr("import.target", {target_dir()}));
    target_label_.add_css_class("muted");
    target_label_.set_xalign(0);
    target_label_.set_wrap(true);
    page->append(target_label_);

    force_.set_label(tr("import.force"));
    force_.set_tooltip_text(tr("import.force_hint"));
    page->append(force_);

    auto* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    import_.set_label(tr("import.run"));
    import_.add_css_class("suggested-action");
    import_.set_sensitive(false);
    import_.signal_clicked().connect(sigc::mem_fun(*this, &ImportTab::run));
    buttons->append(import_);
    buttons->append(spinner_);
    page->append(*buttons);

    result_.set_xalign(0);
    result_.set_wrap(true);
    page->append(result_);
    append(*page);
}

void ImportTab::run() {
    if (file_.empty()) return;
    import_.set_sensitive(false);
    spinner_.set_spinning(true);
    result_.set_text("");
    target_label_.set_text(tr("import.target", {target_dir()}));
    run_async(
        alive_, [file = file_, force = force_.get_active(), dir = target_dir()] {
            return exchange::import_file(file, force, dir);
        },
        [this](exchange::Result r) {
            spinner_.set_spinning(false);
            import_.set_sensitive(true);
            result_.remove_css_class("error-text");
            result_.remove_css_class("ok-text");
            if (r.io_error) {
                result_.set_text(tr("import.err.read", {r.message}));
                result_.add_css_class("error-text");
            } else if (r.status != 0) {
                result_.set_text(tr("import.err.core", {core::status_text(Domain::Cdc, r.status)}));
                result_.add_css_class("error-text");
            } else {
                result_.set_text(tr("import.done", {std::to_string(r.songs), std::to_string(r.covers),
                                                    fmt::bytes(r.bytes)}) +
                                 (r.failed ? "\n" + tr("import.failed_files", {std::to_string(r.failed)}) : ""));
                result_.add_css_class("ok-text");
            }
            Log::get().add(r.io_error || r.status ? Level::Error : Level::Ok, "import", result_.get_text());
            Library::get().reload();
        });
}

}  // namespace dp::ui
