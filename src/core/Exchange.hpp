// packet import / export (`cdc_*`): `.dplr` files. Blocking functions —
// call them from a background thread (see `run_async`).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dp::exchange {

inline constexpr const char* kExtension = ".dplr";

struct Flags {
    bool audio = true;
    bool names = true;
    bool covers = true;
};

/// what to export: the whole library, playlists by name, or tracks by id.
struct Selection {
    enum Kind { Library, Playlists, Tracks } kind = Library;
    std::vector<std::string> playlists;
    std::vector<int64_t> tracks;
};

struct Result {
    uint32_t status = 0;  // FFICdcStatus (0 = ok), or a negative-ish io marker
    bool io_error = false;
    std::string message;  // io error text
    uint64_t bytes = 0;
    size_t songs = 0, covers = 0, failed = 0;
};

/// packs `what` and writes it to `file`.
Result export_to_file(const Selection& what, const Flags& flags, const std::string& file);

/// imports `file` into the library: songs and covers are written into
/// `target_dir` (unique names), then the import is committed.
Result import_file(const std::string& file, bool force, const std::string& target_dir);

/// `dream_player_<unix-time>.dplr`.
std::string default_file_name();

}  // namespace dp::exchange
