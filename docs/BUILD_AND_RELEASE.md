# Build And Release

Sieve keeps build inputs explicit so a source checkout is reproducible without
embedding developer-machine paths in the project.

## Dependencies

- JUCE is supplied through `SIEVE_JUCE_DIR`. The default is the local `JUCE/`
  directory, which is intentionally ignored by git.
- Windows builds additionally require the Microsoft.Web.WebView2 NuGet package.
  Set `JUCE_WEBVIEW2_PACKAGE_LOCATION` to its package directory, or let JUCE's
  `FindWebView2.cmake` discover the current-user NuGet cache.
- Apple builds use the system WebKit backend and do not require WebView2.

## Formats

- Windows: VST3 and Standalone.
- macOS: VST3, AU and Standalone.
- macOS with the Xcode generator: AUv3 is also enabled.

## Archive Contents

A release archive should contain the plugin bundle plus the installer entrypoint,
`README.md`, `LICENSE.md` and `THIRD_PARTY_LICENSES.md`. It must not contain the JUCE source tree, the
WebView2 SDK, a developer's user-data folder or machine-specific build caches.

The Windows archive follows the Bloom Pad installer layout and contains the
complete `Sieve.vst3` bundle, `Standalone/Sieve.exe`, `Install.bat`,
`install.ps1`, notices and a SHA-256 sidecar. The package can be produced with:

```powershell
./package_sieve.ps1 -Configuration Release -Version 1.0.0 -BuildDirectory build
```

`Install.bat` elevates to administrator and invokes `install.ps1`. The installer
removes only the existing Sieve bundle, verifies `moduleinfo.json`, checks the
WebView2 and VC++ x64 runtime registry entries, copies the complete bundle and
verifies the installed files. It deliberately stops when a runtime is missing;
the ZIP does not bundle or replace shared Microsoft runtimes.

## Apple GitHub Action

`.github/workflows/apple-release.yml` builds the Apple formats with the Xcode
generator on both Apple Silicon and Intel macOS runners. It checks out JUCE
8.0.12 at the exact commit used by the project, builds `VST3`, `AU`,
`Standalone`, and `AUv3`, then uploads a ZIP and SHA-256 file for each
architecture.

The workflow can be started from GitHub's Actions page or with GitHub CLI:

```bash
gh workflow run apple-release.yml --ref main
gh run watch
```

The reusable packaging step is `scripts/package_apple.sh`. It requires the
`Sieve.vst3`, `Sieve.component`, standalone app, and `Sieve.appex` bundles, so
an accidental build that omits AUv3 fails instead of producing an incomplete
Apple archive.

The first Apple workflow intentionally produces unsigned validation artifacts.
Distribution signing, notarization, and TestFlight submission require Apple
certificates and protected GitHub secrets and must be added as a separate
release job after the unsigned build passes.

Do not install the generated plugin into a system plugin directory until it has
been inspected and accepted in a host.
