#include "theme/Theme.hpp"

#include <gdkmm/display.h>
#include <gtkmm/settings.h>
#include <gtkmm/stylecontext.h>

#include <fstream>
#include <sstream>

#include "util/Format.hpp"
#include "util/Log.hpp"

namespace dp {

namespace fs = std::filesystem;

// the built-in dark theme: black with a green accent.
static const std::map<std::string, std::string> kDark = {
    {"name", "Dark"},          {"bg", "#000000"},      {"surface", "#0c0c0c"},     {"surface_alt", "#161616"},
    {"border", "#232323"},     {"text", "#e9e9e9"},    {"muted", "#8b8b8b"},       {"accent", "#1ed760"},
    {"accent_text", "#000000"}, {"warning", "#f5c400"}, {"error", "#ff4d4f"},
};

Theme& Theme::get() {
    static Theme t;
    return t;
}

std::map<std::string, std::string> Theme::parse(const fs::path& file) {
    std::map<std::string, std::string> out;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        const std::string t = fmt::trim(line);
        if (t.empty() || t[0] == '#') continue;
        const auto eq = t.find('=');
        if (eq == std::string::npos) continue;
        out[fmt::trim(t.substr(0, eq))] = fmt::trim(t.substr(eq + 1));
    }
    return out;
}

void Theme::apply(const fs::path& dir, const std::string& id) {
    dir_ = dir;
    std::error_code ec;
    id_ = fs::exists(dir / (id + ".theme"), ec) ? id : "dark";
    tokens_ = kDark;
    for (auto& [k, v] : parse(dir / (id_ + ".theme"))) tokens_[k] = v;

    std::ifstream in(dir / "style.css");
    std::stringstream ss;
    ss << in.rdbuf();
    std::string css = ss.str();
    if (css.empty()) Log::get().warn("theme", "style.css not found in " + dir.string());
    for (size_t pos = css.find("{{"); pos != std::string::npos; pos = css.find("{{", pos)) {
        const auto end = css.find("}}", pos);
        if (end == std::string::npos) break;
        const std::string token = css.substr(pos + 2, end - pos - 2);
        const auto it = tokens_.find(token);
        const std::string value = it != tokens_.end() ? it->second : "#ff00ff";
        css.replace(pos, end - pos + 2, value);
        pos += value.size();
    }

    auto display = Gdk::Display::get_default();
    if (!provider_) {
        provider_ = Gtk::CssProvider::create();
        provider_->signal_parsing_error().connect([](const Glib::RefPtr<const Gtk::CssSection>& section,
                                                     const Glib::Error& e) {
            Log::get().warn("theme", "css:" + std::to_string(section->get_start_location().get_lines() + 1) + ": " +
                                         e.what());
        });
        Gtk::StyleProvider::add_provider_for_display(display, provider_,
                                                     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    // the base GTK theme in its dark variant, so whatever the stylesheet
    // doesn't cover still looks dark.
    auto settings = Gtk::Settings::get_for_display(display);
#if GTK_CHECK_VERSION(4, 20, 0)
    g_object_set(settings->gobj(), "gtk-interface-color-scheme", GTK_INTERFACE_COLOR_SCHEME_DARK, nullptr);
#else
    settings->property_gtk_application_prefer_dark_theme() = true;
#endif
    provider_->load_from_string(css);
    changed_.emit();
}

Gdk::RGBA Theme::color(const std::string& token) const {
    const auto it = tokens_.find(token);
    if (it != tokens_.end()) return Gdk::RGBA(it->second);
    const auto d = kDark.find(token);
    return Gdk::RGBA(d != kDark.end() ? d->second : "#ff00ff");
}

std::vector<ThemeInfo> Theme::available() const {
    std::vector<ThemeInfo> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir_, ec)) {
        if (e.path().extension() != ".theme") continue;
        const auto t = parse(e.path());
        const auto name = t.find("name");
        out.push_back({e.path().stem().string(), name != t.end() ? name->second : e.path().stem().string()});
    }
    if (out.empty()) out.push_back({"dark", "Dark"});
    return out;
}

}  // namespace dp
