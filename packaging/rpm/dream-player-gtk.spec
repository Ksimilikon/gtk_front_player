# Dream Player GTK for Fedora (and other rpm distros with GTK >= 4.10).
# Built by packaging/build.sh: Source0 — the app, Source1 — the core.
%global corever 0.1.4
# the core is a Rust cdylib: no separate debuginfo package.
%global debug_package %{nil}

Name:           dream-player-gtk
Version:        0.1.0
Release:        1%{?dist}
Summary:        Offline music player with sync between devices
License:        GPL-3.0-or-later AND MPL-2.0 AND OFL-1.1
URL:            https://github.com/ksimilikon/gtk_front_player
Source0:        gtk_front_player-%{version}.tar.gz
Source1:        rs_dream_player-%{corever}.tar.gz

BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  cargo
BuildRequires:  rust
BuildRequires:  pkgconfig(gtkmm-4.0)
BuildRequires:  pkgconfig(alsa)
BuildRequires:  pkgconfig(fontconfig)
BuildRequires:  pkgconfig(gdk-pixbuf-2.0)
BuildRequires:  pkgconfig(pangoft2)
BuildRequires:  desktop-file-utils
Requires:       hicolor-icon-theme
Recommends:     android-tools

%description
A GTK 4 desktop frontend for the rs_dream_player core: playlists, a play
queue, tag and cover editing, import / export of .dplr packets, sync with
other devices (TCP, USB through adb), core mods, MPRIS integration.

This project is created entirely with AI.

%prep
%setup -q -n gtk_front_player-%{version} -a 1

%build
%meson -Dcore_dir=%{_builddir}/gtk_front_player-%{version}/rs_dream_player-%{corever} -Dcore_profile=release -Dsource_data_fallback=false
%meson_build

%install
%meson_install
install -Dm644 rs_dream_player-%{corever}/LICENSE %{buildroot}%{_licensedir}/%{name}/LICENSE-core
install -Dm644 data/fonts/LICENSE-Terminus.txt %{buildroot}%{_licensedir}/%{name}/LICENSE-Terminus

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/dev.dreamplayer.gtk.desktop

%files
%license LICENSE
%{_licensedir}/%{name}/LICENSE-core
%{_licensedir}/%{name}/LICENSE-Terminus
%{_bindir}/dream_player_gtk
%{_libdir}/dream_player_gtk/
%{_datadir}/dream_player_gtk/
%{_datadir}/applications/dev.dreamplayer.gtk.desktop
%{_datadir}/icons/hicolor/scalable/apps/dev.dreamplayer.gtk.svg
%{_datadir}/metainfo/dev.dreamplayer.gtk.metainfo.xml

%changelog
* Wed Nov 18 2026 ksimilikon - 0.1.0-1
- First package.
