#include "ui/tabs/AboutTab.hpp"

#include <gtkmm/expander.h>
#include <gtkmm/linkbutton.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>

#include <fstream>
#include <sstream>

#include "config.h"
#include "i18n/I18n.hpp"
#include "ui/widgets/Common.hpp"
#include "util/Paths.hpp"

namespace dp::ui {

static constexpr const char* kAppRepo = "https://github.com/ksimilikon/gtk_front_player";
static constexpr const char* kCoreRepo = "https://github.com/ksimilikon/rs_dream_player";
static constexpr const char* kAndroidRepo = "https://github.com/ksimilikon/android_front_player";
static constexpr const char* kAuthor = "https://github.com/ksimilikon";

std::string AboutTab::title() const { return tr("page.about"); }

static Gtk::Widget* link(const std::string& text, const std::string& url) {
    auto* b = Gtk::make_managed<Gtk::LinkButton>(url, text);
    b->set_halign(Gtk::Align::START);
    b->set_tooltip_text(url);
    return b;
}

static Gtk::Widget* bullet(const std::string& text) {
    auto* row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    row->append(*make_label("•", "accent"));
    auto* l = make_label(text, {}, true);
    l->set_hexpand(true);
    row->append(*l);
    return row;
}

static Gtk::Widget* license(const std::string& title, const std::string& file) {
    auto* exp = Gtk::make_managed<Gtk::Expander>(title);
    std::ifstream in(paths::resource_dir() / file);
    std::stringstream ss;
    ss << in.rdbuf();
    auto* view = Gtk::make_managed<Gtk::TextView>();
    view->set_editable(false);
    view->set_monospace(true);
    view->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    view->get_buffer()->set_text(ss.str().empty() ? tr("about.license_missing") : ss.str());
    view->add_css_class("license-text");
    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_child(*view);
    scroll->set_min_content_height(260);
    exp->set_child(*scroll);
    return exp;
}

AboutTab::AboutTab() {
    auto* scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_vexpand(true);
    auto* page = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    page->add_css_class("form-page");

    auto* name = make_label("Dream Player", "about-title");
    page->append(*name);
    page->append(*make_label(tr("about.tagline"), "muted", true));

    page->append(*section_title(tr("about.features")));
    for (int i = 1;; ++i) {
        const std::string key = "about.feature." + std::to_string(i);
        const std::string& text = tr(key);
        if (text == key) break;
        page->append(*bullet(text));
    }

    page->append(*section_title(tr("about.versions")));
    page->append(*info_row(tr("about.app"), DP_APP_VERSION));
    page->append(*info_row(tr("about.core"), DP_CORE_VERSION));
    page->append(*info_row(tr("about.gtk"), std::to_string(gtk_get_major_version()) + "." +
                                                std::to_string(gtk_get_minor_version()) + "." +
                                                std::to_string(gtk_get_micro_version())));

    page->append(*section_title(tr("about.repos")));
    page->append(*link(tr("about.repo_app"), kAppRepo));
    page->append(*link(tr("about.repo_core"), kCoreRepo));
    page->append(*link(tr("about.repo_android"), kAndroidRepo));

    page->append(*section_title(tr("about.creators")));
    page->append(*link("ksimilikon", kAuthor));
    page->append(*bullet(tr("about.ai")));

    page->append(*section_title(tr("about.licenses")));
    page->append(*license(tr("about.license_app"), "LICENSE-app.txt"));
    page->append(*license(tr("about.license_core"), "LICENSE-core.txt"));
    page->append(*bullet(tr("about.license_gtk")));

    scroll->set_child(*page);
    append(*scroll);
}

}  // namespace dp::ui
