// playback orchestration over the core engine (like `App` in the TUI): the
// queue, next / previous, autoplay at the end of a track, repeat / shuffle,
// volumes. The engine plays one track; the queue lives here. Main thread only.
#pragma once

#include <sigc++/signal.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/Core.hpp"

namespace dp {

/// one queue entry: a library track (`id`) or a file outside it (`path`,
/// e.g. songs a mod offers).
struct QueueItem {
    int64_t id = 0;
    std::string path;
};

struct PlaybackState {
    bool has_track = false;
    bool playing = false;
    uint64_t position = 0;  // seconds
    uint64_t duration = 0;
};

class Player {
public:
    static Player& get();

    /// installs the end-of-track hook and starts polling the engine.
    void start();

    /// plays `items` as the queue `name` from `start`.
    void play_queue(const std::string& name, std::vector<QueueItem> items, size_t start);
    void play_ids(const std::string& name, const std::vector<int64_t>& ids, size_t start);
    /// a file outside the library (a queue of one).
    void play_path(const std::string& path);
    /// puts a track right after the current one.
    void play_next(const QueueItem& item);

    void toggle();  // play / pause; nothing loaded → replays the queue position
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void seek(uint64_t seconds);

    void set_repeat_one(bool on);
    void set_shuffle(bool on);
    bool repeat_one() const { return repeat_one_; }
    bool shuffle() const { return shuffle_; }

    /// master volume 0..1 (saved by the core).
    void set_volume(double v);
    double volume() const { return volume_; }
    /// volume of the current track (also saved into the db for library tracks).
    void set_track_volume(float v);

    const PlaybackState& state() const { return state_; }
    /// the current queue entry, if any.
    std::optional<QueueItem> current() const;
    /// the library track playing now (null for files outside the library).
    const core::Track* current_track() const;
    /// title / artists of what plays now (also for files outside the library).
    std::string current_title() const;
    std::string current_artists() const;
    const std::string& queue_name() const { return queue_name_; }
    const std::vector<QueueItem>& queue() const { return queue_; }
    size_t queue_index() const { return index_; }

    /// a different track started (or playback ended: no track).
    sigc::signal<void()>& signal_track_changed() { return track_changed_; }
    /// play / pause / position / duration changed.
    sigc::signal<void()>& signal_state_changed() { return state_changed_; }
    /// position jumped (seek) — for MPRIS `Seeked`.
    sigc::signal<void(uint64_t)>& signal_seeked() { return seeked_; }
    sigc::signal<void()>& signal_modes_changed() { return modes_changed_; }
    sigc::signal<void()>& signal_volume_changed() { return volume_changed_; }

private:
    void play_current();
    void step(int dir);
    void on_track_end();
    bool poll();
    static void end_hook();

    std::vector<QueueItem> queue_;
    std::vector<size_t> order_;  // play order (shuffled or identity)
    size_t index_ = 0;           // position in `order_`
    std::string queue_name_;
    bool repeat_one_ = false;
    bool shuffle_ = false;
    double volume_ = 1.0;
    PlaybackState state_;
    std::string external_title_;  // a file outside the library

    sigc::signal<void()> track_changed_, state_changed_, modes_changed_, volume_changed_;
    sigc::signal<void(uint64_t)> seeked_;
};

}  // namespace dp
