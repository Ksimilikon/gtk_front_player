#include "core/Covers.hpp"

#include <glibmm/bytes.h>

#include <algorithm>

#include "core/ffi.h"

namespace dp {

Covers& Covers::get() {
    static Covers c;
    return c;
}

Glib::RefPtr<Gdk::Texture> Covers::lookup(const std::string& key) {
    const auto it = map_.find(key);
    if (it == map_.end()) return {};
    order_.remove(key);
    order_.push_back(key);
    return it->second;
}

void Covers::store(const std::string& key, const Glib::RefPtr<Gdk::Texture>& tex) {
    // a missing cover is cached too (null), so it isn't looked up again.
    map_[key] = tex;
    order_.remove(key);
    order_.push_back(key);
    while (order_.size() > kMax) {
        map_.erase(order_.front());
        order_.pop_front();
    }
}

static Glib::RefPtr<Gdk::Texture> from_core(int64_t id, const char* path) {
    uint8_t* ptr = nullptr;
    size_t len = 0;
    if (idx_get_track_cover(id, path, &ptr, &len) != 0 || !ptr) return {};
    Glib::RefPtr<Gdk::Texture> tex;
    try {
        auto bytes = Glib::Bytes::create(ptr, len);
        tex = Gdk::Texture::create_from_bytes(bytes);
    } catch (const Glib::Error&) {
    }
    idx_free_bytes(ptr, len);
    return tex;
}

Glib::RefPtr<Gdk::Texture> Covers::track(int64_t id) {
    const std::string key = "t:" + std::to_string(id);
    if (map_.count(key)) return lookup(key);
    auto tex = from_core(id, nullptr);
    store(key, tex);
    return tex;
}

Glib::RefPtr<Gdk::Texture> Covers::track_path(const std::string& path) {
    const std::string key = "p:" + path;
    if (map_.count(key)) return lookup(key);
    auto tex = from_core(0, path.c_str());
    store(key, tex);
    return tex;
}

Glib::RefPtr<Gdk::Texture> Covers::file(const std::string& path) {
    if (path.empty()) return {};
    const std::string key = "f:" + path;
    if (map_.count(key)) return lookup(key);
    Glib::RefPtr<Gdk::Texture> tex;
    try {
        tex = Gdk::Texture::create_from_filename(path);
    } catch (const Glib::Error&) {
    }
    store(key, tex);
    return tex;
}

std::string Covers::track_bytes(int64_t id) {
    uint8_t* ptr = nullptr;
    size_t len = 0;
    if (idx_get_track_cover(id, nullptr, &ptr, &len) != 0 || !ptr) return {};
    std::string out(reinterpret_cast<const char*>(ptr), len);
    idx_free_bytes(ptr, len);
    return out;
}

void Covers::invalidate_track(int64_t id) {
    const std::string key = "t:" + std::to_string(id);
    map_.erase(key);
    order_.remove(key);
}

void Covers::clear() {
    map_.clear();
    order_.clear();
}

}  // namespace dp
