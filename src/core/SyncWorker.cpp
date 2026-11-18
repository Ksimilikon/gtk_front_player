#include "core/SyncWorker.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <glib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "core/Core.hpp"
#include "i18n/I18n.hpp"
#include "util/Format.hpp"
#include "util/MainThread.hpp"
#include "util/Paths.hpp"

namespace dp {

using core::Domain;
namespace fs = std::filesystem;

std::string device_fingerprint() {
    uint8_t key[32];
    if (con_get_pub_key(&key) != 0) return {};
    return fmt::fingerprint(key);
}

namespace {

constexpr int kPollMs = 200;
constexpr int kConnectTimeoutMs = 10000;

struct Ctx {
    std::shared_ptr<std::atomic<bool>> cancel;
    std::weak_ptr<int> alive;
    SyncWorker::Listener listener;

    void send(SyncMessage m) const {
        post([alive = alive, l = listener, m = std::move(m)] {
            if (!alive.expired()) l(m);
        });
    }
    void text(Level lv, const std::string& t) const { send({SyncMessage::Text, lv, t, false}); }
    bool cancelled() const { return cancel->load(); }
};

struct Fd {
    int fd = -1;
    explicit Fd(int f = -1) : fd(f) {}
    ~Fd() {
        if (fd >= 0) ::close(fd);
    }
    Fd(Fd&& o) noexcept : fd(o.fd) { o.fd = -1; }
    Fd& operator=(Fd&& o) noexcept {
        std::swap(fd, o.fd);
        return *this;
    }
};

struct Failure {
    std::string text;
    bool cancelled = false;
};

std::string errno_text() { return std::strerror(errno); }

void tune(int fd) {
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
}

/// waits for one connection on `port` (`loopback` — 127.0.0.1 only).
Fd accept_one(uint16_t port, bool loopback, const Ctx& ctx) {
    Fd ls(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (ls.fd < 0) throw Failure{errno_text()};
    int one = 1;
    setsockopt(ls.fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(loopback ? INADDR_LOOPBACK : INADDR_ANY);
    if (::bind(ls.fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0 || ::listen(ls.fd, 1) < 0)
        throw Failure{tr("sync.err.bind", {std::to_string(port), errno_text()})};
    for (;;) {
        if (ctx.cancelled()) throw Failure{tr("sync.cancelled"), true};
        pollfd p{ls.fd, POLLIN, 0};
        const int r = ::poll(&p, 1, kPollMs);
        if (r < 0 && errno != EINTR) throw Failure{errno_text()};
        if (r <= 0) continue;
        sockaddr_in peer{};
        socklen_t plen = sizeof peer;
        Fd s(::accept4(ls.fd, reinterpret_cast<sockaddr*>(&peer), &plen, SOCK_CLOEXEC));
        if (s.fd < 0) continue;
        char ip[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof ip);
        ctx.text(Level::Ok, tr("sync.connected", {std::string(ip) + ":" + std::to_string(ntohs(peer.sin_port))}));
        tune(s.fd);
        return s;
    }
}

/// `host[:port]` → a connected socket.
Fd connect_to(const std::string& target, uint16_t default_port, const Ctx& ctx) {
    std::string host = target, port = std::to_string(default_port);
    if (const auto colon = target.rfind(':'); colon != std::string::npos && target.find(':') == colon) {
        host = target.substr(0, colon);
        port = target.substr(colon + 1);
    }
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (int e = getaddrinfo(host.c_str(), port.c_str(), &hints, &res); e != 0)
        throw Failure{tr("sync.err.resolve", {target, gai_strerror(e)})};
    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> guard(res, freeaddrinfo);
    ctx.text(Level::Info, tr("sync.connecting", {host + ":" + port}));
    std::string last = tr("sync.err.unreachable");
    for (addrinfo* ai = res; ai; ai = ai->ai_next) {
        Fd s(::socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol));
        if (s.fd < 0) continue;
        const int flags = fcntl(s.fd, F_GETFL);
        fcntl(s.fd, F_SETFL, flags | O_NONBLOCK);
        if (::connect(s.fd, ai->ai_addr, ai->ai_addrlen) < 0 && errno != EINPROGRESS) {
            last = errno_text();
            continue;
        }
        int waited = 0;
        bool ok = false;
        while (waited < kConnectTimeoutMs) {
            if (ctx.cancelled()) throw Failure{tr("sync.cancelled"), true};
            pollfd p{s.fd, POLLOUT, 0};
            const int r = ::poll(&p, 1, kPollMs);
            waited += kPollMs;
            if (r > 0) {
                int err = 0;
                socklen_t l = sizeof err;
                getsockopt(s.fd, SOL_SOCKET, SO_ERROR, &err, &l);
                if (err == 0) ok = true;
                else last = std::strerror(err);
                break;
            }
        }
        if (!ok) continue;
        fcntl(s.fd, F_SETFL, flags);
        tune(s.fd);
        ctx.text(Level::Ok, tr("sync.connected", {host + ":" + port}));
        return s;
    }
    throw Failure{last};
}

void adb(const std::vector<std::string>& args) {
    std::vector<char*> argv{const_cast<char*>("adb")};
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    char* err = nullptr;
    int status = 0;
    GError* gerr = nullptr;
    if (!g_spawn_sync(nullptr, argv.data(), nullptr, GSpawnFlags(G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL), nullptr,
                      nullptr, nullptr, &err, &status, &gerr)) {
        std::string msg = gerr ? gerr->message : "adb";
        if (gerr) g_error_free(gerr);
        throw Failure{tr("sync.err.adb_missing") + " (" + msg + ")"};
    }
    std::string e = err ? fmt::trim(err) : "";
    g_free(err);
    if (!g_spawn_check_wait_status(status, nullptr)) throw Failure{"adb " + fmt::join(args, " ") + ": " + e};
}

/// brings the transport up; `host` — this side listened.
Fd establish(const SyncTarget& t, bool& host, const Ctx& ctx) {
    const std::string p = "tcp:" + std::to_string(t.port);
    switch (t.kind) {
        case SyncTarget::TcpListen:
            host = true;
            ctx.text(Level::Info, tr("sync.waiting", {std::to_string(t.port)}));
            return accept_one(t.port, false, ctx);
        case SyncTarget::TcpConnect:
            host = false;
            return connect_to(t.address, t.port, ctx);
        case SyncTarget::AdbConnect:
            host = false;
            adb({"forward", p, p});
            ctx.text(Level::Info, tr("sync.adb_forward", {p}));
            return connect_to("127.0.0.1:" + std::to_string(t.port), t.port, ctx);
        case SyncTarget::AdbListen:
            host = true;
            adb({"reverse", p, p});
            ctx.text(Level::Info, tr("sync.adb_reverse", {p}));
            return accept_one(t.port, true, ctx);
    }
    throw Failure{"?"};
}

bool write_all(int fd, const uint8_t* data, size_t len) {
    while (len) {
        const ssize_t n = ::send(fd, data, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        data += n;
        len -= static_cast<size_t>(n);
    }
    return true;
}

bool flush_output(FFISyncSession* s, int fd) {
    uint8_t* ptr = nullptr;
    size_t len = 0;
    if (sync_take_output(s, &ptr, &len) != 0 || !ptr) return true;
    const bool ok = write_all(fd, ptr, len);
    sync_free_bytes(ptr, len);
    return ok;
}

/// writes the files of a received portion: a song over the local file with
/// the same name if there is one, anything else next to the music.
void import_portion(FFISyncSession* s, const std::string& music_dir, const Ctx& ctx) {
    FFISyncFiles files{};
    if (sync_import_files(s, &files) != 0) return;
    std::vector<std::string> written(files.len);
    std::vector<const char*> paths(files.len, nullptr);
    std::error_code ec;
    fs::create_directories(music_dir, ec);
    for (size_t i = 0; i < files.len; ++i) {
        const FFISyncFile& f = files.ptr[i];
        const fs::path dest = (f.replace && f.kind == FFI_IMPORT_SONG)
                                  ? fs::path(f.replace)
                                  : paths::unique_path(music_dir, core::str(f.name));
        std::ofstream out(dest, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(f.data), static_cast<std::streamsize>(f.data_len));
        out.close();
        if (!out) {
            ctx.text(Level::Warn, tr("sync.write_failed", {core::str(f.name)}));
            continue;
        }
        written[i] = dest.string();
        paths[i] = written[i].c_str();
    }
    if (uint32_t st = sync_import_done(s, paths.empty() ? nullptr : paths.data(), paths.size()); st != 0)
        ctx.text(Level::Error, tr("sync.import_failed", {core::status_text(Domain::Sync, st)}));
}

std::string portion(const char* verb_key, uint32_t songs, uint32_t playlists) {
    return tr(verb_key, {std::to_string(songs), std::to_string(playlists)});
}

std::string wire(uint32_t status) {
    const std::string key = "status.wire." + std::to_string(status);
    const std::string& t = tr(key);
    return t == key ? "#" + std::to_string(status) : t;
}

void describe(const FFISyncEvent& ev, const Ctx& ctx) {
    switch (ev.kind) {
        case FFI_SYNC_EV_PAIRED: {
            std::string name = "#" + std::to_string(ev.paired_id);
            FFIConPairedList l{};
            if (con_get_paired(&l) == 0) {
                for (size_t i = 0; i < l.len; ++i)
                    if (l.ptr[i].id == ev.paired_id)
                        name += " " + core::str(l.ptr[i].name) + " [" + fmt::fingerprint(l.ptr[i].pub_key) + "]";
                con_free_paired_list(l);
            }
            ctx.text(Level::Ok, tr("sync.paired", {name}));
            break;
        }
        case FFI_SYNC_EV_SENT: ctx.text(Level::Info, portion("sync.sent", ev.songs, ev.playlists)); break;
        case FFI_SYNC_EV_RECEIVED: ctx.text(Level::Info, portion("sync.received", ev.songs, ev.playlists)); break;
        case FFI_SYNC_EV_PEER_REJECTED: ctx.text(Level::Warn, tr("sync.rejected", {wire(ev.status)})); break;
        case FFI_SYNC_EV_DONE: ctx.text(Level::Ok, tr("sync.done")); break;
        case FFI_SYNC_EV_FAILED:
            ctx.text(Level::Error,
                     tr(ev.by_peer ? "sync.failed_peer" : "sync.failed_here", {wire(ev.status)}));
            break;
        default: break;
    }
}

FFISyncSession* open_session(uint64_t con, bool host, const SyncMode& m, uint32_t& st) {
    FFISyncSession* s = nullptr;
    switch (m.kind) {
        case SyncMode::Sync: st = sync_new(con, host, &s); break;
        case SyncMode::Receive: st = sync_new_receive(con, host, m.force, &s); break;
        case SyncMode::Send: {
            core::CStrings names(m.what.playlists);
            const uint32_t what = m.what.kind == exchange::Selection::Library     ? 0
                                  : m.what.kind == exchange::Selection::Playlists ? 1
                                                                                  : 2;
            st = sync_new_send(con, host, what, names.data(), names.size(),
                               m.what.tracks.empty() ? nullptr : m.what.tracks.data(), m.what.tracks.size(),
                               m.flags.audio, m.flags.names, m.flags.covers, &s);
            break;
        }
    }
    return st == 0 ? s : nullptr;
}

/// carries bytes between the socket and the session; `true` — complete.
bool drive(int fd, bool host, uint64_t con, const SyncMode& mode, const std::string& music_dir, const Ctx& ctx,
           bool& changed) {
    uint32_t st = 0;
    FFISyncSession* s = open_session(con, host, mode, st);
    if (!s) throw Failure{core::status_text(Domain::Sync, st)};
    std::unique_ptr<FFISyncSession, decltype(&sync_free)> guard(s, sync_free);
    std::vector<uint8_t> buf(256 * 1024);
    bool complete = false;
    for (;;) {
        if (!flush_output(s, fd)) throw Failure{errno_text()};
        FFISyncEvent ev{};
        while (sync_poll_event(s, &ev) == 0 && ev.kind != FFI_SYNC_EV_NONE) {
            if (ev.kind == FFI_SYNC_EV_IMPORT) import_portion(s, music_dir, ctx);
            if (ev.kind == FFI_SYNC_EV_RECEIVED) changed = true;
            if (ev.kind == FFI_SYNC_EV_DONE) complete = true;
            describe(ev, ctx);
        }
        if (sync_finished(s)) break;
        if (ctx.cancelled()) {
            sync_cancel(s);
            ctx.text(Level::Warn, tr("sync.cancelled"));
            break;
        }
        pollfd p{fd, POLLIN, 0};
        const int r = ::poll(&p, 1, kPollMs);
        if (r < 0 && errno != EINTR) throw Failure{errno_text()};
        if (r <= 0) continue;
        const ssize_t n = ::recv(fd, buf.data(), buf.size(), 0);
        if (n == 0) throw Failure{tr("sync.err.closed")};
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            throw Failure{errno_text()};
        }
        sync_feed(s, buf.data(), static_cast<size_t>(n));
    }
    // the last answer (`Ack` / `Abort`) still has to reach the peer.
    flush_output(s, fd);
    return complete;
}

void run(const SyncTarget& target, const SyncMode& mode, const std::string& music_dir, const Ctx& ctx) {
    if (auto fp = device_fingerprint(); !fp.empty()) ctx.text(Level::Info, tr("sync.this_device", {fp}));
    bool complete = false, changed = false;
    try {
        bool host = false;
        Fd sock = establish(target, host, ctx);
        uint64_t con = 0;
        if (uint32_t st = con_add(&con); st != 0) throw Failure{core::status_text(Domain::Con, st)};
        if (uint32_t st = con_connect(con); st != 0) {
            con_remove(con);
            throw Failure{core::status_text(Domain::Con, st)};
        }
        try {
            complete = drive(sock.fd, host, con, mode, music_dir, ctx, changed);
        } catch (...) {
            con_disconnect(con);
            con_remove(con);
            throw;
        }
        con_disconnect(con);
        con_remove(con);
    } catch (const Failure& f) {
        ctx.text(f.cancelled ? Level::Warn : Level::Error, f.text);
    }
    if (changed) ctx.send({SyncMessage::LibraryChanged, Level::Info, {}, false});
    ctx.send({SyncMessage::Finished, Level::Info, {}, complete});
}

}  // namespace

SyncWorker::SyncWorker(SyncTarget target, SyncMode mode, std::string music_dir, Listener listener) {
    Ctx ctx{cancel_, alive_, std::move(listener)};
    std::thread([ctx, target = std::move(target), mode = std::move(mode), music_dir = std::move(music_dir),
                 finished = finished_] {
        run(target, mode, music_dir, ctx);
        finished->store(true);
    }).detach();
}

SyncWorker::~SyncWorker() { cancel(); }

}  // namespace dp
