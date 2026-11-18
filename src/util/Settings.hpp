// app settings: a plain `key = value` text file in the config dir. A key may
// repeat (lists). Saved on every change.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dp {

class Settings {
public:
    static Settings& get();

    void load(const std::filesystem::path& file);

    std::string str(const std::string& key, const std::string& def = {}) const;
    bool boolean(const std::string& key, bool def) const;
    double number(const std::string& key, double def) const;
    std::vector<std::string> list(const std::string& key) const;

    void set(const std::string& key, const std::string& value);
    void set_bool(const std::string& key, bool value) { set(key, value ? "true" : "false"); }
    void set_number(const std::string& key, double value);
    void set_list(const std::string& key, const std::vector<std::string>& values);

private:
    void save() const;
    std::filesystem::path file_;
    std::vector<std::pair<std::string, std::string>> entries_;  // file order
};

// keys
namespace key {
inline constexpr const char* language = "language";        // "auto" | code
inline constexpr const char* theme = "theme";              // theme id
inline constexpr const char* music_dir = "music_dir";
inline constexpr const char* enabled_mod = "enabled_mod";  // repeated
inline constexpr const char* output_device = "output_device";
inline constexpr const char* repeat_one = "repeat_one";
inline constexpr const char* shuffle = "shuffle";
inline constexpr const char* window_width = "window_width";
inline constexpr const char* window_height = "window_height";
inline constexpr const char* sync_port = "sync_port";
inline constexpr const char* sync_address = "sync_address";
}  // namespace key

}  // namespace dp
