#include "dbus/Mpris.hpp"

#include <glibmm/convert.h>

#include <filesystem>
#include <fstream>

#include "core/Covers.hpp"
#include "core/Player.hpp"
#include "util/Log.hpp"
#include "util/Paths.hpp"

namespace dp {

static constexpr const char* kName = "org.mpris.MediaPlayer2.dream_player_gtk";
static constexpr const char* kPath = "/org/mpris/MediaPlayer2";
static constexpr const char* kRoot = "org.mpris.MediaPlayer2";
static constexpr const char* kPlayer = "org.mpris.MediaPlayer2.Player";

static const char* kXml = R"(
<node>
  <interface name="org.mpris.MediaPlayer2">
    <method name="Raise"/>
    <method name="Quit"/>
    <property name="CanQuit" type="b" access="read"/>
    <property name="CanRaise" type="b" access="read"/>
    <property name="HasTrackList" type="b" access="read"/>
    <property name="Identity" type="s" access="read"/>
    <property name="DesktopEntry" type="s" access="read"/>
    <property name="SupportedUriSchemes" type="as" access="read"/>
    <property name="SupportedMimeTypes" type="as" access="read"/>
  </interface>
  <interface name="org.mpris.MediaPlayer2.Player">
    <method name="Next"/>
    <method name="Previous"/>
    <method name="Pause"/>
    <method name="PlayPause"/>
    <method name="Stop"/>
    <method name="Play"/>
    <method name="Seek"><arg direction="in" name="Offset" type="x"/></method>
    <method name="SetPosition">
      <arg direction="in" name="TrackId" type="o"/>
      <arg direction="in" name="Position" type="x"/>
    </method>
    <method name="OpenUri"><arg direction="in" name="Uri" type="s"/></method>
    <signal name="Seeked"><arg name="Position" type="x"/></signal>
    <property name="PlaybackStatus" type="s" access="read"/>
    <property name="LoopStatus" type="s" access="readwrite"/>
    <property name="Rate" type="d" access="readwrite"/>
    <property name="Shuffle" type="b" access="readwrite"/>
    <property name="Metadata" type="a{sv}" access="read"/>
    <property name="Volume" type="d" access="readwrite"/>
    <property name="Position" type="x" access="read"/>
    <property name="MinimumRate" type="d" access="read"/>
    <property name="MaximumRate" type="d" access="read"/>
    <property name="CanGoNext" type="b" access="read"/>
    <property name="CanGoPrevious" type="b" access="read"/>
    <property name="CanPlay" type="b" access="read"/>
    <property name="CanPause" type="b" access="read"/>
    <property name="CanSeek" type="b" access="read"/>
    <property name="CanControl" type="b" access="read"/>
  </interface>
</node>)";

void Mpris::start(std::function<void()> raise, std::function<void()> quit) {
    raise_ = std::move(raise);
    quit_ = std::move(quit);
    try {
        node_ = Gio::DBus::NodeInfo::create_for_xml(kXml);
    } catch (const Glib::Error& e) {
        Log::get().error("dbus", e.what());
        return;
    }
    owner_ = Gio::DBus::own_name(
        Gio::DBus::BusType::SESSION, kName, sigc::mem_fun(*this, &Mpris::on_bus), {},
        [](const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring& name) {
            Log::get().warn("dbus", "name lost: " + name);
        });

    auto& p = Player::get();
    p.signal_track_changed().connect([this] { emit_changed(kPlayer, {"Metadata", "PlaybackStatus"}); });
    p.signal_state_changed().connect([this] {
        // the position changes every poll: only the status is announced.
        const auto& s = Player::get().state();
        const std::string status = !s.has_track ? "Stopped" : s.playing ? "Playing" : "Paused";
        if (status != last_status_) {
            last_status_ = status;
            emit_changed(kPlayer, {"PlaybackStatus"});
        }
    });
    p.signal_modes_changed().connect([this] { emit_changed(kPlayer, {"LoopStatus", "Shuffle"}); });
    p.signal_volume_changed().connect([this] { emit_changed(kPlayer, {"Volume"}); });
    p.signal_seeked().connect([this](uint64_t sec) {
        if (!conn_) return;
        try {
            conn_->emit_signal(kPath, kPlayer, "Seeked", {},
                               Glib::VariantContainerBase::create_tuple(
                                   Glib::Variant<gint64>::create(static_cast<gint64>(sec) * 1000000)));
        } catch (const Glib::Error&) {
        }
    });
}

void Mpris::stop() {
    if (conn_)
        for (guint id : registrations_) conn_->unregister_object(id);
    registrations_.clear();
    if (owner_) Gio::DBus::unown_name(owner_);
    owner_ = 0;
}

void Mpris::on_bus(const Glib::RefPtr<Gio::DBus::Connection>& conn, const Glib::ustring&) {
    conn_ = conn;
    // must outlive the registration.
    static auto* keep = new Gio::DBus::InterfaceVTable(sigc::mem_fun(*this, &Mpris::on_method),
                                                       sigc::mem_fun(*this, &Mpris::on_get),
                                                       sigc::mem_fun(*this, &Mpris::on_set));
    try {
        registrations_.push_back(conn->register_object(kPath, node_->lookup_interface(kRoot), *keep));
        registrations_.push_back(conn->register_object(kPath, node_->lookup_interface(kPlayer), *keep));
        Log::get().info("dbus", "MPRIS: " + std::string(kName));
    } catch (const Glib::Error& e) {
        Log::get().error("dbus", e.what());
    }
}

void Mpris::on_method(const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring&, const Glib::ustring&,
                      const Glib::ustring& iface, const Glib::ustring& method,
                      const Glib::VariantContainerBase& params,
                      const Glib::RefPtr<Gio::DBus::MethodInvocation>& inv) {
    auto& p = Player::get();
    if (iface == kRoot) {
        if (method == "Raise" && raise_) raise_();
        if (method == "Quit" && quit_) quit_();
    } else if (method == "Next") {
        p.next();
    } else if (method == "Previous") {
        p.previous();
    } else if (method == "Pause") {
        p.pause();
    } else if (method == "PlayPause") {
        p.toggle();
    } else if (method == "Stop") {
        p.stop();
    } else if (method == "Play") {
        p.play();
    } else if (method == "Seek") {
        Glib::Variant<gint64> off;
        params.get_child(off, 0);
        const gint64 target = static_cast<gint64>(p.state().position) * 1000000 + off.get();
        if (target < 0) p.seek(0);
        else if (p.state().duration && target / 1000000 >= static_cast<gint64>(p.state().duration)) p.next();
        else p.seek(static_cast<uint64_t>(target / 1000000));
    } else if (method == "SetPosition") {
        Glib::Variant<gint64> pos;
        params.get_child(pos, 1);
        if (pos.get() >= 0) p.seek(static_cast<uint64_t>(pos.get() / 1000000));
    } else if (method == "OpenUri") {
        Glib::Variant<Glib::ustring> uri;
        params.get_child(uri, 0);
        try {
            p.play_path(Glib::filename_from_uri(uri.get()));
        } catch (const Glib::Error&) {
        }
    }
    inv->return_value(Glib::VariantContainerBase());
}

void Mpris::on_get(Glib::VariantBase& out, const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring&,
                   const Glib::ustring&, const Glib::ustring& iface, const Glib::ustring& prop) {
    out = property(iface, prop);
}

bool Mpris::on_set(const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring&, const Glib::ustring&,
                   const Glib::ustring&, const Glib::ustring& prop, const Glib::VariantBase& value) {
    auto& p = Player::get();
    if (prop == "Volume") {
        p.set_volume(Glib::VariantBase::cast_dynamic<Glib::Variant<double>>(value).get());
    } else if (prop == "Shuffle") {
        p.set_shuffle(Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(value).get());
    } else if (prop == "LoopStatus") {
        p.set_repeat_one(Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(value).get() == "Track");
    }
    return true;
}

Glib::VariantBase Mpris::property(const Glib::ustring& iface, const Glib::ustring& prop) {
    using B = Glib::Variant<bool>;
    using S = Glib::Variant<Glib::ustring>;
    using SL = Glib::Variant<std::vector<Glib::ustring>>;
    auto& p = Player::get();
    const auto& s = p.state();
    if (iface == kRoot) {
        if (prop == "CanQuit" || prop == "CanRaise") return B::create(true);
        if (prop == "HasTrackList") return B::create(false);
        if (prop == "Identity") return S::create("Dream Player");
        if (prop == "DesktopEntry") return S::create("dev.dreamplayer.gtk");
        if (prop == "SupportedUriSchemes") return SL::create({"file"});
        if (prop == "SupportedMimeTypes") return SL::create({"audio/mpeg", "audio/flac", "audio/ogg", "audio/x-wav"});
        return {};
    }
    if (prop == "PlaybackStatus") return S::create(!s.has_track ? "Stopped" : s.playing ? "Playing" : "Paused");
    if (prop == "LoopStatus") return S::create(p.repeat_one() ? "Track" : "None");
    if (prop == "Rate" || prop == "MinimumRate" || prop == "MaximumRate") return Glib::Variant<double>::create(1.0);
    if (prop == "Shuffle") return B::create(p.shuffle());
    if (prop == "Metadata") return metadata();
    if (prop == "Volume") return Glib::Variant<double>::create(p.volume());
    if (prop == "Position") return Glib::Variant<gint64>::create(static_cast<gint64>(s.position) * 1000000);
    if (prop == "CanGoNext" || prop == "CanGoPrevious" || prop == "CanPlay" || prop == "CanPause" ||
        prop == "CanControl")
        return B::create(true);
    if (prop == "CanSeek") return B::create(s.has_track);
    return {};
}

std::string Mpris::art_url(int64_t id) {
    if (id == art_id_) return art_url_;
    art_id_ = id;
    art_url_.clear();
    const std::string bytes = Covers::get().track_bytes(id);
    if (bytes.empty()) return art_url_;
    // a fixed file name: the cover of the current track only.
    const auto file = paths::cache_dir() / ("mpris_cover_" + std::to_string(id));
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(paths::cache_dir(), ec))
        if (e.path().filename().string().rfind("mpris_cover_", 0) == 0) std::filesystem::remove(e.path(), ec);
    std::ofstream(file, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    art_url_ = Glib::filename_to_uri(file.string());
    return art_url_;
}

Glib::VariantBase Mpris::metadata() {
    Props m;
    auto& p = Player::get();
    const auto cur = p.current();
    if (!cur || !p.state().has_track) {
        m["mpris:trackid"] = Glib::Variant<Glib::DBusObjectPathString>::create("/org/mpris/MediaPlayer2/TrackList/NoTrack");
        return Glib::Variant<Props>::create(m);
    }
    const std::string id_path = "/dev/dreamplayer/track/" + std::to_string(cur->id);
    m["mpris:trackid"] = Glib::Variant<Glib::DBusObjectPathString>::create(id_path);
    m["xesam:title"] = Glib::Variant<Glib::ustring>::create(p.current_title());
    m["mpris:length"] = Glib::Variant<gint64>::create(static_cast<gint64>(p.state().duration) * 1000000);
    if (const auto* t = p.current_track()) {
        std::vector<Glib::ustring> artists(t->artists.begin(), t->artists.end());
        m["xesam:artist"] = Glib::Variant<std::vector<Glib::ustring>>::create(artists);
        if (!t->album.empty()) m["xesam:album"] = Glib::Variant<Glib::ustring>::create(t->album);
        if (!t->path.empty()) m["xesam:url"] = Glib::Variant<Glib::ustring>::create(Glib::filename_to_uri(t->path));
        if (const std::string art = art_url(t->id); !art.empty())
            m["mpris:artUrl"] = Glib::Variant<Glib::ustring>::create(art);
    }
    return Glib::Variant<Props>::create(m);
}

void Mpris::emit_changed(const Glib::ustring& iface, const std::vector<Glib::ustring>& props) {
    if (!conn_) return;
    Props changed;
    for (const auto& name : props) changed[name] = property(iface, name);
    try {
        conn_->emit_signal(kPath, "org.freedesktop.DBus.Properties", "PropertiesChanged", {},
                           Glib::VariantContainerBase::create_tuple(
                               {Glib::Variant<Glib::ustring>::create(iface), Glib::Variant<Props>::create(changed),
                                Glib::Variant<std::vector<Glib::ustring>>::create({})}));
    } catch (const Glib::Error& e) {
        Log::get().warn("dbus", e.what());
    }
}

}  // namespace dp
