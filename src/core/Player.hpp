// playback orchestration over the core engine (like `App` in the TUI): the
// queue, next / previous, autoplay at the end of a track, repeat / shuffle,
// volumes. The engine plays one track; the queue lives here, in play order.
// Main thread only.
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

struct QueueEntry {
    QueueItem item;
    uint64_t key = 0;  // unique within the queue (stable across moves)
    int64_t orig = 0;  // order before shuffling (shuffle off restores it)
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

    /// plays `items` as the queue `name` from `start`. `focus` — the UI
    /// switches to the queue tab (not for a preview).
    void play_queue(const std::string& name, std::vector<QueueItem> items, size_t start, bool focus = true, bool playlist = false);
    void play_ids(const std::string& name, const std::vector<int64_t>& ids, size_t start);
    /// a file (a library track if the path is known); a queue of one.
    void play_path(const std::string& path);
    /// listens to one song without touching the UI focus (editors).
    void preview(int64_t id);

    enum class Where { AfterCurrent, AtEnd };
    /// adds songs to the queue (nothing playing — starts them as queue `name`).
    void enqueue(const std::vector<int64_t>& ids, Where where, const std::string& name);
    void play_next(const QueueItem& item);
    /// queue editing (positions in play order).
    void jump(size_t pos);
    void move(size_t from, size_t to);
    void remove_at(size_t pos);
    /// stops and empties the queue.
    void clear();

    void toggle();  // play / pause; nothing loaded → replays the queue position
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void seek(uint64_t seconds);
    void seek_by(int seconds);

    void set_repeat_one(bool on);
    void set_shuffle(bool on);
    void set_repeat_playlist(bool on);
    bool repeat_playlist() const;
    bool looping_playlist() const { return playlist_ && repeat_playlist(); }
    bool repeat_one() const { return repeat_one_; }
    bool shuffle() const { return shuffle_; }

    /// master volume 0..1 (saved by the core).
    void set_volume(double v);
    void change_volume(double delta) { set_volume(volume_ + delta); }
    double volume() const { return volume_; }
    /// volume of the current track 0..2 (also saved into the db for library tracks).
    void set_track_volume(float v);
    void change_track_volume(float delta);
    float track_volume() const;

    const PlaybackState& state() const { return state_; }
    std::optional<QueueItem> current() const;
    const core::Track* current_track() const;
    std::string current_title() const;
    std::string current_artists() const;
    const std::string& queue_name() const { return queue_name_; }
    const std::vector<QueueEntry>& entries() const { return queue_; }
    size_t position() const { return index_; }
    bool has_queue() const { return !queue_.empty(); }

    sigc::signal<void()>& signal_track_changed() { return track_changed_; }
    sigc::signal<void()>& signal_state_changed() { return state_changed_; }
    /// the queue content / order / name changed.
    sigc::signal<void()>& signal_queue_changed() { return queue_changed_; }
    /// a new queue started; `focus` — show it.
    sigc::signal<void(bool)>& signal_queue_started() { return queue_started_; }
    sigc::signal<void(uint64_t)>& signal_seeked() { return seeked_; }
    sigc::signal<void()>& signal_modes_changed() { return modes_changed_; }
    sigc::signal<void()>& signal_volume_changed() { return volume_changed_; }

private:
    void play_current();
    void step(int dir);
    void on_track_end();
    bool poll();
    void shuffle_rest(bool on);
    static void end_hook();

    std::vector<QueueEntry> queue_;  // play order
    size_t index_ = 0;
    uint64_t next_key_ = 1;
    std::string queue_name_;
    bool playlist_ = false;
    bool repeat_one_ = false;
    bool shuffle_ = false;
    double volume_ = 1.0;
    PlaybackState state_;
    std::string external_title_;

    sigc::signal<void()> track_changed_, state_changed_, modes_changed_, volume_changed_, queue_changed_;
    sigc::signal<void(bool)> queue_started_;
    sigc::signal<void(uint64_t)> seeked_;
};

}  // namespace dp
