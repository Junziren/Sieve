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

A release archive should contain the plugin bundle plus `README.md`, `LICENSE.md`
and `THIRD_PARTY_LICENSES.md`. It must not contain the JUCE source tree, the
WebView2 SDK, a developer's user-data folder or machine-specific build caches.

The Windows archive can be produced with:

```powershell
./package_sieve.ps1 -Configuration Release -Version 1.0.0 -BuildDirectory build
```

Do not install the generated plugin into a system plugin directory until it has
been inspected and accepted in a host.
