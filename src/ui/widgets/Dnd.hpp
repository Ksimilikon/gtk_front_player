// drag-and-drop inside the app's lists. The payload is a string:
//   "row:<index>"  — a row of the same list (reordering),
//   "track:<id>"   — a library song (from a pool / another list).
// Drop targets highlight the row under the pointer (`drop-above` /
// `drop-below`) and report where the drop landed.
#pragma once

#include <gtkmm/widget.h>

#include <functional>
#include <string>

namespace dp::ui {

/// makes `w` draggable; `payload` is taken when the drag starts.
void drag_source(Gtk::Widget& w, std::function<std::string()> payload);

/// `w` (a row) accepts drops; `on_drop(payload, after)` — `after` is true
/// when the pointer was in the lower half of the row. Returns accepted.
void drop_on_row(Gtk::Widget& w, std::function<bool(const std::string& payload, bool after)> on_drop);

/// `w` (a list / an empty area) accepts drops at its end.
void drop_on_area(Gtk::Widget& w, std::function<bool(const std::string& payload)> on_drop);

/// "row:12" → 12 for prefix "row:"; -1 if the payload has another prefix.
long payload_num(const std::string& payload, const std::string& prefix);

}  // namespace dp::ui
