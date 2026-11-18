#include "util/Settings.hpp"

#include <fstream>
#include <sstream>

#include "util/Format.hpp"

namespace dp {

Settings& Settings::get() {
    static Settings s;
    return s;
}

void Settings::load(const std::filesystem::path& file) {
    file_ = file;
    entries_.clear();
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        const std::string t = fmt::trim(line);
        if (t.empty() || t[0] == '#') continue;
        const auto eq = t.find('=');
        if (eq == std::string::npos) continue;
        entries_.emplace_back(fmt::trim(t.substr(0, eq)), fmt::trim(t.substr(eq + 1)));
    }
}

std::string Settings::str(const std::string& key, const std::string& def) const {
    for (const auto& [k, v] : entries_)
        if (k == key) return v;
    return def;
}

bool Settings::boolean(const std::string& key, bool def) const {
    const std::string v = str(key);
    if (v.empty()) return def;
    return v == "true" || v == "1" || v == "yes";
}

double Settings::number(const std::string& key, double def) const {
    const std::string v = str(key);
    if (v.empty()) return def;
    std::istringstream is(v);
    is.imbue(std::locale::classic());
    double d = def;
    is >> d;
    return is.fail() ? def : d;
}

std::vector<std::string> Settings::list(const std::string& key) const {
    std::vector<std::string> out;
    for (const auto& [k, v] : entries_)
        if (k == key) out.push_back(v);
    return out;
}

void Settings::set(const std::string& key, const std::string& value) {
    for (auto& [k, v] : entries_)
        if (k == key) {
            if (v == value) return;
            v = value;
            save();
            return;
        }
    entries_.emplace_back(key, value);
    save();
}

void Settings::set_number(const std::string& key, double value) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << value;
    set(key, os.str());
}

void Settings::set_list(const std::string& key, const std::vector<std::string>& values) {
    std::erase_if(entries_, [&](const auto& e) { return e.first == key; });
    for (const auto& v : values) entries_.emplace_back(key, v);
    save();
}

void Settings::save() const {
    if (file_.empty()) return;
    std::ofstream out(file_, std::ios::trunc);
    out << "# Dream Player GTK settings (key = value; a key may repeat)\n";
    for (const auto& [k, v] : entries_) out << k << " = " << v << '\n';
}

}  // namespace dp
