// the queue being played: its tab is titled with the playlist name and a `*`.
// Songs are in play order, reordered by drag-and-drop; the queue can be saved
// as a playlist. Closing the tab stops playback and empties the queue.
#pragma once

#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>

#include "ui/Tab.hpp"

namespace dp::ui {

class QueueTab : public Tab {
public:
    QueueTab();
    static constexpr const char* kKey = "queue";
    std::string key() const override { return kKey; }
    std::string title() const override;
    void on_closed() override;

private:
    void schedule_rebuild();
    void rebuild();
    void mark_current();
    void save_as_playlist();

    Gtk::Label title_, stats_;
    Gtk::ScrolledWindow scroll_;
    Gtk::ListBox list_;
    bool rebuild_pending_ = false;
};

}  // namespace dp::ui
