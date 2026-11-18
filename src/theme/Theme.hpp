// themes: a theme is a text file `<resources>/themes/<id>.theme` with color
// tokens (`accent = #1ed760`); the stylesheet `themes/style.css` is a template
// where `{{token}}` is replaced with the theme value. Custom-drawn widgets
// (waves, seek bar) read the tokens through `Theme::color`.
//
// required tokens (a missing one falls back to the built-in dark theme):
//   bg, surface, surface_alt, border, text, muted, accent, accent_text,
//   warning, error
#pragma once

#include <gdkmm/rgba.h>
#include <gtkmm/cssprovider.h>
#include <sigc++/signal.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace dp {

struct ThemeInfo {
    std::string id;
    std::string name;
};

class Theme {
public:
    static Theme& get();

    /// loads and applies theme `id` to the default display (unknown → dark).
    void apply(const std::filesystem::path& dir, const std::string& id);

    Gdk::RGBA color(const std::string& token) const;
    const std::string& id() const { return id_; }
    std::vector<ThemeInfo> available() const;

    /// the theme changed (redraw custom widgets).
    sigc::signal<void()>& signal_changed() { return changed_; }

private:
    static std::map<std::string, std::string> parse(const std::filesystem::path& file);
    std::filesystem::path dir_;
    std::string id_;
    std::map<std::string, std::string> tokens_;
    Glib::RefPtr<Gtk::CssProvider> provider_;
    sigc::signal<void()> changed_;
};

}  // namespace dp
