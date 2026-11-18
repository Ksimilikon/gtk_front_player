// library sync with another device (like the TUI's `sync` module): the
// protocol, pairing and encryption live in the core (`con_*`, `sync_*`); the
// worker thread only carries bytes over a transport and writes received
// files. Transports: TCP (wait / connect by address) and USB through ADB port
// forwarding. The side that listens is the host.
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "core/Exchange.hpp"
#include "util/Log.hpp"

namespace dp {

inline constexpr uint16_t kSyncPort = 47800;

struct SyncTarget {
    enum Kind { TcpListen, TcpConnect, AdbConnect, AdbListen } kind = TcpListen;
    uint16_t port = kSyncPort;
    std::string address;  // TcpConnect: host[:port]
};

struct SyncMode {
    enum Kind { Sync, Receive, Send } kind = Sync;
    bool force = false;  // Receive
    exchange::Selection what;  // Send
    exchange::Flags flags;     // Send
};

struct SyncMessage {
    enum Kind { Text, LibraryChanged, Finished } kind = Text;
    Level level = Level::Info;
    std::string text;
    bool complete = false;  // Finished
};

class SyncWorker {
public:
    using Listener = std::function<void(const SyncMessage&)>;

    /// starts the worker; `listener` is called on the main thread.
    SyncWorker(SyncTarget target, SyncMode mode, std::string music_dir, Listener listener);
    /// cancels; the thread ends by itself (not joined: a connect may take
    /// seconds).
    ~SyncWorker();

    void cancel() { cancel_->store(true); }
    bool running() const { return !finished_->load(); }

private:
    std::shared_ptr<std::atomic<bool>> cancel_ = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> finished_ = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<int> alive_ = std::make_shared<int>(0);
};

/// this device's key fingerprint (empty if the keys are unavailable).
std::string device_fingerprint();

}  // namespace dp
