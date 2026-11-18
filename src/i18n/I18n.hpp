// localization from plain text files: `<resources>/lang/<code>.lang`.
//
// file format (UTF-8):
//   # comment
//   @name = Русский          — meta: the language's own name
//   key.sub = Text with {0} and {1} placeholders
//   multi = line one\nline two  — `\n` is a line break, `\\` a backslash
//
// lookup: the chosen language → English (`en.lang`) → the key itself. The
// language is chosen once at startup (settings `language`, or `auto` from the
// system locale); switching takes effect after a restart.
#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

namespace dp {

struct LanguageInfo {
    std::string code;  // file stem: "ru", "en"
    std::string name;  // `@name` of the file
};

class I18n {
public:
    static I18n& get();

    /// loads `code` (`auto` — from LANG / LC_MESSAGES) from `dir`.
    void load(const std::filesystem::path& dir, const std::string& code);

    const std::string& tr(const std::string& key) const;
    std::string tr(const std::string& key, std::initializer_list<std::string> args) const;

    const std::string& code() const { return code_; }
    /// every `*.lang` file in the directory.
    std::vector<LanguageInfo> available() const;

private:
    static std::unordered_map<std::string, std::string> parse(const std::filesystem::path& file);
    std::filesystem::path dir_;
    std::string code_ = "en";
    std::unordered_map<std::string, std::string> strings_, fallback_;
};

/// shortcuts.
inline const std::string& tr(const std::string& key) { return I18n::get().tr(key); }
inline std::string tr(const std::string& key, std::initializer_list<std::string> args) {
    return I18n::get().tr(key, args);
}

}  // namespace dp
