// the core mod system (pre-alpha, will change a lot): a thin layer over
// `mods_*`. Which mods are enabled is the frontend's state (settings
// `enabled_mod`); enabled mods are started on launch. Main thread only.
#pragma once

#include <sigc++/signal.h>

#include <string>
#include <vector>

#include "core/ffi.h"

namespace dp {

struct ModInfo {
    std::string path;  // identifier
    std::string name, version, author, description;
    FFIModState state = FFI_MOD_STOPPED;
    bool is_static = false;
    uint32_t error = 0;
    std::string error_msg;
    bool enabled = false;  // the user wants it running
};

/// a menu action of a running mod.
struct ModAction {
    std::string mod_path;
    std::string mod_name;
    uint32_t place = UI_PLACE_MENU;
    uint32_t index = 0;
    std::string title;
};

class Mods {
public:
    static Mods& get();

    /// installs the error listener and starts the enabled mods.
    void start();

    std::vector<ModInfo> list() const;
    uint32_t add(const std::string& file);
    uint32_t remove(const std::string& path);
    uint32_t rescan();
    /// starts / stops and remembers the choice.
    uint32_t set_enabled(const std::string& path, bool on);

    std::vector<ModAction> actions(uint32_t place) const;
    uint32_t call(const ModAction& a, uint64_t target);

    uint32_t tab_count(const std::string& path) const;
    /// tab `id` (a pointer into the mod's memory; valid while it runs).
    const ModTab* tab(const std::string& path, uint32_t id) const;
    uint32_t activate(const std::string& path, const ModElement* el, uint64_t target);

    /// the mod list / states / actions changed.
    sigc::signal<void()>& signal_changed() { return changed_; }

private:
    static void on_error(const char* path, uint32_t code, const char* msg);
    void report(const std::string& what, const std::string& path, uint32_t st);
    sigc::signal<void()> changed_;
};

}  // namespace dp
