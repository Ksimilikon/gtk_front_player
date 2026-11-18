#!/usr/bin/env bash
# runs inside a Fedora container (see packaging/build.sh).
set -euo pipefail
dnf install -y rpm-build rpmdevtools dnf-plugins-core
top=/root/rpmbuild
mkdir -p "$top"/{SOURCES,SPECS}
cp /src/*.tar.gz "$top/SOURCES/"
cp /packaging/rpm/dream-player-gtk.spec "$top/SPECS/"
sed -i "s/^Version: .*/Version:        $APP_VERSION/; s/^%global corever .*/%global corever $CORE_VERSION/" \
    "$top/SPECS/dream-player-gtk.spec"
dnf builddep -y "$top/SPECS/dream-player-gtk.spec"
rpmbuild -bb "$top/SPECS/dream-player-gtk.spec"
find "$top/RPMS" -name '*.rpm' -exec cp {} /out/ \;
chown "$HOST_UID:$HOST_GID" /out/*.rpm
