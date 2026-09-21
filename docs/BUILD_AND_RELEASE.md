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
`install.ps1`, the bundled Microsoft runtimes, notices, a package manifest and a
SHA-256 sidecar. The package can be produced with:

```powershell
./package_sieve.ps1 -Configuration Release -Version 1.0.0 -BuildDirectory build
```

`Install.bat` elevates to administrator and invokes `install.ps1`. The installer
removes only the existing Sieve bundle, verifies `moduleinfo.json`, checks the
WebView2 and VC++ x64 runtime registry entries, copies the complete bundle and
verifies the installed files.

When a runtime is missing, the installer runs the copy bundled in
`Dependencies\` with the vendor's silent switches instead of requiring network
access. Both bundled files must carry a valid Microsoft Corporation
Authenticode signature or the installer refuses to run them. Cached downloads
live in the git-ignored `dependencies/` folder; refresh them with
`scripts/fetch_windows_dependencies.ps1`, which re-verifies the signature on
every run. `package_sieve.ps1` fetches anything missing automatically and
records versions and SHA-256 hashes in `Dependencies/dependencies.json` and
`PACKAGE_MANIFEST.txt`.

Verify the package without touching the system:

```powershell
./install.ps1 -DryRun
```

It reports whether each runtime is already installed or would be installed from
the bundle, and lists the plugin destinations.

## Apple GitHub Action

`.github/workflows/apple-release.yml` builds the Apple formats with the Xcode
generator on Apple Silicon and Intel macOS runners, plus a Universal build.
It checks out JUCE 8.0.12 at the exact commit used by the project, builds `VST3`,
`AU` and `Standalone`, and enables experimental `AUv3` in the arm64 job. It
validates the installed AU with auval, then uploads a ZIP and SHA-256 sidecar.

The workflow can be started from GitHub's Actions page or with GitHub CLI:

```bash
gh workflow run apple-release.yml --ref main
gh run watch
```

The reusable packaging step is `scripts/package_apple.sh`. It requires the
`Sieve.vst3`, `Sieve.component` and standalone app bundles. AUv3 is optional;
when enabled in CMake, packaging requires its extension inside the standalone
app. Package names and architecture checks come from the built binaries.
See [MACOS.md](MACOS.md) for local Universal builds and acceptance checks.

### Pulling Apple artifacts to a non-Apple workstation

macOS bundles cannot be produced without Apple toolchains, so `dist/` on a
Windows workstation is filled from CI:

```powershell
./scripts/fetch_apple_artifacts.ps1 [-RunId <id>] [-Version 1.0.0]
```

The script resolves the latest successful `apple-release.yml` run (or the given
run), downloads the `arm64`, `x86_64` and `universal` artifacts, verifies each
archive against its SHA-256 sidecar, and then calls
`scripts/verify_apple_packages.ps1`. That verifier reads the ZIP entries
directly instead of extracting them, because Windows extraction turns the
symlinks inside `.app`/`.vst3` bundles into ordinary files. It confirms which
Mach-O architectures are present in the VST3, AU and Standalone binaries and
whether the arm64 build embeds `Sieve.appex`.

Artifacts pulled this way are unsigned validation builds; signing and
notarization remain separate release steps.

The first Apple workflow intentionally produces unsigned validation artifacts.
Distribution signing, notarization, and TestFlight submission require Apple
certificates and protected GitHub secrets and must be added as a separate
release job after the unsigned build passes.

Do not install the generated plugin into a system plugin directory until it has
been inspected and accepted in a host.
