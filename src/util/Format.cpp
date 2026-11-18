#include "util/Format.hpp"

#include <glib.h>

#include <cstdio>
#include <ctime>

namespace dp::fmt {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        auto pos = s.find(sep, start);
        if (pos == std::string::npos) pos = s.size();
        std::string part = trim(s.substr(start, pos - start));
        if (!part.empty()) out.push_back(std::move(part));
        start = pos + 1;
    }
    return out;
}

std::string duration(uint64_t s) {
    char buf[32];
    if (s >= 3600)
        std::snprintf(buf, sizeof buf, "%llu:%02llu:%02llu", (unsigned long long)(s / 3600),
                      (unsigned long long)(s / 60 % 60), (unsigned long long)(s % 60));
    else
        std::snprintf(buf, sizeof buf, "%llu:%02llu", (unsigned long long)(s / 60), (unsigned long long)(s % 60));
    return buf;
}

std::string bytes(uint64_t n) {
    char* s = g_format_size(n);
    std::string out = s;
    g_free(s);
    return out;
}

bool contains_ci(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    char* h = g_utf8_casefold(haystack.c_str(), -1);
    char* n = g_utf8_casefold(needle.c_str(), -1);
    const bool found = std::string(h).find(n) != std::string::npos;
    g_free(h);
    g_free(n);
    return found;
}

std::string fingerprint(const uint8_t* k) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "%02x%02x %02x%02x %02x%02x %02x%02x", k[0], k[1], k[2], k[3], k[4], k[5], k[6],
                  k[7]);
    return buf;
}

static std::string strtime(int64_t t, const char* f) {
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
    localtime_r(&tt, &tm);
    char buf[64];
    std::strftime(buf, sizeof buf, f, &tm);
    return buf;
}

std::string clock_time(int64_t t) { return strtime(t, "%H:%M:%S"); }
std::string date_time(int64_t t) { return strtime(t, "%Y-%m-%d %H:%M"); }

}  // namespace dp::fmt
