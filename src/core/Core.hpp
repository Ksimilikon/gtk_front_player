// C++ side of the core: owning copies of the FFI structs, status texts and
// init. Every core buffer is copied and freed right away with its paired
// `*_free_*`, so nothing below holds core memory.
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/ffi.h"

namespace dp::core {

struct Track {
    int64_t id = 0;
    std::string path;
    std::string title;
    std::vector<std::string> artists;
    std::string album;
    std::vector<std::string> genres;
    uint64_t duration = 0;  // seconds
    uint32_t sample_rate = 0;
    uint32_t bitrate = 0;
    int64_t listen_count = 0;
    std::string cover_art;  // cover file from the db (may be empty)
    float volume = 1.0f;
    bool invalid = false;
    std::string color;
    std::string label;
    bool ignore_orphan = false;

    std::string artists_text() const;
    /// the title, or the file name when the tags have none.
    std::string display_title() const;
};

struct Playlist {
    std::string name;
    std::vector<int64_t> tracks;
    std::string cover;
    uint64_t created_at = 0;
    uint64_t updated_at = 0;
};

/// which status enum a code belongs to (for its text).
enum class Domain { Init, Idx, Engine, Cdc, Con, Sync, Mod };

/// a localized, human-readable text of a status code.
std::string status_text(Domain d, uint32_t code);

struct Dirs {
    std::filesystem::path cache, config, data, music;
};

/// `init` of the core; the status (`0` = ok).
uint32_t init(const Dirs& dirs);
const Dirs& dirs();

// helpers over core memory
std::string take_string(char* s);  // copies and frees (null → "")
std::string str(const char* s);    // copies (null → "")
std::vector<std::string> take_strings(FFIStringList list, void (*free_fn)(FFIStringList));
Track to_track(const FFITrackMetadata& m);

/// keeps C strings alive for an FFI call taking `const char* const*`.
class CStrings {
public:
    explicit CStrings(const std::vector<std::string>& v);
    const char* const* data() const { return ptrs_.empty() ? nullptr : ptrs_.data(); }
    size_t size() const { return ptrs_.size(); }

private:
    std::vector<const char*> ptrs_;
};

/// one track straight from the db (fresh, not the cached library copy).
std::optional<Track> fetch_track(int64_t id);

/// output devices and the current one.
std::vector<std::string> output_devices();
std::string output_device();

/// some core calls hold the core lock for long (indexing, packing, import):
/// while one runs, periodic polling (the player position) is skipped so the
/// UI thread doesn't block on the lock.
class LongOp {
public:
    LongOp() { ++count(); }
    ~LongOp() { --count(); }
    LongOp(const LongOp&) = delete;
    LongOp& operator=(const LongOp&) = delete;
    static bool active() { return count() > 0; }

private:
    static std::atomic<int>& count() {
        static std::atomic<int> c{0};
        return c;
    }
};

/// the last core panic message (empty if none).
std::string last_panic();

}  // namespace dp::core
