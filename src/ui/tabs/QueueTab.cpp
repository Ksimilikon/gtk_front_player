#include "ui/tabs/QueueTab.hpp"

#include <gtkmm/adjustment.h>
#include <gtkmm/button.h>

#include "core/Library.hpp"
#include "core/Player.hpp"
#include "i18n/I18n.hpp"
#include "ui/Dialogs.hpp"
#include "ui/Menus.hpp"
#include "ui/widgets/Dnd.hpp"
#include "ui/widgets/TrackRow.hpp"
#include "util/Format.hpp"
#include "util/Log.hpp"

namespace dp::ui {

std::string QueueTab::title() const {
    const std::string& n = Player::get().queue_name();
    return (n.empty() ? tr("player.queue") : n) + "*";
}

QueueTab::QueueTab() {
    auto* head = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 18);
    head->add_css_class("page-header");
    auto* info = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    info->set_hexpand(true);
    title_.add_css_class("page-title");
    title_.set_xalign(0);
    title_.set_ellipsize(Pango::EllipsizeMode::END);
    stats_.add_css_class("muted");
    stats_.set_xalign(0);
    info->append(title_);
    info->append(stats_);
    info->append(*make_label(tr("queue.hint"), "muted", true));
    head->append(*info);
    auto* buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    buttons->set_valign(Gtk::Align::CENTER);
    auto* save = Gtk::make_managed<Gtk::Button>(tr("queue.save"));
    save->add_css_class("suggested-action");
    save->signal_clicked().connect(sigc::mem_fun(*this, &QueueTab::save_as_playlist));
    buttons->append(*save);
    head->append(*buttons);
    append(*head);

    list_.add_css_class("plain-list");
    list_.set_selection_mode(Gtk::SelectionMode::NONE);
    list_.signal_row_activated().connect([](Gtk::ListBoxRow* row) {
        if (auto* r = dynamic_cast<TrackRow*>(row)) Player::get().jump(static_cast<size_t>(r->number() - 1));
    });
    scroll_.set_child(list_);
    scroll_.set_vexpand(true);
    append(scroll_);

    Player::get().signal_queue_changed().connect(sigc::mem_fun(*this, &QueueTab::schedule_rebuild));
    Player::get().signal_track_changed().connect(sigc::mem_fun(*this, &QueueTab::mark_current));
    Library::get().signal_changed().connect(sigc::mem_fun(*this, &QueueTab::schedule_rebuild));
    rebuild();
}

void QueueTab::on_closed() {
    // closing the queue tab stops playback and empties the queue.
    Player::get().clear();
}

// many queue changes arrive in a row (start + play): rebuild once.
void QueueTab::schedule_rebuild() {
    if (rebuild_pending_) return;
    rebuild_pending_ = true;
    defer([this] {
        rebuild_pending_ = false;
        rebuild();
    });
}

void QueueTab::rebuild() {
    auto& p = Player::get();
    title_.set_text(title());
    signal_title_changed.emit();
    std::vector<int64_t> ids;
    for (const auto& e : p.entries())
        if (e.item.id) ids.push_back(e.item.id);
    stats_.set_text(tr("playlists.stats", {std::to_string(p.entries().size()),
                                           fmt::duration(Library::get().total_duration(ids))}));

    const double scroll = scroll_.get_vadjustment()->get_value();
    while (auto* c = list_.get_first_child()) list_.remove(*c);
    for (size_t i = 0; i < p.entries().size(); ++i) {
        const auto& e = p.entries()[i];
        core::Track t;
        if (const auto* lt = e.item.id ? Library::get().track(e.item.id) : nullptr) {
            t = *lt;
        } else {
            t.id = e.item.id;
            t.path = e.item.path;
        }
        auto* row = Gtk::make_managed<TrackRow>(t, static_cast<int>(i + 1));
        const int64_t id = e.item.id;
        on_context_menu(*row, [row, i, id] {
            std::vector<MenuItem> items;
            items.push_back({tr("action.play"), [i] { Player::get().jump(i); }});
            items.push_back({tr("queue.remove"), [i] { Player::get().remove_at(i); }, "destructive"});
            if (id) append_song_items(items, *row, id);
            return items;
        });
        drag_source(*row, [i] { return "row:" + std::to_string(i); });
        drop_on_row(*row, [this, i](const std::string& payload, bool after) {
            const long from = payload_num(payload, "row:");
            if (from < 0) return false;
            size_t to = i + (after ? 1 : 0);
            if (to > static_cast<size_t>(from)) --to;
            defer([from, to] { Player::get().move(static_cast<size_t>(from), to); });
            return true;
        });
        list_.append(*row);
    }
    mark_current();
    defer([this, scroll] { scroll_.get_vadjustment()->set_value(scroll); });
}

void QueueTab::mark_current() {
    const auto& p = Player::get();
    for (auto* w = list_.get_first_child(); w; w = w->get_next_sibling())
        if (auto* r = dynamic_cast<TrackRow*>(w))
            r->set_playing(p.state().has_track && static_cast<size_t>(r->number() - 1) == p.position());
}

void QueueTab::save_as_playlist() {
    auto* w = window_of(*this);
    if (!w) return;
    ask_playlist_name(*w, tr("queue.save"), Player::get().queue_name(), [](const std::string& name) {
        std::vector<int64_t> ids;
        for (const auto& e : Player::get().entries())
            if (e.item.id) ids.push_back(e.item.id);
        const auto* existing = Library::get().playlist(name);
        if (Library::get().save_playlist(name, ids, existing ? existing->cover : "") == 0)
            Log::get().ok("ui", tr("editor.saved", {name}));
    });
}

}  // namespace dp::ui
