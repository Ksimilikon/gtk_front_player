#include "util/Paths.hpp"

#include <glib.h>

#include "config.h"

namespace dp::paths {

static constexpr const char* kAppDir = "dream_player";  // the TUI's directory

fs::path app_dir() { return fs::path(g_get_user_config_dir()) / kAppDir; }
fs::path mods_dir() { return app_dir() / "mods"; }
fs::path settings_file() { return app_dir() / "gtk.conf"; }
fs::path cache_dir() { return fs::path(g_get_user_cache_dir()) / kAppDir; }

fs::path default_music_dir() {
    if (const char* m = g_get_user_special_dir(G_USER_DIRECTORY_MUSIC)) return m;
    return fs::path(g_get_home_dir()) / "Music";
}

fs::path resource_dir() {
    std::error_code ec;
    if (const char* env = g_getenv("DREAM_PLAYER_GTK_DATA"); env && fs::is_directory(env, ec)) return env;
    if (fs::is_directory(fs::path(DP_INSTALL_DATA_DIR) / "lang", ec)) return DP_INSTALL_DATA_DIR;
    return DP_SOURCE_DATA_DIR;
}

void ensure_dirs() {
    std::error_code ec;
    for (const auto& d : {app_dir(), mods_dir(), cache_dir()}) fs::create_directories(d, ec);
    // the first layout of this frontend kept its own directories.
    const fs::path old_settings = fs::path(g_get_user_config_dir()) / "dream_player_gtk" / "settings.conf";
    if (fs::exists(old_settings, ec) && !fs::exists(settings_file(), ec)) fs::copy_file(old_settings, settings_file(), ec);
    const fs::path old_mods = fs::path(g_get_user_data_dir()) / "dream_player_gtk" / "mods";
    for (const auto& e : fs::directory_iterator(old_mods, ec))
        if (!fs::exists(mods_dir() / e.path().filename(), ec)) fs::copy(e.path(), mods_dir() / e.path().filename(), ec);
}

fs::path unique_path(const fs::path& dir, const std::string& name) {
    fs::path candidate = dir / name;
    std::error_code ec;
    if (!fs::exists(candidate, ec)) return candidate;
    const fs::path p(name);
    const std::string stem = p.stem().string(), ext = p.extension().string();
    for (int i = 2;; ++i) {
        candidate = dir / (stem + " (" + std::to_string(i) + ")" + ext);
        if (!fs::exists(candidate, ec)) return candidate;
    }
}

}  // namespace dp::paths
