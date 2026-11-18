// where the app keeps its files (XDG) and where its resources lie.
#pragma once

#include <filesystem>
#include <string>

namespace dp::paths {

namespace fs = std::filesystem;

/// $XDG_CONFIG_HOME/dream_player_gtk — settings, the core config.
fs::path config_dir();
/// $XDG_DATA_HOME/dream_player_gtk — the db, keys, `mods/`.
fs::path data_dir();
/// $XDG_CACHE_HOME/dream_player_gtk.
fs::path cache_dir();
/// the system music directory (XDG_MUSIC_DIR, else ~/Music).
fs::path default_music_dir();
/// `<data>/mods` — the core loads dynamic mods from here on init.
fs::path mods_dir();

/// read-only resources (lang/, themes/, licenses): $DREAM_PLAYER_GTK_DATA,
/// the installed share dir, or the source tree (dev builds).
fs::path resource_dir();

/// creates the app directories.
void ensure_dirs();

/// `dir/name`, or `dir/name (2).ext`... — a path that doesn't exist yet.
fs::path unique_path(const fs::path& dir, const std::string& name);

}  // namespace dp::paths
