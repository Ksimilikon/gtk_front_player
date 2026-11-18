#!/bin/sh
# builds the rs_dream_player core (cdylib, feature `modding`) and copies the
# library to the meson output path.
# usage: build_core.sh <cargo> <core_dir> <target_dir> <profile> <output>
set -e
cargo="$1"; core_dir="$2"; target_dir="$3"; profile="$4"; out="$5"
flag=""
[ "$profile" = "release" ] && flag="--release"
# no build machine paths inside the library (panic locations of the core and
# of the crates): they are remapped to short neutral prefixes.
cargo_home="${CARGO_HOME:-$HOME/.cargo}"
sysroot="$(rustc --print sysroot 2>/dev/null || true)"
export RUSTFLAGS="${RUSTFLAGS:-} --remap-path-prefix=$core_dir=/rs_dream_player --remap-path-prefix=$cargo_home=/cargo"
[ -n "$sysroot" ] && RUSTFLAGS="$RUSTFLAGS --remap-path-prefix=$sysroot=/rust"
CARGO_TARGET_DIR="$target_dir" "$cargo" build --lib --features modding $flag \
    --manifest-path "$core_dir/Cargo.toml"
cp "$target_dir/$profile/libdream_player.so" "$out"
