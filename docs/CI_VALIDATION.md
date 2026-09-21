# CI validation

The repository now uses GitHub Actions for the platform checks that cannot be
completed reliably on the Windows development machine.

## Windows VST3

`.github/workflows/windows-vst3.yml` checks out JUCE 8.0.12 at the pinned
commit, downloads the WebView2 SDK required by the CMake build, builds the
Release VST3 and Standalone targets, checks the complete bundle and runs
pluginval 1.0.4 at strictness level 5.

The pluginval invocation uses `--skip-gui-tests`. It covers the VST3 contract,
audio processing, MIDI/state/automation paths, buses and parameters. The only
remaining item for this job is UI integration in a real WebView2 host/DAW.

The CI summary deliberately states that limitation instead of treating a
headless pluginval pass as proof that the embedded WebView UI is visible in a
host editor.

## macOS AUv3

`.github/workflows/apple-release.yml` builds arm64, Intel and Universal Apple
artifacts. The arm64 job enables `SIEVE_BUILD_AUV3=ON`, verifies the AU with
`auval`, checks that `Sieve.appex` is embedded in `Sieve.app`, and packages the
VST3, AU and Standalone bundles into an arm64 ZIP with a SHA-256 sidecar.

The archive is an unsigned validation artifact. AUv3 host behavior, WebKit UI
integration, signing and notarization remain separate acceptance steps.
