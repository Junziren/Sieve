#!/usr/bin/env bash
set -euo pipefail

[[ "$(uname -s)" == Darwin ]] || { echo 'This build requires macOS and Xcode.' >&2; exit 1; }
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$root_dir/build-mac}"
[[ "$build_dir" = /* ]] || build_dir="$root_dir/$build_dir"
xcode-select -p >/dev/null
cmake -S "$root_dir" -B "$build_dir" -G Xcode \
    -DCMAKE_OSX_ARCHITECTURES="${SIEVE_ARCHITECTURES:-arm64;x86_64}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-11.0}" \
    -DSIEVE_JUCE_DIR="${SIEVE_JUCE_DIR:-$root_dir/JUCE}" \
    -DSIEVE_BUILD_AUV3="${SIEVE_BUILD_AUV3:-OFF}" "$@"
cmake --build "$build_dir" --config Release --target Sieve_All --parallel "${BUILD_JOBS:-3}"
BUILD_DIR="$build_dir" bash "$root_dir/scripts/package_apple.sh"
