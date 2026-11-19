#include "ui/widgets/TrackSort.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#include "core/Library.hpp"
#include "i18n/I18n.hpp"

namespace dp::ui {

TrackSort::TrackSort() : Gtk::DropDown(std::vector<Glib::ustring>{
        tr("sort.title"), tr("sort.file"), tr("sort.artists"), tr("sort.db_added"), tr("sort.db_updated"),
        tr("sort.file_modified"), tr("sort.file_created")}) {
    set_tooltip_text(tr("sort.tooltip"));
}

std::pair<int64_t, int64_t> TrackSort::file_times(int64_t id) {
    if (auto it = times_.find(id); it != times_.end()) return it->second;
    std::pair<int64_t, int64_t> out{0, 0};
    if (const auto* t = Library::get().track(id); t && !t->path.empty()) {
        struct statx sx{};
        if (statx(AT_FDCWD, t->path.c_str(), 0, STATX_MTIME | STATX_BTIME | STATX_CTIME, &sx) == 0) {
            out.first = sx.stx_mtime.tv_sec;
            // the birth time when the filesystem keeps it, else the change time.
            out.second = (sx.stx_mask & STATX_BTIME) ? sx.stx_btime.tv_sec : sx.stx_ctime.tv_sec;
        }
    }
    times_[id] = out;
    return out;
}

int TrackSort::compare(int64_t a, int64_t b) {
    const auto* ta = Library::get().track(a);
    const auto* tb = Library::get().track(b);
    if (!ta || !tb) return 0;
    auto cmp_str = [](const std::string& x, const std::string& y) {
        return g_utf8_collate(x.c_str(), y.c_str());
    };
    auto cmp_num = [](int64_t x, int64_t y) { return x < y ? -1 : x > y ? 1 : 0; };
    switch (static_cast<Sort>(get_selected())) {
        case ByTitle: return cmp_str(ta->display_title(), tb->display_title());
        case ByFile:
            return cmp_str(std::filesystem::path(ta->path).filename().string(),
                           std::filesystem::path(tb->path).filename().string());
        case ByArtists: return cmp_str(ta->artists_text(), tb->artists_text());
        // the core doesn't expose the db dates yet: the id grows with every
        // added song, so it orders by "added to the library".
        case ByDbAdded:
        case ByDbUpdated: return cmp_num(a, b);
        case ByFileModified: return cmp_num(file_times(a).first, file_times(b).first);
        case ByFileCreated: return cmp_num(file_times(a).second, file_times(b).second);
    }
    return 0;
}

}  // namespace dp::ui
