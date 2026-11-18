#include "core/Covers.hpp"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <glibmm/bytes.h>

#include <algorithm>
#include <fstream>
#include <iterator>

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

void Covers::store(const std::string& key, const Glib::RefPtr<Gdk::Texture>& tex, size_t max) {
    // a missing cover is cached too (null), so it isn't looked up again.
    map_[key] = tex;
    order_.remove(key);
    order_.push_back(key);
    while (order_.size() > max + kMax) {
        map_.erase(order_.front());
        order_.pop_front();
    }
}

static std::string core_bytes(int64_t id, const char* path) {
    uint8_t* ptr = nullptr;
    size_t len = 0;
    if (idx_get_track_cover(id, path, &ptr, &len) != 0 || !ptr) return {};
    std::string out(reinterpret_cast<const char*>(ptr), len);
    idx_free_bytes(ptr, len);
    return out;
}

static Glib::RefPtr<Gdk::Texture> texture_of(const std::string& bytes) {
    if (bytes.empty()) return {};
    try {
        return Gdk::Texture::create_from_bytes(Glib::Bytes::create(bytes.data(), bytes.size()));
    } catch (const Glib::Error&) {
        return {};
    }
}

Glib::RefPtr<Gdk::Texture> Covers::track(int64_t id) {
    const std::string key = "t:" + std::to_string(id);
    if (map_.count(key)) return lookup(key);
    auto tex = texture_of(core_bytes(id, nullptr));
    store(key, tex, kMaxThumbs);
    return tex;
}

Glib::RefPtr<Gdk::Texture> Covers::track_path(const std::string& path) {
    const std::string key = "p:" + path;
    if (map_.count(key)) return lookup(key);
    auto tex = texture_of(core_bytes(0, path.c_str()));
    store(key, tex, kMaxThumbs);
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
    store(key, tex, kMaxThumbs);
    return tex;
}

std::string Covers::track_bytes(int64_t id) { return core_bytes(id, nullptr); }

void Covers::thumb_track(int64_t id, int px, AliveRef alive, Callback cb) {
    request({"tt:" + std::to_string(id) + "@" + std::to_string(px), id, {}, px, std::move(alive), std::move(cb)});
}

void Covers::thumb_file(const std::string& path, int px, AliveRef alive, Callback cb) {
    if (path.empty()) return cb({});
    request({"tf:" + path + "@" + std::to_string(px), 0, path, px, std::move(alive), std::move(cb)});
}

void Covers::request(Job job) {
    if (map_.count(job.key)) {
        job.cb(lookup(job.key));
        return;
    }
    auto& w = waiting_[job.key];
    const bool queued = !w.empty();
    w.push_back(job);
    if (queued) return;
    {
        std::lock_guard lock(mu_);
        jobs_.push_front(std::move(job));  // the newest request first (what is on screen now)
        if (!started_) {
            // detached: it lives as long as the process (a joinable thread
            // in a static object would abort at exit).
            started_ = true;
            std::thread([this] { worker(); }).detach();
        }
    }
    cv_.notify_one();
}

/// decodes `bytes` and scales the picture so its shorter side is `side` px;
/// a memory texture (safe to build off the main thread).
static GdkTexture* decode_scaled(const std::string& bytes, int side) {
    if (bytes.empty()) return nullptr;
    GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
    GdkTexture* out = nullptr;
    if (gdk_pixbuf_loader_write(loader, reinterpret_cast<const guchar*>(bytes.data()), bytes.size(), nullptr) &&
        gdk_pixbuf_loader_close(loader, nullptr)) {
        if (GdkPixbuf* pb = gdk_pixbuf_loader_get_pixbuf(loader)) {
            const int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
            const double scale = std::min(1.0, static_cast<double>(side) / std::min(w, h));
            GdkPixbuf* scaled = scale < 1.0 ? gdk_pixbuf_scale_simple(pb, std::max(1, static_cast<int>(w * scale)),
                                                                      std::max(1, static_cast<int>(h * scale)),
                                                                      GDK_INTERP_BILINEAR)
                                            : static_cast<GdkPixbuf*>(g_object_ref(pb));
            if (scaled) {
                GBytes* px = gdk_pixbuf_read_pixel_bytes(scaled);
                out = gdk_memory_texture_new(gdk_pixbuf_get_width(scaled), gdk_pixbuf_get_height(scaled),
                                             gdk_pixbuf_get_has_alpha(scaled) ? GDK_MEMORY_R8G8B8A8 : GDK_MEMORY_R8G8B8,
                                             px, static_cast<gsize>(gdk_pixbuf_get_rowstride(scaled)));
                g_bytes_unref(px);
                g_object_unref(scaled);
            }
        }
    } else {
        gdk_pixbuf_loader_close(loader, nullptr);
    }
    g_object_unref(loader);
    return out;
}

void Covers::worker() {
    for (;;) {
        Job job;
        {
            std::unique_lock lock(mu_);
            cv_.wait(lock, [this] { return !jobs_.empty(); });
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        std::string bytes;
        if (job.id) {
            bytes = core_bytes(job.id, nullptr);
        } else {
            std::ifstream in(job.path, std::ios::binary);
            bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        // twice the displayed size: sharp on HiDPI screens.
        GdkTexture* raw = decode_scaled(bytes, job.px * 2);
        post([this, key = job.key, raw] {
            Glib::RefPtr<Gdk::Texture> tex = raw ? Glib::wrap(raw) : Glib::RefPtr<Gdk::Texture>();
            store(key, tex, kMaxThumbs);
            auto it = waiting_.find(key);
            if (it == waiting_.end()) return;
            auto list = std::move(it->second);
            waiting_.erase(it);
            for (auto& j : list)
                if (!j.alive.expired()) j.cb(tex);
        });
    }
}

void Covers::invalidate_track(int64_t id) {
    const std::string full = "t:" + std::to_string(id), thumb = "tt:" + std::to_string(id) + "@";
    for (auto it = order_.begin(); it != order_.end();) {
        if (*it == full || it->rfind(thumb, 0) == 0) {
            map_.erase(*it);
            it = order_.erase(it);
        } else {
            ++it;
        }
    }
}

void Covers::clear() {
    map_.clear();
    order_.clear();
}

}  // namespace dp
