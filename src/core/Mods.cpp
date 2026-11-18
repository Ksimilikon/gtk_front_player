#include "core/Mods.hpp"

#include <algorithm>

#include "core/Core.hpp"
#include "i18n/I18n.hpp"
#include "util/Log.hpp"
#include "util/MainThread.hpp"
#include "util/Settings.hpp"

namespace dp {

using core::Domain;

Mods& Mods::get() {
    static Mods m;
    return m;
}

// called on the mod's thread; the strings live only during the call.
void Mods::on_error(const char* path, uint32_t code, const char* msg) {
    const std::string p = core::str(path), m = core::str(msg);
    Log::get().error("mods", tr("mods.log.error", {p, core::status_text(Domain::Mod, code), m}));
    post([] { Mods::get().signal_changed().emit(); });
}

void Mods::report(const std::string& what, const std::string& path, uint32_t st) {
    if (st == 0)
        Log::get().ok("mods", tr(what, {path}));
    else
        Log::get().error("mods", tr(what, {path}) + ": " + core::status_text(Domain::Mod, st));
}

void Mods::start() {
    mods_set_error_listener(&Mods::on_error);
    const auto enabled = Settings::get().list(key::enabled_mod);
    for (const auto& m : list()) {
        if (std::find(enabled.begin(), enabled.end(), m.path) == enabled.end()) continue;
        report("mods.log.started", m.path, mods_start(m.path.c_str()));
    }
    changed_.emit();
}

std::vector<ModInfo> Mods::list() const {
    FFIModsList l{};
    std::vector<ModInfo> out;
    if (mods_get_list(&l) != 0) return out;
    const auto enabled = Settings::get().list(key::enabled_mod);
    for (size_t i = 0; i < l.len; ++i) {
        const FFIModInfo& m = l.ptr[i];
        ModInfo info;
        info.path = core::str(m.path);
        info.name = core::str(m.name);
        info.version = core::str(m.version);
        info.author = core::str(m.author);
        info.description = core::str(m.description);
        info.state = m.state;
        info.is_static = m.is_static;
        info.error = m.error;
        info.error_msg = core::str(m.error_msg);
        info.enabled = std::find(enabled.begin(), enabled.end(), info.path) != enabled.end();
        out.push_back(std::move(info));
    }
    mods_free_list(l);
    return out;
}

uint32_t Mods::add(const std::string& file) {
    const uint32_t st = mods_add(file.c_str());
    report("mods.log.added", file, st);
    changed_.emit();
    return st;
}

uint32_t Mods::remove(const std::string& path) {
    const uint32_t st = mods_remove(path.c_str());
    report("mods.log.removed", path, st);
    auto enabled = Settings::get().list(key::enabled_mod);
    std::erase(enabled, path);
    Settings::get().set_list(key::enabled_mod, enabled);
    changed_.emit();
    return st;
}

uint32_t Mods::rescan() {
    const uint32_t st = mods_rescan();
    if (st != 0) Log::get().error("mods", core::status_text(Domain::Mod, st));
    changed_.emit();
    return st;
}

uint32_t Mods::set_enabled(const std::string& path, bool on) {
    auto enabled = Settings::get().list(key::enabled_mod);
    std::erase(enabled, path);
    if (on) enabled.push_back(path);
    Settings::get().set_list(key::enabled_mod, enabled);
    uint32_t st;
    if (on) {
        st = mods_start(path.c_str());
        report("mods.log.started", path, st);
    } else {
        st = mods_stop(path.c_str());
        report("mods.log.stopped", path, st);
    }
    changed_.emit();
    return st;
}

std::vector<ModAction> Mods::actions(uint32_t place) const {
    std::vector<ModAction> out;
    for (const auto& m : list()) {
        if (m.state != FFI_MOD_RUNNING) continue;
        FFIStringList l{};
        if (mods_get_actions(m.path.c_str(), place, &l) != 0) continue;
        const auto titles = core::take_strings(l, mods_free_strings);
        for (uint32_t i = 0; i < titles.size(); ++i) out.push_back({m.path, m.name, place, i, titles[i]});
    }
    return out;
}

uint32_t Mods::call(const ModAction& a, uint64_t target) {
    const uint32_t st = mods_call_action(a.mod_path.c_str(), a.place, a.index, target);
    if (st != 0)
        Log::get().error("mods", tr("mods.log.action_failed", {a.title, core::status_text(Domain::Mod, st)}));
    changed_.emit();
    return st;
}

uint32_t Mods::tab_count(const std::string& path) const {
    uint32_t n = 0;
    if (mods_get_tab_count(path.c_str(), &n) != 0) return 0;
    return n;
}

const ModTab* Mods::tab(const std::string& path, uint32_t id) const {
    const ModTab* t = nullptr;
    if (mods_get_tab(path.c_str(), id, &t) != 0) return nullptr;
    return t;
}

uint32_t Mods::activate(const std::string& path, const ModElement* el, uint64_t target) {
    const uint32_t st = mods_activate(path.c_str(), el, target);
    if (st != 0) Log::get().error("mods", core::status_text(Domain::Mod, st));
    return st;
}

}  // namespace dp
