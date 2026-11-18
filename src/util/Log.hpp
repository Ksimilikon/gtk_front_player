// the app log: what the LOGS tab and the status line show. Thread-safe:
// entries from other threads are delivered to listeners on the main thread.
#pragma once

#include <sigc++/signal.h>

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace dp {

enum class Level { Info, Ok, Warn, Error };

struct LogEntry {
    int64_t time;        // unix seconds
    Level level;
    std::string source;  // "core", "sync", "mods", "ui", ...
    std::string text;
};

class Log {
public:
    static Log& get();

    void add(Level level, const std::string& source, const std::string& text);
    void info(const std::string& src, const std::string& t) { add(Level::Info, src, t); }
    void ok(const std::string& src, const std::string& t) { add(Level::Ok, src, t); }
    void warn(const std::string& src, const std::string& t) { add(Level::Warn, src, t); }
    void error(const std::string& src, const std::string& t) { add(Level::Error, src, t); }

    /// a copy of the kept entries (main thread).
    std::deque<LogEntry> entries() const;
    void clear();

    /// a new entry (main thread).
    sigc::signal<void(const LogEntry&)>& signal_added() { return added_; }
    sigc::signal<void()>& signal_cleared() { return cleared_; }

private:
    static constexpr size_t kMax = 2000;
    mutable std::mutex mu_;
    std::deque<LogEntry> entries_;
    sigc::signal<void(const LogEntry&)> added_;
    sigc::signal<void()> cleared_;
};

}  // namespace dp
