#!/usr/bin/env bash
# runs inside a Debian / Ubuntu container (see packaging/build.sh) and
# produces dream-player-gtk_<version>-1~<distro>_<arch>.deb.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
MAINTAINER="${MAINTAINER:-ksimilikon <ksimilikon@users.noreply.github.com>}"

apt-get update
apt-get install -y --no-install-recommends \
    build-essential meson ninja-build pkg-config dpkg-dev file curl ca-certificates \
    libgtkmm-4.0-dev libasound2-dev libfontconfig-dev libgdk-pixbuf-2.0-dev libpango1.0-dev

# the core needs a recent Rust (edition 2024): rustup instead of the distro's.
curl -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain stable
export PATH="$HOME/.cargo/bin:$PATH"

work=/build
mkdir -p "$work"
tar -C "$work" -xzf "/src/gtk_front_player-$APP_VERSION.tar.gz"
tar -C "$work" -xzf "/src/rs_dream_player-$CORE_VERSION.tar.gz"

cd "$work"
meson setup build "gtk_front_player-$APP_VERSION" --prefix=/usr --buildtype=release \
    -Dcore_dir="$work/rs_dream_player-$CORE_VERSION" -Dcore_profile=release -Dsource_data_fallback=false
meson compile -C build
root="$work/root"
DESTDIR="$root" meson install -C build

# strip the binaries, ship the licenses the Debian way.
libdir="$(find "$root/usr/lib" -type d -name dream_player_gtk -path '*lib*' | head -1)"
strip --strip-unneeded "$root/usr/bin/dream_player_gtk" "$libdir/libdream_player.so"
doc="$root/usr/share/doc/dream-player-gtk"
install -d "$doc"
{
    echo "Dream Player GTK — GPL-3.0-or-later (the app), MPL-2.0 (the rs_dream_player core),"
    echo "OFL-1.1 (the bundled Terminus font). Full texts: /usr/share/dream_player_gtk/."
} > "$doc/copyright"

# runtime dependencies from the libraries the binaries link (the core is
# private: its directory is passed as a search path).
mkdir -p "$work/shl/debian"
printf 'Source: dream-player-gtk\n\nPackage: dream-player-gtk\n' > "$work/shl/debian/control"
depends="$(cd "$work/shl" && dpkg-shlibdeps -O --ignore-missing-info -l"$libdir" \
    "$root/usr/bin/dream_player_gtk" "$libdir/libdream_player.so" | sed -n 's/^shlibs:Depends=//p')"

arch="$(dpkg --print-architecture)"
size="$(du -sk "$root" | cut -f1)"
mkdir -p "$root/DEBIAN"
cat > "$root/DEBIAN/control" <<EOF
Package: dream-player-gtk
Version: $APP_VERSION-1~$DISTRO_TAG
Architecture: $arch
Maintainer: $MAINTAINER
Installed-Size: $size
Depends: $depends, hicolor-icon-theme
Recommends: adb
Section: sound
Priority: optional
Homepage: https://github.com/ksimilikon/gtk_front_player
Description: offline music player with sync between devices
 A GTK 4 desktop frontend for the rs_dream_player core: playlists, a play
 queue, tag and cover editing, import / export of .dplr packets, sync with
 other devices (TCP, USB through adb), core mods, MPRIS integration.
 .
 This project is created entirely with AI.
EOF

deb="/out/dream-player-gtk_${APP_VERSION}-1~${DISTRO_TAG}_${arch}.deb"
dpkg-deb --build --root-owner-group "$root" "$deb"
chown "$HOST_UID:$HOST_GID" "$deb"
echo "built: $deb"
