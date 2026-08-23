# Third-Party Notices

This file records the dependencies that are relevant to source builds and release
archives. The dependency source trees retain their own license files.

## JUCE 8

The project uses the JUCE framework from the local `JUCE/` source tree. JUCE 8
modules are available under the AGPLv3 or the commercial JUCE license. See
`JUCE/LICENSE.md` and the JUCE license terms before distributing a binary.

## JUCE Bundled Dependencies

JUCE includes or references several third-party components, including AudioUnitSDK,
VST3 SDK, FLAC, Ogg Vorbis, zlib, libpng, HarfBuzz, SheenBidi, CHOC and others.
Their license files are kept in the corresponding `JUCE/modules/` directories;
the authoritative index is `JUCE/LICENSE.md`.

## Microsoft WebView2

Windows builds use the statically linked WebView2 loader from the Microsoft.Web.WebView2
NuGet package. The SDK is a build dependency and is not copied into the plugin
archive. End users need the Microsoft Edge WebView2 Runtime installed on Windows.
The package and runtime remain subject to Microsoft's terms.

## Fonts and UI Assets

The current WebView uses system font fallbacks and contains no remote font or CDN
dependency. Any future bundled FAD family font or identity asset must add its own
copyright and license notice here before release.
