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

struct FontInfo {
    std::string id;       // "pixel" | "system"
    std::string name_key; // i18n key
};

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
    /// the value from the theme file itself (without the user's override).
    std::string theme_value(const std::string& token) const;

    /// every color token, in display order.
    static const std::vector<std::string>& tokens();
    /// the user's palette: an override of a token (settings `color.<token>`),
    /// applied over any theme; "" removes it. Re-applies the theme.
    void set_override(const std::string& token, const std::string& value);
    void reset_overrides();
    const std::string& id() const { return id_; }
    std::vector<ThemeInfo> available() const;

    /// the UI fonts: the bundled pixel font (default) or the system one.
    static const std::vector<FontInfo>& fonts();
    /// registers the fonts shipped in `<resources>/fonts` for this process.
    static void register_fonts(const std::filesystem::path& dir);
    /// switches the font (settings `font`) and re-applies the theme.
    void set_font(const std::string& id);
    std::string font() const;

    /// the theme changed (redraw custom widgets).
    sigc::signal<void()>& signal_changed() { return changed_; }

private:
    static std::map<std::string, std::string> parse(const std::filesystem::path& file);
    std::filesystem::path dir_;
    std::string id_;
    std::map<std::string, std::string> tokens_, theme_tokens_;
    Glib::RefPtr<Gtk::CssProvider> provider_;
    sigc::signal<void()> changed_;
};

}  // namespace dp
