// D-Bus integration: an MPRIS 2 server (org.mpris.MediaPlayer2.dream_player_gtk)
// so media keys, the DE's player widget and `playerctl` control playback.
#pragma once

#include <giomm/dbusconnection.h>
#include <giomm/dbusintrospection.h>
#include <giomm/dbusownname.h>

#include <functional>
#include <map>

namespace dp {

class Mpris {
public:
    /// `raise` shows the window, `quit` closes the app.
    void start(std::function<void()> raise, std::function<void()> quit);
    void stop();

private:
    using Props = std::map<Glib::ustring, Glib::VariantBase>;

    void on_bus(const Glib::RefPtr<Gio::DBus::Connection>& conn, const Glib::ustring& name);
    void on_method(const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring& sender,
                   const Glib::ustring& path, const Glib::ustring& iface, const Glib::ustring& method,
                   const Glib::VariantContainerBase& params,
                   const Glib::RefPtr<Gio::DBus::MethodInvocation>& inv);
    void on_get(Glib::VariantBase& out, const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring& sender,
                const Glib::ustring& path, const Glib::ustring& iface, const Glib::ustring& prop);
    bool on_set(const Glib::RefPtr<Gio::DBus::Connection>&, const Glib::ustring& sender, const Glib::ustring& path,
                const Glib::ustring& iface, const Glib::ustring& prop, const Glib::VariantBase& value);

    Glib::VariantBase property(const Glib::ustring& iface, const Glib::ustring& prop);
    Glib::VariantBase metadata();
    std::string art_url(int64_t id);
    void emit_changed(const Glib::ustring& iface, const std::vector<Glib::ustring>& props);

    std::function<void()> raise_, quit_;
    guint owner_ = 0;
    Glib::RefPtr<Gio::DBus::Connection> conn_;
    Glib::RefPtr<Gio::DBus::NodeInfo> node_;
    std::vector<guint> registrations_;
    std::string last_status_;
    int64_t art_id_ = -1;
    std::string art_url_;
};

}  // namespace dp
