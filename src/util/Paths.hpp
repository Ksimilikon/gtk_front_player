// where the app keeps its files and where its resources lie.
//
// The app shares its directory with the core's TUI: `~/.config/dream_player`
// ($XDG_CONFIG_HOME/dream_player) holds the library db (`music_db.sqlite`),
// the device keys, the core `config.toml`, the mods (`mods/`) and the GTK
// settings (`gtk.conf`) — so both frontends see the same library.
#pragma once

#include <filesystem>
#include <string>

namespace dp::paths {

namespace fs = std::filesystem;

/// `~/.config/dream_player` — the app directory (shared with the TUI). The
/// core gets it as its cache, config and data directory, as the TUI does.
fs::path app_dir();
/// `<app>/mods` — the core loads dynamic mods from `<data>/mods` on init.
fs::path mods_dir();
/// `<app>/gtk.conf` — settings of this frontend.
fs::path settings_file();
/// `~/.cache/dream_player` — throwaway files of this frontend (MPRIS cover).
fs::path cache_dir();
/// the system music directory (XDG_MUSIC_DIR, else ~/Music).
fs::path default_music_dir();

/// read-only resources (lang/, themes/, licenses): $DREAM_PLAYER_GTK_DATA,
/// the installed share dir, or the source tree (dev builds).
fs::path resource_dir();

/// creates the app directories; moves files of the old layout
/// (`~/.config/dream_player_gtk`, `~/.local/share/dream_player_gtk/mods`).
void ensure_dirs();

/// `dir/name`, or `dir/name (2).ext`... — a path that doesn't exist yet.
fs::path unique_path(const fs::path& dir, const std::string& name);

}  // namespace dp::paths
