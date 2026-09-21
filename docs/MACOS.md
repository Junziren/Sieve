# Sieve on macOS

## Build

The desktop target is macOS 11 or newer, on Apple Silicon and Intel. The UI
uses the system WKWebView and embeds its HTML, CSS and JavaScript. No WebView2,
Node.js or web server is required at runtime.

Install Xcode (select it with `xcode-select`) and CMake 3.22 or newer. From the
repository root, obtain the project's pinned JUCE revision if JUCE is absent:

```bash
git clone --depth 1 --branch 8.0.12 https://github.com/juce-framework/JUCE.git JUCE
git -C JUCE rev-parse HEAD
# Expected: 29396c22c93392d6738e021b83196283d6e4d850
bash scripts/build_apple.sh
```

By default this produces Universal binaries containing `arm64` and `x86_64`,
with VST3, AU and Standalone formats. Artifacts are in
`build-mac/Source/Sieve_artefacts/Release`; the ZIP and SHA-256 sidecar are in
`dist`. Packaging reads the actual bundle version and architecture, and checks
every bundled executable for the required architectures before publishing the ZIP.

To build only the current architecture or use an external JUCE checkout:

```bash
SIEVE_ARCHITECTURES="$(uname -m)" SIEVE_JUCE_DIR=/path/to/JUCE \
  bash scripts/build_apple.sh
```

`BUILD_DIR` accepts an absolute path or a path relative to the repository.
`BUILD_JOBS` controls parallel compilation. Additional CMake arguments may be
passed to the build script. To package an existing Release build:

```bash
BUILD_DIR=build-mac bash scripts/package_apple.sh
```

AUv3 is experimental and opt-in with `SIEVE_BUILD_AUV3=ON`. It requires Xcode;
JUCE embeds the extension in `Sieve.app/Contents/PlugIns`. Install the containing
app, not a loose `.appex`. A Developer ID/signing and sandbox/file-import review
is still required before distributing AUv3. Use a separate build directory when
switching AUv3 on/off to avoid stale embedded extensions.

## Install

Quit your DAW. From the extracted archive, copy the **complete bundles**:

| Bundle | Per-user destination |
| --- | --- |
| `VST3/Sieve.vst3` | `~/Library/Audio/Plug-Ins/VST3/Sieve.vst3` |
| `AU/Sieve.component` | `~/Library/Audio/Plug-Ins/Components/Sieve.component` |
| `Standalone/Sieve.app` | `~/Applications/Sieve.app` |

Create destination folders if needed. When upgrading, replace the previous
Sieve bundle rather than merging its contents. Rescan plugins in the DAW.
Logic Pro and GarageBand use the AU version; VST3 hosts use the VST3 version.

CI archives are development validation builds without Developer ID notarization.
Gatekeeper may block downloaded builds. Public distribution requires signing
and notarization; the build scripts do not bypass Gatekeeper or remove quarantine.

## Verification status and acceptance

The Windows development environment can check script syntax, source contracts
and Windows builds, but cannot compile or execute Apple bundles. A configured
macOS minimum version is not proof of testing on that OS.

The Apple workflow builds arm64, Intel and Universal artifacts, checks packaged
architectures, and installs and runs `auval -v aumu Siev UPBL` on each runner.
Its arm64 job also builds the experimental AUv3 container. Results only exist
after that workflow runs successfully for the relevant commit.

Before a release, test on both Intel and Apple Silicon Macs:

- Run pluginval against the installed VST3 at strictness 10; run AU validation.
- Open in Logic/GarageBand and a VST3 DAW; verify offline UI rendering, Retina
  scaling, file chooser and drag/drop with WAV/AIFF/FLAC/OGG/MP3 files.
- Play MIDI; check automation, project save/reload, sample restoration,
  multiple instances, editor close/reopen and offline export.
- Check macOS 11 separately, since current CI runners use newer WebKit versions.
- Verify signed/notarized bundles on a clean Mac before public distribution.

AUv3 host registration, sandbox access and native WebKit interaction remain
manual acceptance items; an `auval` pass for AU does not validate AUv3 or the UI.
