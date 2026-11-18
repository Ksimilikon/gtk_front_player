#include "util/Log.hpp"

#include <ctime>
#include <iostream>

#include "util/MainThread.hpp"

namespace dp {

Log& Log::get() {
    static Log l;
    return l;
}

void Log::add(Level level, const std::string& source, const std::string& text) {
    LogEntry e{static_cast<int64_t>(std::time(nullptr)), level, source, text};
    static const char* names[] = {"info", "ok", "warn", "error"};
    std::cerr << "[" << names[static_cast<int>(level)] << "] " << source << ": " << text << '\n';
    {
        std::lock_guard lock(mu_);
        entries_.push_back(e);
        if (entries_.size() > kMax) entries_.pop_front();
    }
    post([this, e] { added_.emit(e); });
}

std::deque<LogEntry> Log::entries() const {
    std::lock_guard lock(mu_);
    return entries_;
}

void Log::clear() {
    {
        std::lock_guard lock(mu_);
        entries_.clear();
    }
    cleared_.emit();
}

}  // namespace dp
