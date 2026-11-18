#!/usr/bin/env bash
# Builds installable packages of Dream Player GTK.
#
#   packaging/build.sh [targets...]
#
# targets:
#   arch     Arch Linux / Manjaro / EndeavourOS  — .pkg.tar.zst (makepkg on the
#            host if it is Arch, otherwise in an archlinux container)
#   debian   Debian 13 (trixie)                  — .deb  (container)
#   ubuntu   Ubuntu 24.04 LTS (noble)            — .deb  (container)
#   fedora   Fedora (latest)                     — .rpm  (container)
#   all      every target above (default)
#
# Every package is built inside its own distro (a container), so it links
# against that distro's GTK / glibc and installs there with its package
# manager. Containers need docker or podman; the sources of the core are
# taken from CORE_DIR (default: ../rs_dream_player).
#
# The results land in packaging/out/.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "$HERE/.." && pwd)"
CORE_DIR="$(cd "${CORE_DIR:-$APP_DIR/../rs_dream_player}" && pwd)"
OUT="$HERE/out"
SRC="$OUT/src"

APP_VERSION="$(sed -n "s/^  version : '\(.*\)',/\1/p" "$APP_DIR/meson.build" | head -1)"
CORE_VERSION="$(sed -n 's/^version *= *"\(.*\)"/\1/p' "$CORE_DIR/Cargo.toml" | head -1)"
export APP_VERSION CORE_VERSION

engine() {
    if command -v podman >/dev/null; then echo podman
    elif command -v docker >/dev/null; then echo docker
    else echo "need docker or podman for container builds" >&2; exit 1
    fi
}

# source tarballs of the working trees (uncommitted changes included),
# without build output.
make_sources() {
    mkdir -p "$SRC"
    tar -C "$(dirname "$APP_DIR")" --transform "s,^$(basename "$APP_DIR"),gtk_front_player-$APP_VERSION," \
        --exclude-vcs --exclude="$(basename "$APP_DIR")/build" --exclude="$(basename "$APP_DIR")/packaging/out" \
        -czf "$SRC/gtk_front_player-$APP_VERSION.tar.gz" "$(basename "$APP_DIR")"
    tar -C "$(dirname "$CORE_DIR")" --transform "s,^$(basename "$CORE_DIR"),rs_dream_player-$CORE_VERSION," \
        --exclude-vcs --exclude=target \
        -czf "$SRC/rs_dream_player-$CORE_VERSION.tar.gz" "$(basename "$CORE_DIR")"
    echo "sources: $SRC"
}

# runs packaging/<script> inside <image> with the sources and the output dir.
in_container() {
    local image="$1" script="$2" tag="$3"
    local e; e="$(engine)"
    echo "── $tag: $image ──"
    "$e" run --rm \
        -e APP_VERSION -e CORE_VERSION -e DISTRO_TAG="$tag" -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
        -v "$SRC:/src:ro" -v "$HERE:/packaging:ro" -v "$OUT:/out" \
        "$image" bash "/packaging/$script"
}

build_arch() {
    local work="$OUT/arch-work"
    rm -rf "$work"
    mkdir -p "$work"
    cp "$HERE/arch/PKGBUILD" "$SRC"/*.tar.gz "$work/"
    sed -i "s/^pkgver=.*/pkgver=$APP_VERSION/; s/^_corever=.*/_corever=$CORE_VERSION/" "$work/PKGBUILD"
    if command -v makepkg >/dev/null && [ -f /etc/arch-release ]; then
        echo "── arch: makepkg on the host ──"
        (cd "$work" && PKGDEST="$OUT" makepkg -f --noconfirm)
    else
        in_container docker.io/library/archlinux:latest arch/build-arch.sh arch
    fi
}

main() {
    local targets=("$@")
    [ ${#targets[@]} -eq 0 ] && targets=(all)
    [ "${targets[0]}" = all ] && targets=(arch debian ubuntu fedora)
    mkdir -p "$OUT"
    make_sources
    for t in "${targets[@]}"; do
        case "$t" in
            arch) build_arch ;;
            debian) in_container docker.io/library/debian:trixie deb/build-deb.sh debian13 ;;
            ubuntu) in_container docker.io/library/ubuntu:24.04 deb/build-deb.sh ubuntu24.04 ;;
            fedora) in_container registry.fedoraproject.org/fedora:latest rpm/build-rpm.sh fedora ;;
            *) echo "unknown target: $t" >&2; exit 1 ;;
        esac
    done
    echo
    echo "packages:"
    ls -1 "$OUT"/*.{pkg.tar.zst,deb,rpm} 2>/dev/null || true
}

main "$@"
