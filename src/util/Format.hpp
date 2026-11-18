// small string helpers.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dp::fmt {

std::string trim(const std::string& s);
std::string join(const std::vector<std::string>& v, const std::string& sep);
/// splits by `sep`, trims every part, drops empty ones.
std::vector<std::string> split(const std::string& s, char sep);
/// `m:ss` or `h:mm:ss`.
std::string duration(uint64_t seconds);
/// `1.2 MB`.
std::string bytes(uint64_t n);
/// case-insensitive (UTF-8 aware) substring match.
bool contains_ci(const std::string& haystack, const std::string& needle);
/// short fingerprint of a public key: `ab12 cd34 ef56 7890`.
std::string fingerprint(const uint8_t* key32);
/// local time `HH:MM:SS`.
std::string clock_time(int64_t unix_seconds);
/// local date `YYYY-MM-DD HH:MM`.
std::string date_time(int64_t unix_seconds);

}  // namespace dp::fmt
