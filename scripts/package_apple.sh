#!/usr/bin/env bash
set -euo pipefail

[[ "$(uname -s)" == Darwin ]] || { echo 'Packaging requires macOS.' >&2; exit 1; }
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-build-mac}"
[[ "$build_dir" = /* ]] || build_dir="$root_dir/$build_dir"
artifact_root="$build_dir/Source/Sieve_artefacts/Release"
[[ -f "$build_dir/CMakeCache.txt" ]] || { echo 'Missing build configuration' >&2; exit 1; }
vst_binary="$artifact_root/VST3/Sieve.vst3/Contents/MacOS/Sieve"
[[ -f "$vst_binary" ]] || { echo "Missing binary: $vst_binary" >&2; exit 1; }
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$artifact_root/VST3/Sieve.vst3/Contents/Info.plist")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Invalid bundle version' >&2; exit 1; }
architectures="$(lipo -archs "$vst_binary")"
case "$architectures" in
    'arm64 x86_64'|'x86_64 arm64') architecture=universal ;;
    arm64|x86_64) architecture="$architectures" ;;
    *) echo "Unsupported architectures: $architectures" >&2; exit 1 ;;
esac
if [[ -n "${SIEVE_ARCH:-}" && "$SIEVE_ARCH" != "$architecture" ]]; then
    echo "Expected $SIEVE_ARCH, found $architecture" >&2; exit 1
fi
package_name="Sieve-v${version}-macos-${architecture}"
dist_root="$root_dir/dist"
archive="$dist_root/$package_name.zip"
mkdir -p "$dist_root"
staging="$(mktemp -d "$dist_root/.apple-package.XXXXXX")"
trap 'rm -rf "$staging"' EXIT
package_root="$staging/$package_name"

copy_bundle() {
    local source="$1"
    local destination="$2"

    if [[ ! -d "$source" ]]; then
        echo "Missing Apple bundle: $source" >&2
        exit 1
    fi

    local executable
    executable="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$source/Contents/Info.plist")"
    local arch
    for arch in $architectures; do
        lipo -verify_arch "$arch" "$source/Contents/MacOS/$executable"
    done

    mkdir -p "$(dirname "$destination")"
    ditto "$source" "$destination"
}

if [[ ! -d "$artifact_root" ]]; then
    echo "Apple build artifacts not found: $artifact_root" >&2
    exit 1
fi

mkdir -p "$package_root"

copy_bundle "$artifact_root/VST3/Sieve.vst3" "$package_root/VST3/Sieve.vst3"
copy_bundle "$artifact_root/AU/Sieve.component" "$package_root/AU/Sieve.component"
copy_bundle "$artifact_root/Standalone/Sieve.app" "$package_root/Standalone/Sieve.app"

# AUv3 is installed through its containing app, never as a loose extension.
appex="$artifact_root/Standalone/Sieve.app/Contents/PlugIns/Sieve.appex"
if grep -q '^SIEVE_BUILD_AUV3:BOOL=ON$' "$build_dir/CMakeCache.txt"; then
    [[ -d "$appex" ]] || { echo "Missing embedded AUv3: $appex" >&2; exit 1; }
    copy_bundle "$appex" "$package_root/Standalone/Sieve.app/Contents/PlugIns/Sieve.appex"
elif [[ -d "$appex" ]]; then
    echo 'Stale AUv3 extension in an AUv3-disabled build. Use a fresh build directory.' >&2
    exit 1
fi

cp "$root_dir/README.md" "$package_root/README.md"
cp "$root_dir/LICENSE.md" "$package_root/LICENSE.md"
cp "$root_dir/THIRD_PARTY_LICENSES.md" "$package_root/THIRD_PARTY_LICENSES.md"
cp "$root_dir/docs/MACOS.md" "$package_root/MACOS.md"

ditto -c -k --sequesterRsrc --keepParent "$package_root" "$staging/package.zip"
mv -f "$staging/package.zip" "$archive"
(
    cd "$dist_root"
    shasum -a 256 "$(basename "$archive")"
) | tee "$archive.sha256"

echo "Apple package: $archive"
