#!/bin/sh
# builds the rs_dream_player core (cdylib, feature `modding`) and copies the
# library to the meson output path.
# usage: build_core.sh <cargo> <core_dir> <target_dir> <profile> <output>
set -e
cargo="$1"; core_dir="$2"; target_dir="$3"; profile="$4"; out="$5"
flag=""
[ "$profile" = "release" ] && flag="--release"
CARGO_TARGET_DIR="$target_dir" "$cargo" build --lib --features modding $flag \
    --manifest-path "$core_dir/Cargo.toml"
cp "$target_dir/$profile/libdream_player.so" "$out"
