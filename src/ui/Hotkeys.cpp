#include "ui/Hotkeys.hpp"

#include <gtk/gtk.h>

#include "util/Settings.hpp"

namespace dp::ui {

static constexpr auto kMods = GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_ALT_MASK | GDK_SUPER_MASK;

Hotkeys& Hotkeys::get() {
    static Hotkeys h;
    return h;
}

const std::vector<ActionInfo>& Hotkeys::actions() {
    static const std::vector<ActionInfo> list = {
        {Action::PlayPause, "play_pause", "space", "hotkey.play_pause", false},
        {Action::VolumeUp, "volume_up", "Up", "hotkey.volume_up", false},
        {Action::VolumeDown, "volume_down", "Down", "hotkey.volume_down", false},
        {Action::NextTrack, "next_track", "Right", "hotkey.next_track", false},
        {Action::PrevTrack, "prev_track", "Left", "hotkey.prev_track", false},
        {Action::TrackVolumeUp, "track_volume_up", "<Control>Up", "hotkey.track_volume_up", false},
        {Action::TrackVolumeDown, "track_volume_down", "<Control>Down", "hotkey.track_volume_down", false},
        {Action::SeekForward, "seek_forward", "<Control>Right", "hotkey.seek_forward", false},
        {Action::SeekBack, "seek_back", "<Control>Left", "hotkey.seek_back", false},
        {Action::Save, "save", "<Control>s", "hotkey.save", true},
        {Action::CloseTab, "close_tab", "<Control>w", "hotkey.close_tab", true},
        {Action::NextTab, "next_tab", "<Control>Tab", "hotkey.next_tab", true},
        {Action::PrevTab, "prev_tab", "<Control><Shift>ISO_Left_Tab", "hotkey.prev_tab", true},
        {Action::NewPlaylist, "new_playlist", "<Control>n", "hotkey.new_playlist", true},
        {Action::Reindex, "reindex", "<Control>r", "hotkey.reindex", false},
        {Action::OpenSettings, "settings", "<Control>comma", "hotkey.settings", true},
        {Action::Quit, "quit", "<Control>q", "hotkey.quit", true},
    };
    return list;
}

const ActionInfo& Hotkeys::info(Action a) {
    for (const auto& i : actions())
        if (i.action == a) return i;
    return actions().front();
}

std::string Hotkeys::accel(Action a) const {
    const auto& i = info(a);
    return Settings::get().str(std::string("hotkey.") + i.id, i.accel);
}

bool Hotkeys::enabled(Action a) const {
    return Settings::get().boolean(std::string("hotkey.") + info(a).id + ".on", true);
}

void Hotkeys::set_accel(Action a, const std::string& accel) {
    Settings::get().set(std::string("hotkey.") + info(a).id, accel);
    changed_.emit();
}

void Hotkeys::set_enabled(Action a, bool on) {
    Settings::get().set_bool(std::string("hotkey.") + info(a).id + ".on", on);
    changed_.emit();
}

void Hotkeys::reset(Action a) {
    Settings::get().set(std::string("hotkey.") + info(a).id, info(a).accel);
    Settings::get().set_bool(std::string("hotkey.") + info(a).id + ".on", true);
    changed_.emit();
}

double Hotkeys::volume_step() const { return Settings::get().number("hotkey.volume_step", 5) / 100.0; }
double Hotkeys::track_volume_step() const { return Settings::get().number("hotkey.track_volume_step", 5) / 100.0; }
int Hotkeys::seek_step() const { return static_cast<int>(Settings::get().number("hotkey.seek_step", 5)); }

/// the keyval of a key press with the layout ignored: if the current layout
/// gives a non-Latin symbol, the other layouts are tried for a Latin one.
static guint layout_free_keyval(Gdk::Display& display, guint keyval, guint keycode, Gdk::ModifierType state) {
    const gunichar uc = gdk_keyval_to_unicode(keyval);
    if (uc == 0 || uc < 0x80) return keyval;
    for (int group = 0; group < 4; ++group) {
        guint kv = 0;
        if (gdk_display_translate_key(display.gobj(), keycode, static_cast<GdkModifierType>(state), group, &kv,
                                      nullptr, nullptr, nullptr)) {
            const gunichar u = gdk_keyval_to_unicode(kv);
            if (u != 0 && u < 0x80) return kv;
        }
    }
    return keyval;
}

static bool is_modifier(guint kv) {
    switch (kv) {
        case GDK_KEY_Control_L: case GDK_KEY_Control_R: case GDK_KEY_Shift_L: case GDK_KEY_Shift_R:
        case GDK_KEY_Alt_L: case GDK_KEY_Alt_R: case GDK_KEY_Super_L: case GDK_KEY_Super_R:
        case GDK_KEY_Meta_L: case GDK_KEY_Meta_R: case GDK_KEY_ISO_Level3_Shift: case GDK_KEY_Caps_Lock:
            return true;
        default: return false;
    }
}

std::string Hotkeys::accel_of(Gdk::Display& display, guint keyval, guint keycode, Gdk::ModifierType state) {
    const guint kv = gdk_keyval_to_lower(layout_free_keyval(display, keyval, keycode, state));
    if (is_modifier(kv)) return {};
    const auto mods = static_cast<GdkModifierType>(static_cast<guint>(state) & kMods);
    char* name = gtk_accelerator_name(kv, mods);
    std::string out = name ? name : "";
    g_free(name);
    return out;
}

std::string Hotkeys::label(const std::string& accel) {
    guint kv = 0;
    GdkModifierType mods{};
    if (accel.empty() || !gtk_accelerator_parse(accel.c_str(), &kv, &mods)) return {};
    char* l = gtk_accelerator_get_label(kv, mods);
    std::string out = l ? l : "";
    g_free(l);
    return out;
}

std::optional<Action> Hotkeys::match(Gdk::Display& display, guint keyval, guint keycode, Gdk::ModifierType state,
                                     bool in_text) const {
    if (capturing) return std::nullopt;
    const guint kv = gdk_keyval_to_lower(layout_free_keyval(display, keyval, keycode, state));
    const guint mods = static_cast<guint>(state) & kMods;
    for (const auto& i : actions()) {
        if (in_text && !i.in_text) continue;
        if (!enabled(i.action)) continue;
        guint akv = 0;
        GdkModifierType amods{};
        const std::string a = accel(i.action);
        if (a.empty() || !gtk_accelerator_parse(a.c_str(), &akv, &amods)) continue;
        if (gdk_keyval_to_lower(akv) == kv && (static_cast<guint>(amods) & kMods) == mods) return i.action;
    }
    return std::nullopt;
}

}  // namespace dp::ui
