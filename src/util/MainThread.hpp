// running code on the GTK main thread from any thread, and background work
// whose result comes back to the main thread.
#pragma once

#include <glib.h>

#include <functional>
#include <memory>
#include <thread>

namespace dp {

/// queues `fn` to run on the main loop (thread-safe: g_idle_add).
inline void post(std::function<void()> fn) {
    auto* heap = new std::function<void()>(std::move(fn));
    g_idle_add_full(
        G_PRIORITY_DEFAULT,
        [](gpointer data) -> gboolean {
            (*static_cast<std::function<void()>*>(data))();
            return G_SOURCE_REMOVE;
        },
        heap, [](gpointer data) { delete static_cast<std::function<void()>*>(data); });
}

/// a token a UI object holds; background results check it before touching the
/// object (the object may be gone by then).
using Alive = std::shared_ptr<int>;
using AliveRef = std::weak_ptr<int>;
inline Alive make_alive() { return std::make_shared<int>(0); }

/// runs `work` on a detached thread, then `done(result)` on the main thread
/// if `alive` still exists.
template <typename Work, typename Done>
void run_async(AliveRef alive, Work work, Done done) {
    std::thread([alive = std::move(alive), work = std::move(work), done = std::move(done)]() mutable {
        auto result = work();
        post([alive, done = std::move(done), result = std::move(result)]() mutable {
            if (!alive.expired()) done(std::move(result));
        });
    }).detach();
}

}  // namespace dp
