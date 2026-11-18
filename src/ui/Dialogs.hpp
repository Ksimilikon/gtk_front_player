// small modal dialogs of the app.
#pragma once

#include <gtkmm/window.h>

#include <functional>
#include <string>

namespace dp::ui {

/// chooses a playlist (search + list, or a new name); `done(name)`.
void pick_playlist(Gtk::Window& parent, const std::string& title, std::function<void(const std::string&)> done);

/// asks a playlist name to save into; under the field a red note says
/// whether the name overwrites an existing playlist or creates a new one.
void ask_playlist_name(Gtk::Window& parent, const std::string& title, const std::string& initial,
                       std::function<void(const std::string&)> done);

/// unsaved changes: `save` / `discard` (cancel does nothing).
void ask_unsaved(Gtk::Window& parent, const std::string& what, std::function<void()> save,
                 std::function<void()> discard);

}  // namespace dp::ui
