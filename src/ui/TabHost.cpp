#include "ui/TabHost.hpp"

#include <gtkmm/button.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/label.h>

#include <algorithm>

#include "i18n/I18n.hpp"
#include "ui/Nav.hpp"

namespace dp::ui {

static Nav* g_nav = nullptr;
Nav& nav() { return *g_nav; }
void set_nav(Nav* n) { g_nav = n; }

TabHost::TabHost() : Gtk::Box(Gtk::Orientation::VERTICAL, 0) {
    bar_.add_css_class("tab-bar");
    scroller_.set_policy(Gtk::PolicyType::EXTERNAL, Gtk::PolicyType::NEVER);
    scroller_.set_hexpand(true);
    scroller_.set_child(buttons_);
    buttons_.set_margin_start(6);
    bar_.append(scroller_);
    stack_.set_transition_type(Gtk::StackTransitionType::CROSSFADE);
    stack_.set_transition_duration(120);
    stack_.set_vexpand(true);
    stack_.set_hexpand(true);
    stack_.property_visible_child().signal_changed().connect([this] {
        update_headers();
        if (auto* t = current()) t->on_activated();
    });
    append(bar_);
    append(stack_);
}

TabHost::~TabHost() {
    for (auto& e : tabs_) stack_.remove(*e.page);
}

void TabHost::set_end_widget(Gtk::Widget& w) { bar_.append(w); }

Gtk::Box* TabHost::make_header(Tab& tab) {
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
    box->add_css_class("tab-button");
    auto* label = Gtk::make_managed<Gtk::Label>(tab.title());
    label->set_ellipsize(Pango::EllipsizeMode::END);
    label->set_max_width_chars(22);
    box->append(*label);
    const std::string key = tab.key();
    if (tab.closable()) {
        auto* close_btn = Gtk::make_managed<Gtk::Button>();
        close_btn->set_icon_name("window-close-symbolic");
        close_btn->add_css_class("flat");
        close_btn->add_css_class("tab-close");
        close_btn->set_focusable(false);
        close_btn->set_tooltip_text(tr("tabs.close"));
        close_btn->signal_clicked().connect([this, key] { close(key); });
        box->append(*close_btn);
    }
    auto click = Gtk::GestureClick::create();
    click->set_button(0);
    click->signal_released().connect([this, key, closable = tab.closable(), c = click.get()](int, double, double) {
        if (c->get_current_button() == GDK_BUTTON_MIDDLE) {
            if (closable) close(key);
        } else {
            focus(key);
        }
    });
    box->add_controller(click);
    tab.signal_title_changed.connect([label, &tab] { label->set_text(tab.title()); });
    tab.signal_close_request.connect([this, key] { Glib::signal_idle().connect_once([this, key] { close(key); }); });
    return box;
}

Tab* TabHost::open(const std::string& key, const std::function<std::unique_ptr<Tab>()>& make) {
    if (Tab* t = find(key)) {
        focus(key);
        return t;
    }
    auto page = make();
    Tab* raw = page.get();
    Entry e;
    e.header = make_header(*raw);
    buttons_.append(*e.header);
    stack_.add(*raw, key);
    e.page = std::move(page);
    tabs_.push_back(std::move(e));
    focus(key);
    return raw;
}

Tab* TabHost::find(const std::string& key) const {
    for (const auto& e : tabs_)
        if (e.page->key() == key) return e.page.get();
    return nullptr;
}

void TabHost::focus(const std::string& key) {
    if (find(key)) stack_.set_visible_child(key);
    update_headers();
}

Tab* TabHost::current() const { return dynamic_cast<Tab*>(const_cast<Gtk::Stack&>(stack_).get_visible_child()); }

void TabHost::close(const std::string& key) {
    const auto it = std::find_if(tabs_.begin(), tabs_.end(), [&](const Entry& e) { return e.page->key() == key; });
    if (it == tabs_.end() || !it->page->closable()) return;
    const bool was_current = current() == it->page.get();
    const size_t idx = static_cast<size_t>(it - tabs_.begin());
    buttons_.remove(*it->header);
    stack_.remove(*it->page);
    tabs_.erase(it);
    if (was_current && !tabs_.empty()) focus(tabs_[std::min(idx, tabs_.size() - 1)].page->key());
    update_headers();
}

void TabHost::close_current() {
    if (auto* t = current()) close(t->key());
}

void TabHost::cycle(int dir) {
    if (tabs_.empty()) return;
    const Tab* cur = current();
    size_t i = 0;
    for (; i < tabs_.size(); ++i)
        if (tabs_[i].page.get() == cur) break;
    const long n = static_cast<long>(tabs_.size());
    const long next = ((static_cast<long>(i) + dir) % n + n) % n;
    focus(tabs_[static_cast<size_t>(next)].page->key());
}

void TabHost::update_headers() {
    const Tab* cur = current();
    for (auto& e : tabs_) {
        if (e.page.get() == cur)
            e.header->add_css_class("active");
        else
            e.header->remove_css_class("active");
    }
}

}  // namespace dp::ui
