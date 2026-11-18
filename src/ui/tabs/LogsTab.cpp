#include "ui/tabs/LogsTab.hpp"

#include <gdkmm/clipboard.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/button.h>

#include "core/Core.hpp"
#include "i18n/I18n.hpp"
#include "ui/widgets/Common.hpp"
#include "util/Format.hpp"

namespace dp::ui {

std::string LogsTab::title() const { return tr("page.logs"); }

namespace {
class LogRow : public Gtk::ListBoxRow {
public:
    Level level;
    std::string text;
};
}  // namespace

LogsTab::LogsTab() {
    auto* bar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    bar->add_css_class("toolbar");
    filter_ = Gtk::make_managed<Gtk::DropDown>(
        std::vector<Glib::ustring>{tr("logs.all"), tr("logs.warnings"), tr("logs.errors")});
    filter_->property_selected().signal_changed().connect([this] { list_.invalidate_filter(); });
    bar->append(*filter_);
    auto* spacer = Gtk::make_managed<Gtk::Box>();
    spacer->set_hexpand(true);
    bar->append(*spacer);
    auto* copy = Gtk::make_managed<Gtk::Button>(tr("logs.copy"));
    copy->signal_clicked().connect([this] {
        std::string all;
        for (const auto& e : Log::get().entries())
            all += fmt::date_time(e.time) + " [" + e.source + "] " + e.text + "\n";
        if (auto panic = core::last_panic(); !panic.empty()) all += "core panic: " + panic + "\n";
        get_clipboard()->set_text(all);
    });
    bar->append(*copy);
    auto* clear = Gtk::make_managed<Gtk::Button>(tr("logs.clear"));
    clear->signal_clicked().connect([] { Log::get().clear(); });
    bar->append(*clear);
    append(*bar);

    list_.add_css_class("plain-list");
    list_.set_selection_mode(Gtk::SelectionMode::NONE);
    list_.set_filter_func([this](Gtk::ListBoxRow* row) {
        auto* r = dynamic_cast<LogRow*>(row);
        if (!r) return true;
        switch (filter_->get_selected()) {
            case 1: return r->level == Level::Warn || r->level == Level::Error;
            case 2: return r->level == Level::Error;
            default: return true;
        }
    });
    scroll_.set_child(list_);
    scroll_.set_vexpand(true);
    append(scroll_);

    Log::get().signal_added().connect(sigc::mem_fun(*this, &LogsTab::add));
    Log::get().signal_cleared().connect(sigc::mem_fun(*this, &LogsTab::reload));
    reload();
}

void LogsTab::reload() {
    while (auto* c = list_.get_first_child()) list_.remove(*c);
    for (const auto& e : Log::get().entries()) add(e);
    if (auto panic = core::last_panic(); !panic.empty())
        add({0, Level::Error, "core", tr("logs.core_panic", {panic})});
}

void LogsTab::add(const LogEntry& e) {
    static const char* classes[] = {"log-info", "log-ok", "log-warn", "log-error"};
    static const char* names[] = {"INFO", "OK", "WARN", "ERROR"};
    auto* row = Gtk::make_managed<LogRow>();
    row->level = e.level;
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    box->add_css_class("log-row");
    box->append(*make_label(e.time ? fmt::clock_time(e.time) : "--:--:--", "muted"));
    auto* lv = make_label(names[static_cast<int>(e.level)], classes[static_cast<int>(e.level)]);
    lv->set_size_request(52, -1);
    box->append(*lv);
    auto* src = make_label(e.source, "muted");
    src->set_size_request(64, -1);
    box->append(*src);
    auto* text = make_label(e.text, {}, true);
    text->set_hexpand(true);
    text->set_selectable(true);
    box->append(*text);
    row->set_child(*box);
    list_.append(*row);
    // stick to the bottom if already there.
    auto adj = scroll_.get_vadjustment();
    if (adj->get_value() + adj->get_page_size() >= adj->get_upper() - 40)
        defer([this] {
            auto a = scroll_.get_vadjustment();
            a->set_value(a->get_upper());
        });
}

}  // namespace dp::ui
