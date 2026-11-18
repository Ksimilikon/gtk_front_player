#include "i18n/I18n.hpp"

#include <glib.h>

#include <algorithm>
#include <fstream>

#include "util/Format.hpp"

namespace dp {

namespace fs = std::filesystem;

I18n& I18n::get() {
    static I18n i;
    return i;
}

static std::string unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[++i];
            out += n == 'n' ? '\n' : n == 't' ? '\t' : n;
        } else {
            out += s[i];
        }
    }
    return out;
}

std::unordered_map<std::string, std::string> I18n::parse(const fs::path& file) {
    std::unordered_map<std::string, std::string> out;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        const std::string t = fmt::trim(line);
        if (t.empty() || t[0] == '#') continue;
        const auto eq = t.find('=');
        if (eq == std::string::npos) continue;
        out[fmt::trim(t.substr(0, eq))] = unescape(fmt::trim(t.substr(eq + 1)));
    }
    return out;
}

/// the system language code: "ru_RU.UTF-8" → "ru".
static std::string system_language() {
    for (const char* const* n = g_get_language_names(); *n; ++n) {
        std::string l = *n;
        if (l == "C" || l == "POSIX") continue;
        const auto cut = l.find_first_of("_.@");
        return cut == std::string::npos ? l : l.substr(0, cut);
    }
    return "en";
}

void I18n::load(const fs::path& dir, const std::string& code) {
    dir_ = dir;
    code_ = (code.empty() || code == "auto") ? system_language() : code;
    std::error_code ec;
    if (!fs::exists(dir / (code_ + ".lang"), ec)) code_ = "en";
    fallback_ = parse(dir / "en.lang");
    strings_ = code_ == "en" ? fallback_ : parse(dir / (code_ + ".lang"));
}

const std::string& I18n::tr(const std::string& key) const {
    if (auto it = strings_.find(key); it != strings_.end()) return it->second;
    if (auto it = fallback_.find(key); it != fallback_.end()) return it->second;
    return key;
}

std::string I18n::tr(const std::string& key, std::initializer_list<std::string> args) const {
    std::string s = tr(key);
    size_t i = 0;
    for (const auto& a : args) {
        const std::string ph = "{" + std::to_string(i++) + "}";
        for (size_t pos = s.find(ph); pos != std::string::npos; pos = s.find(ph, pos + a.size()))
            s.replace(pos, ph.size(), a);
    }
    return s;
}

std::vector<LanguageInfo> I18n::available() const {
    std::vector<LanguageInfo> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir_, ec)) {
        if (e.path().extension() != ".lang") continue;
        const auto strings = parse(e.path());
        const auto name = strings.find("@name");
        const std::string code = e.path().stem().string();
        out.push_back({code, name != strings.end() ? name->second : code});
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.code < b.code; });
    return out;
}

}  // namespace dp
