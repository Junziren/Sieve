# Sieve UI revision

The active UI is `Resources/web/index.html`, `app.css` and `app.js`, embedded
through `SieveWebUI`. This revision uses a standalone cool-grey/blue studio
layout designed around Sieve's sample-to-sort workflow. It does not depend on
the FAD OS design skill, fonts, artwork, menus or services.

## Workflow and behavior

- Sorting controls and ADSR/output occupy the left column; the four voice plots
  span the full right column, with metadata above each larger bar display.
- The waveform section owns both the load button and the file drop target.
- Algorithm selection uses a native HTML select with keyboard support. Slice
  count supports every integer from 4 to 512, including host-restored values
  outside the shortcut buttons.
- “Step interval” replaces the ambiguous “Speed” label; smaller values mean
  faster stepping. Turbo copy describes actual accelerated sorting behavior.
- Knobs support relative drag, Shift fine adjustment, one gesture per wheel
  burst, keyboard adjustment, direct numeric input and double-click reset.
  Sustain input is percent; pan input is -100 to 100; gain is a multiplier.
- Parameter range, step, skew and default metadata come from APVTS at the native
  handshake. Host updates do not echo back and cannot interrupt an active edit.
- Voice rows distinguish running, paused, completed and idle; playback markers
  clear when the native frame has no playing voice. Paused order remains visible.
- Action errors persist until cleared or replaced by a subsequent action.
  Realtime voice frames do not overwrite them. Cancelling the file chooser
  restores the load button.
- The ADSR plot is a parameter schematic with compressed time spacing, not a
  measured audio envelope. Browser preview has no generated audio or fake data.

The processor, DSP, parameter IDs/order/ranges and saved-state schema are
unchanged. Native additions are editor-only: metadata, host parameter menus,
surface visibility and gesture tracking. C++ closes outstanding gestures on
hide/destruction, and skips telemetry work while hidden or About is open.

## Checks

Run the browser regression with Node and Playwright available:

```bash
node scripts/test_web_ui.cjs
```

`NODE_PATH` can point to an existing Playwright installation.
`PLAYWRIGHT_CHROMIUM_EXECUTABLE` can select an installed Chromium executable.
Screenshots are written to `build/ui-review/`.

The test uses an explicitly mocked JUCE bridge. It checks host restoration,
gesture boundaries, drag cancellation, native echoes during drag, wheel bursts,
numeric units, keyboard/reset, menu command routing, chooser cancellation,
persistent errors, playback marker clearing, voice states, About focus and
1100×660 / 900×620 layout. Loaded-state screenshot data are fixtures, labelled
as such. They do not prove native WebView or DAW behavior.

Windows VST3 and Standalone compile checks are separate from browser tests.
Manual acceptance still includes real WebView2/WKWebView rendering, host-native
parameter menus, DAW automation recording and file chooser/drop interaction.
The native editor remains fixed at 1100×660; compact browser checks provide
layout headroom, not a new user-resizable editor feature.
