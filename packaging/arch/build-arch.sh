#!/usr/bin/env bash
# runs inside an archlinux container (see packaging/build.sh).
set -euo pipefail
pacman -Syu --noconfirm --needed base-devel meson ninja cargo pkgconf gtkmm-4.0 alsa-lib fontconfig sudo git
# makepkg refuses to run as root.
useradd -m builder
work=/home/builder/work
mkdir -p "$work"
cp /packaging/arch/PKGBUILD /src/*.tar.gz "$work/"
sed -i "s/^pkgver=.*/pkgver=$APP_VERSION/; s/^_corever=.*/_corever=$CORE_VERSION/" "$work/PKGBUILD"
chown -R builder: "$work"
su builder -c "cd $work && makepkg -f --noconfirm"
cp "$work"/*.pkg.tar.zst /out/
chown "$HOST_UID:$HOST_GID" /out/*.pkg.tar.zst
