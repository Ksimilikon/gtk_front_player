#pragma once

#include <gtkmm/dropdown.h>
#include <unordered_map>

namespace dp::ui {

class TrackSort : public Gtk::DropDown {
public:
    TrackSort();
    int compare(int64_t a, int64_t b);
    void clear_cache() { times_.clear(); }
    bool uses_added_date() const { return get_selected() == ByDbUpdated; }

private:
    enum Sort { ByTitle, ByFile, ByArtists, ByDbAdded, ByDbUpdated, ByFileModified, ByFileCreated };
    std::pair<int64_t, int64_t> file_times(int64_t id);
    std::unordered_map<int64_t, std::pair<int64_t, int64_t>> times_;
};

}  // namespace dp::ui
