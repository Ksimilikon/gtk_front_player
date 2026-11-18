// a tab declared by a running mod (`mod_sys::ui::Tab`): its elements are
// rendered with GTK widgets. The tab lives in the mod's memory and may change
// at any time, so it is re-read periodically and rebuilt when its content
// changed. Elements are addressed by their index path and re-resolved from a
// fresh tab pointer before every activation.
#pragma once

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>

#include <cstdint>
#include <vector>

#include "core/ffi.h"
#include "ui/Tab.hpp"

namespace dp::ui {

class ModPageTab : public Tab {
public:
    ModPageTab(std::string mod_path, uint32_t tab_id);
    static std::string key_for(const std::string& path, uint32_t id) {
        return "mod:" + path + ":" + std::to_string(id);
    }
    std::string key() const override { return key_for(path_, id_); }
    std::string title() const override { return title_; }

private:
    using IndexPath = std::vector<size_t>;
    bool refresh();
    void rebuild(const ModTab* tab);
    Gtk::Widget* render(const ModElement& el, const IndexPath& at);
    const ModElement* resolve(const IndexPath& at) const;
    void activate(const IndexPath& at, uint64_t target);

    std::string path_;
    uint32_t id_;
    std::string title_;
    uint64_t signature_ = 0;
    Gtk::ScrolledWindow scroll_;
    Gtk::Box body_{Gtk::Orientation::VERTICAL, 8};
};

}  // namespace dp::ui
