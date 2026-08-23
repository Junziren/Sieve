#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-build}"
version="${SIEVE_VERSION:-1.0.0}"
architecture="${SIEVE_ARCH:-$(uname -m)}"
artifact_root="$root_dir/$build_dir/Source/Sieve_artefacts/Release"
package_name="Sieve-v${version}-macos-${architecture}"
dist_root="$root_dir/dist"
package_root="$dist_root/$package_name"
archive="$dist_root/$package_name.zip"

copy_bundle() {
    local source="$1"
    local destination="$2"

    if [[ ! -d "$source" ]]; then
        echo "Missing Apple bundle: $source" >&2
        exit 1
    fi

    mkdir -p "$(dirname "$destination")"
    ditto "$source" "$destination"
}

if [[ ! -d "$artifact_root" ]]; then
    echo "Apple build artifacts not found: $artifact_root" >&2
    exit 1
fi

rm -rf "$package_root" "$archive" "$archive.sha256"
mkdir -p "$package_root"

copy_bundle "$artifact_root/VST3/Sieve.vst3" "$package_root/VST3/Sieve.vst3"
copy_bundle "$artifact_root/AU/Sieve.component" "$package_root/AU/Sieve.component"
copy_bundle "$artifact_root/Standalone/Sieve.app" "$package_root/Standalone/Sieve.app"

appex="$(find "$artifact_root" -type d -name 'Sieve.appex' -print -quit)"
if [[ -z "$appex" ]]; then
    echo "AUv3 app extension was not produced under: $artifact_root" >&2
    exit 1
fi
copy_bundle "$appex" "$package_root/AUv3/Sieve.appex"

cp "$root_dir/README.md" "$package_root/README.md"
cp "$root_dir/LICENSE.md" "$package_root/LICENSE.md"
cp "$root_dir/THIRD_PARTY_LICENSES.md" "$package_root/THIRD_PARTY_LICENSES.md"

ditto -c -k --sequesterRsrc --keepParent "$package_root" "$archive"
shasum -a 256 "$archive" | tee "$archive.sha256"

echo "Apple package: $archive"
