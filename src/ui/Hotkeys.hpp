// keyboard shortcuts: a fixed set of actions, each with a configurable
// accelerator (GTK accelerator syntax, e.g. `<Control>Left`) that can be
// switched off. Stored in the settings (`hotkey.<id>`, `hotkey.<id>.on`).
//
// Matching ignores the keyboard layout: a key that gives a non-Latin symbol
// in the current layout (e.g. Cyrillic) is re-read in the other layouts until
// a Latin one is found, so Ctrl+W works on a Russian layout too.
#pragma once

#include <gdkmm/display.h>
#include <gdkmm/enums.h>
#include <sigc++/signal.h>

#include <optional>
#include <string>
#include <vector>

namespace dp::ui {

enum class Action {
    PlayPause,
    VolumeUp,
    VolumeDown,
    NextTrack,
    PrevTrack,
    TrackVolumeUp,
    TrackVolumeDown,
    SeekForward,
    SeekBack,
    Save,
    CloseTab,
    NextTab,
    PrevTab,
    NewPlaylist,
    Reindex,
    OpenSettings,
    Quit,
};

struct ActionInfo {
    Action action;
    const char* id;            // settings key part
    const char* accel;         // default accelerator
    const char* label_key;     // i18n key
    bool in_text;              // also fires while typing in a text field
};

class Hotkeys {
public:
    static Hotkeys& get();
    static const std::vector<ActionInfo>& actions();

    std::string accel(Action a) const;  // "" — unbound
    bool enabled(Action a) const;
    void set_accel(Action a, const std::string& accel);
    void set_enabled(Action a, bool on);
    void reset(Action a);

    /// steps of the arrow actions.
    double volume_step() const;       // 0..1
    double track_volume_step() const; // 0..2 scale
    int seek_step() const;            // seconds

    /// the action bound to this key press, if any. `in_text` — the focus is
    /// in a text field (only actions marked `in_text` fire then).
    std::optional<Action> match(Gdk::Display& display, guint keyval, guint keycode, Gdk::ModifierType state,
                                bool in_text) const;
    /// the event as an accelerator string (layout-independent), "" if it is
    /// only a modifier.
    static std::string accel_of(Gdk::Display& display, guint keyval, guint keycode, Gdk::ModifierType state);
    /// a human-readable label of an accelerator ("Ctrl+Left").
    static std::string label(const std::string& accel);

    /// set while the settings capture a new shortcut: nothing fires.
    bool capturing = false;

    sigc::signal<void()>& signal_changed() { return changed_; }

private:
    static const ActionInfo& info(Action a);
    sigc::signal<void()> changed_;
};

}  // namespace dp::ui
