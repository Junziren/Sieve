// Browser regression checks. Install Playwright or expose it with NODE_PATH.
// Native bridge below is a test double; this does not claim DAW/native validation.
const { chromium } = require("playwright");
const assert = require("node:assert/strict");
const path = require("node:path");
const { pathToFileURL } = require("node:url");
const fs = require("node:fs");
(async () => {
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE,
  });
  const page = await browser.newPage({
    viewport: { width: 1100, height: 660 },
    deviceScaleFactor: 1,
  });
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await page.addInitScript(() => {
    window.testEvents = {};
    window.testCalls = [];
    window.__JUCE__ = {
      backend: {
        addEventListener: (n, fn) => (window.testEvents[n] = fn),
        emitEvent: (name, payload) => {
          window.testCalls.push(payload);
          queueMicrotask(() =>
            window.testEvents.__juce__complete({
              promiseId: payload.resultId,
              result: true,
            }),
          );
        },
      },
    };
  });
  await page.goto(pathToFileURL(path.resolve("Resources/web/index.html")).href);
  await page.waitForFunction(() =>
    document.getElementById("connection").classList.contains("connected"),
  );
  const out = path.resolve("build/ui-review");
  fs.mkdirSync(out, { recursive: true });
  await page.screenshot({ path: path.join(out, "sieve-empty.png") });
  // Host states must never produce writes, and arbitrary valid integer slices survive.
  await page.evaluate(() => {
    testCalls.length = 0;
    SieveUI.setParameterState({
      slices: 77,
      speed: 77,
      algorithm: 3,
      turbo: true,
    });
  });
  assert.equal(await page.locator("#slices").inputValue(), "77");
  assert.equal(
    await page.locator("[data-param=speed] input").inputValue(),
    "77 ms",
  );
  assert.equal(await page.evaluate(() => testCalls.length), 0);
  await page.locator("#algorithm").selectOption("2");
  assert.deepEqual(
    await page.evaluate(() =>
      testCalls
        .filter((x) => x.name === "setParameter")
        .map((x) => x.params[2]),
    ),
    ["begin", "change", "end"],
  );
  // Drag, cancellation, blur and wheel bursts must close automation exactly once.
  await page.evaluate(() => (testCalls.length = 0));
  const knob = page.locator("[data-param=speed]");
  const b = await knob.boundingBox();
  await page.mouse.move(b.x + b.width / 2, b.y + 25);
  await page.mouse.down();
  await page.mouse.move(b.x + b.width / 2, b.y + 5, { steps: 3 });
  await page.evaluate(() => window.dispatchEvent(new Event("blur")));
  await page.mouse.up();
  let phases = await page.evaluate(() =>
    testCalls.filter((x) => x.name === "setParameter").map((x) => x.params[2]),
  );
  assert.equal(phases.filter((x) => x === "begin").length, 1);
  assert.equal(phases.filter((x) => x === "end").length, 1);
  assert.equal(phases.at(-1), "end");
  await page.evaluate(() => (testCalls.length = 0));
  await page.mouse.move(b.x + b.width / 2, b.y + 25);
  await page.mouse.wheel(0, -10);
  await page.mouse.wheel(0, -10);
  await page.waitForTimeout(210);
  phases = await page.evaluate(() =>
    testCalls.filter((x) => x.name === "setParameter").map((x) => x.params[2]),
  );
  assert.equal(phases.filter((x) => x === "begin").length, 1);
  assert.equal(phases.filter((x) => x === "end").length, 1);
  // A parameter echo during a drag must not overwrite the local gesture.
  await page.mouse.move(b.x + b.width / 2, b.y + 25);
  await page.mouse.down();
  await page.mouse.move(b.x + b.width / 2, b.y + 10);
  const localValue = await knob.getAttribute("aria-valuenow");
  await page.evaluate(() => SieveUI.setParameterState({ speed: 400 }));
  assert.equal(await knob.getAttribute("aria-valuenow"), localValue);
  await knob.dispatchEvent("pointercancel");
  await page.mouse.up();
  // Exact input, units, keyboard, reset.
  const sustain = page.locator("[data-param=sustain] input");
  await sustain.fill("25");
  await sustain.press("Enter");
  assert.equal(await sustain.inputValue(), "25 %");
  await knob.focus();
  await knob.press("Home");
  assert.equal(await knob.getAttribute("aria-valuenow"), "1");
  await knob.press("ArrowUp");
  assert.equal(await knob.getAttribute("aria-valuenow"), "2");
  await knob.press("Shift+ArrowDown");
  assert.equal(await knob.getAttribute("aria-valuenow"), "1");
  await knob.press("End");
  assert.equal(await knob.getAttribute("aria-valuenow"), "400");
  await knob.locator("svg").dblclick();
  assert.equal(await knob.getAttribute("aria-valuenow"), "30");
  await page.evaluate(() => (testCalls.length = 0));
  await knob.locator("svg").click({ button: "right" });
  assert.equal(
    await page.evaluate(() => testCalls.at(-1).name),
    "parameterContext",
  );
  assert.equal(
    await page.evaluate(() => testCalls.at(-1).params[0]),
    "sortSpeed",
  );
  assert.equal(
    await page.evaluate(
      () => testCalls.filter((c) => c.name === "setParameter").length,
    ),
    0,
  );
  await page.locator("#btn-load").click();
  assert.equal(await page.locator("#btn-load").isDisabled(), true);
  await page.evaluate(() => testEvents.sieveStatus("Load cancelled"));
  assert.equal(await page.locator("#btn-load").isDisabled(), false);
  // Error must survive subsequent realtime and parameter snapshots.
  await page.evaluate(() => {
    testEvents.sieveStatus("Load failed: empty file");
    SieveUI.setParameterState({ slices: 128 });
    SieveUI.setUiFrame({ voices: [], playbackPosition: null });
  });
  assert.match(await page.locator("#footer-status").textContent(), /失败/);
  await page.screenshot({ path: path.join(out, "sieve-error.png") });
  // Explicit fixture data, never shipped as fake native activity.
  await page.evaluate(() => {
    SieveUI.setParameterState({
      algorithm: 0,
      slices: 128,
      speed: 30,
      turbo: false,
    });
    SieveUI.setSampleOverview({
      points: Array.from({ length: 256 }, (_, i) => {
        const a =
          0.1 +
          0.65 * Math.abs(Math.sin(i * 0.07)) * Math.abs(Math.cos(i * 0.023));
        return [-a, a];
      }),
      slices: 128,
    });
    SieveUI.setVoiceStates(
      Array.from({ length: 4 }, (_, i) => ({
        active: i < 2,
        paused: i === 1,
        completed: i === 2,
        midiNote: 60 + i * 4,
        progress: 0.37 + i * 0.2,
        sliceCount: 128,
        currentValues: Array.from({ length: 56 }, (_, n) =>
          i === 2 ? Math.round((n / 55) * 127) : (n * 17 + i * 7) % 128,
        ),
      })),
    );
    SieveUI.setPlaybackPosition({ normalized: 0.38 });
    document.getElementById("connection").textContent = "界面测试 · 模拟状态";
  });
  await page.locator("#dismiss-status").click();
  await page.screenshot({ path: path.join(out, "sieve-loaded.png") });
  await page.evaluate(() => SieveUI.setUiFrame({ playbackPosition: null }));
  assert.equal(await page.evaluate(() => wave.position), null);
  assert.equal(await page.locator(".viz-state").nth(1).textContent(), "已暂停");
  assert.equal(await page.locator(".viz-state").nth(2).textContent(), "已完成");
  await page.locator("#brand").click();
  await page.screenshot({ path: path.join(out, "sieve-about.png") });
  await page.keyboard.press("Tab");
  assert.equal(
    await page.evaluate(() => document.activeElement.id),
    "close-about",
  );
  await page.keyboard.press("Escape");
  assert.equal(await page.evaluate(() => document.activeElement.id), "brand");
  const geometry = await page.evaluate(() => ({
    root: [
      document.documentElement.scrollWidth,
      document.documentElement.scrollHeight,
    ],
    sortBottom: document.querySelector(".sort-controls").getBoundingClientRect()
      .bottom,
    panelBottom: document.getElementById("panel-sort").getBoundingClientRect()
      .bottom,
  }));
  console.log("Geometry:", geometry);
  assert.ok(
    geometry.sortBottom <= geometry.panelBottom,
    "Sort controls overflow",
  );
  assert.deepEqual(geometry.root, [1100, 660]);
  const layout = await page.evaluate(() => {
    const shape = document
      .getElementById("panel-synth")
      .getBoundingClientRect();
    const viz = document.getElementById("panel-viz").getBoundingClientRect();
    const overflow = [
      ...document.querySelectorAll(
        "#panel-synth .knob, #panel-synth .shape-display",
      ),
    ].some((el) => {
      const r = el.getBoundingClientRect();
      return r.right > shape.right || r.left < shape.left;
    });
    return {
      overflow,
      shapeRight: shape.right,
      vizLeft: viz.left,
      voiceHeight: document
        .querySelector(".viz-row canvas")
        .getBoundingClientRect().height,
    };
  });
  assert.equal(layout.overflow, false);
  assert.ok(
    layout.shapeRight < layout.vizLeft,
    "ADSR belongs to the left column",
  );
  assert.ok(
    layout.voiceHeight >= 38,
    "Voice plots should have larger vertical space",
  );
  await page.emulateMedia({ reducedMotion: "reduce" });
  assert.equal(
    await page.evaluate(
      () =>
        document.getAnimations().filter((a) => a.playState === "running")
          .length,
    ),
    0,
  );
  await page.setViewportSize({ width: 900, height: 620 });
  await page.screenshot({ path: path.join(out, "sieve-compact.png") });
  const compact = await page.evaluate(() => ({
    root: [
      document.documentElement.scrollWidth,
      document.documentElement.scrollHeight,
    ],
    sortBottom: document.querySelector(".sort-controls").getBoundingClientRect()
      .bottom,
    panelBottom: document.getElementById("panel-sort").getBoundingClientRect()
      .bottom,
  }));
  assert.deepEqual(compact.root, [900, 620]);
  assert.ok(
    compact.sortBottom <= compact.panelBottom,
    "Compact sort controls overflow",
  );
  assert.deepEqual(errors, []);
  await browser.close();
  console.log(
    "PASS: parameter restore, gestures, exact entry, keyboard/reset, persistent error, voice states, modal and layout. Screenshots:",
    out,
  );
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
