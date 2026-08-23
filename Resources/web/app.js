/* Sieve WebView UI — native DSP/APVTS surface.
   Control set mirrors the plugin 1:1:
   algorithm / slices / speed / duration / attack / decay / sustain /
   release / gain / pan / turbo + load action + waveform +
   4-voice sort visualizer + status bar + About easter egg. */

"use strict";

// ?static disables entrance animations (deterministic screenshots)
if (location.search.includes("static"))
  document.documentElement.classList.add("no-anim");

const params = {
  algorithm: "bubble",
  slices: 128,
  speed: 30,        // ms per sort step (1–400)
  duration: 1.0,    // grain duration factor (0.05–5.0)
  attack: 5, decay: 100, sustain: 0.7, release: 80,
  gain: 0.8, pan: 0.0,
  turbo: false,
};

const ALGOS = [
  { id: "bubble",    name: "Bubble",    complexity: "O(n\u00B2)" },
  { id: "insertion", name: "Insertion", complexity: "O(n\u00B2)" },
  { id: "selection", name: "Selection", complexity: "O(n\u00B2)" },
  { id: "quick",     name: "Quick",     complexity: "O(n log n)" },
  { id: "merge",     name: "Merge",     complexity: "O(n log n)" },
  { id: "shell",     name: "Shell",     complexity: "O(n^1.3)" },
  { id: "heap",      name: "Heap",      complexity: "O(n log n)" },
  { id: "shaker",    name: "Shaker",    complexity: "O(n\u00B2)" },
  { id: "bogo",      name: "Bogo",      complexity: "O(n!)" },
];

const KNOB_DEFS = {
  speed:    { label: "SPEED",    id: "sortSpeed",    min: 1,     max: 400,  def: 30,   fmt: v => v.toFixed(0) + " ms" },
  duration: { label: "DURATION", id: "grainDuration", min: 0.05,  max: 5.0,  def: 1.0,  fmt: v => v.toFixed(2) + " \u00D7" },
  attack:   { label: "ATTACK",   id: "attack",       min: 1,     max: 500,  def: 5,    fmt: v => v.toFixed(0) + " ms" },
  decay:    { label: "DECAY",    id: "decay",        min: 5,     max: 1000, def: 100,  fmt: v => v.toFixed(0) + " ms" },
  sustain:  { label: "SUSTAIN",  id: "sustain",      min: 0,     max: 1,    def: 0.7,  fmt: v => Math.round(v * 100) + " %" },
  release:  { label: "RELEASE",  id: "release",      min: 5,     max: 2000, def: 80,   fmt: v => v.toFixed(0) + " ms" },
  gain:     { label: "GAIN",     id: "gain",         min: 0,     max: 1.5,  def: 0.8,  fmt: v => v.toFixed(2) },
  pan:      { label: "PAN",      id: "pan",          min: -1,    max: 1,    def: 0,    fmt: v => (v === 0 ? "C" : (v < 0 ? "L" : "R") + Math.round(Math.abs(v) * 100)) },
};

const HELP_TEXT = {
  algorithm: "Chooses the ordering algorithm used to sort the randomized grain order.",
  slices: "Sets how many grains the loaded sample is divided into.",
  speed: "Sets the delay between sort steps. Lower values advance faster.",
  duration: "Scales the playback length of each grain.",
  attack: "Sets how quickly each grain reaches its peak level.",
  decay: "Sets how quickly each grain falls from its peak to sustain level.",
  sustain: "Sets the held level of each grain after the decay stage.",
  release: "Sets how long each grain fades out after note-off.",
  gain: "Sets the output level applied to all grain voices.",
  pan: "Moves the grain output between the left and right channels.",
  turbo: "Advances sorting as quickly as possible. Higher values can raise CPU use.",
  load: "Loads a WAV, AIFF, FLAC, OGG, or MP3 sample into the grain engine.",
  waveform: "Shows the static overview of the loaded sample. It is not a live oscilloscope.",
  visualizer: "Shows the current grain ordering reported by the sorting voices.",
};

const nativePromises = new Map();
let nextNativePromiseId = 0;
let nativeBridgeConnected = false;
let nativeEventsBound = false;

function nativeCall(name, ...args) {
  const backend = window.__JUCE__ && window.__JUCE__.backend;
  if (!backend || typeof backend.emitEvent !== "function") return Promise.resolve(false);

  const promiseId = nextNativePromiseId++;
  const result = new Promise(resolve => nativePromises.set(promiseId, resolve));
  backend.emitEvent("__juce__invoke", { name, params: args, resultId: promiseId });
  return result;
}

function bindNativeBridge() {
  const backend = window.__JUCE__ && window.__JUCE__.backend;
  if (!backend || typeof backend.addEventListener !== "function") return;

  nativeBridgeConnected = true;
  if (!nativeEventsBound) {
    nativeEventsBound = true;
    backend.addEventListener("__juce__complete", ({ promiseId, result }) => {
      const resolve = nativePromises.get(promiseId);
      if (!resolve) return;
      nativePromises.delete(promiseId);
      resolve(result);
    });
    backend.addEventListener("sieveParameterState", state => applyParameterState(state));
    backend.addEventListener("sieveSampleOverview", data => wave.setSampleOverview(data));
    backend.addEventListener("sieveUiFrame", frame => applyUiFrame(frame));
    backend.addEventListener("sieveDropState", active => setDropActive(Boolean(active)));
    backend.addEventListener("sieveStatus", message => {
      document.getElementById("footer-status").textContent = String(message || "");
    });
  }
}

function nativeParameter(param, value, phase) {
  if (!nativeBridgeConnected) return;
  const id = param === "slices" ? "sliceCount"
    : param === "turbo" ? "performanceMode"
    : KNOB_DEFS[param]?.id || param;
  nativeCall("setParameter", id, value, phase);
}

/* ================= knob ================= */
class Knob {
  constructor(root) {
    this.root = root;
    this.param = root.dataset.param;
    this.def_ = KNOB_DEFS[this.param];
    this.value = this.def_.def;
    root.dataset.tooltip = HELP_TEXT[this.param] || "Adjust this parameter.";
    this.build(root);
    this.bind(root);
    this.render();
  }
  build(root) {
    root.innerHTML = `
      <div class="k-label">${this.def_.label}</div>
      <svg viewBox="0 0 52 52">
        <defs>
          <linearGradient id="kg-${this.param}" x1="0" y1="1" x2="1" y2="0">
            <stop offset="0" stop-color="#7c3aed"/><stop offset="1" stop-color="#c9a6ff"/>
          </linearGradient>
        </defs>
        <circle cx="26" cy="26" r="23" fill="#17171c"/>
        <path class="k-track" fill="none" stroke="#1e1e25" stroke-width="3.5" stroke-linecap="round"/>
        <path class="k-arc" fill="none" stroke="url(#kg-${this.param})" stroke-width="3.5" stroke-linecap="round"/>
        <circle cx="26" cy="26" r="16.5" fill="url(#kg-${this.param})" opacity="0.06"/>
        <circle cx="26" cy="26" r="16.5" fill="#232329" stroke="#0e0e12" stroke-width="1"/>
        <line class="k-pointer" x1="26" y1="26" x2="26" y2="13.5" stroke="#fff" stroke-width="2" stroke-linecap="round"/>
      </svg>
      <div class="k-value"></div>`;
    this.svg = root;
    this.track = root.querySelector(".k-track");
    this.arc = root.querySelector(".k-arc");
    this.pointer = root.querySelector(".k-pointer");
    this.valueEl = root.querySelector(".k-value");
  }
  bind(root) {
    let dragging = false, lastY = 0;
    const finishGesture = () => {
      if (!dragging) return;
      dragging = false;
      root.classList.remove("dragging");
      nativeParameter(this.param, this.value, "end");
    };
    root.addEventListener("pointerdown", e => {
      dragging = true; lastY = e.clientY;
      root.classList.add("dragging");
      nativeParameter(this.param, this.value, "begin");
      root.setPointerCapture(e.pointerId);
    });
    root.addEventListener("pointermove", e => {
      if (!dragging) return;
      const fine = e.shiftKey ? 0.2 : 1;
      const dy = lastY - e.clientY; lastY = e.clientY;
      const range = this.def_.max - this.def_.min;
      this.set(this.value + (dy / 180) * range * fine, "change");
    });
    root.addEventListener("pointerup", e => {
      finishGesture();
      if (root.hasPointerCapture(e.pointerId)) root.releasePointerCapture(e.pointerId);
    });
    root.addEventListener("pointercancel", finishGesture);
    root.addEventListener("lostpointercapture", finishGesture);
    root.addEventListener("dblclick", () => {
      nativeParameter(this.param, this.value, "begin");
      this.set(this.def_.def, "change");
      nativeParameter(this.param, this.value, "end");
    });
    root.addEventListener("wheel", e => {
      e.preventDefault();
      const range = this.def_.max - this.def_.min;
      nativeParameter(this.param, this.value, "begin");
      this.set(this.value + Math.sign(-e.deltaY) * range * 0.03, "change");
      nativeParameter(this.param, this.value, "end");
    }, { passive: false });
  }
  set(v, phase = null) {
    this.value = Math.min(this.def_.max, Math.max(this.def_.min, v));
    params[this.param] = this.value;
    this.render();
    if (phase) nativeParameter(this.param, this.value, phase);
    if (this.param === "speed") updateStatus();
  }
  setFromNative(v) {
    this.value = Number(v);
    params[this.param] = this.value;
    this.render();
  }
  render() {
    const t = (this.value - this.def_.min) / (this.def_.max - this.def_.min);
    const a0 = -135, a1 = 135, a = a0 + t * (a1 - a0);
    const arcPath = (from, to) => {
      const pt = deg => {
        const r = (deg - 90) * Math.PI / 180;
        return [26 + 19.5 * Math.cos(r), 26 + 19.5 * Math.sin(r)];
      };
      const [x0, y0] = pt(from), [x1, y1] = pt(to);
      const large = (to - from) > 180 ? 1 : 0;
      return `M ${x0.toFixed(2)} ${y0.toFixed(2)} A 19.5 19.5 0 ${large} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
    };
    this.track.setAttribute("d", arcPath(a0, a1));
    this.arc.setAttribute("d", arcPath(a0, Math.max(a0 + 0.01, a)));
    const pr = (a - 90) * Math.PI / 180;
    this.pointer.setAttribute("x2", (26 + 11 * Math.cos(pr)).toFixed(2));
    this.pointer.setAttribute("y2", (26 + 11 * Math.sin(pr)).toFixed(2));
    this.valueEl.textContent = this.def_.fmt(this.value);
  }
}

/* ================= combo / stepper / buttons / about ================= */
function buildCombo() {
  const combo = document.getElementById("algo-combo");
  const list = document.getElementById("algo-list");
  const nameEl = document.getElementById("algo-name");
  const badgeEl = document.getElementById("algo-badge");
  combo.dataset.tooltip = HELP_TEXT.algorithm;
  ALGOS.forEach(a => {
    const item = document.createElement("div");
    item.className = "combo-item" + (a.id === params.algorithm ? " selected" : "");
    item.dataset.index = String(ALGOS.indexOf(a));
    item.innerHTML = `<span>${a.name}</span><span class="badge">${a.complexity}</span>`;
    item.onclick = () => {
      setAlgorithmValue(ALGOS.indexOf(a), true);
      combo.classList.remove("open");
    };
    list.appendChild(item);
  });
  combo.onclick = e => { combo.classList.toggle("open"); e.stopPropagation(); };
  document.addEventListener("click", () => combo.classList.remove("open"));
}

function setAlgorithmValue(index, notify) {
  const safeIndex = Math.max(0, Math.min(ALGOS.length - 1, Number(index) || 0));
  const algo = ALGOS[safeIndex];
  params.algorithm = algo.id;
  document.getElementById("algo-name").textContent = algo.name;
  document.getElementById("algo-badge").textContent = algo.complexity;
  document.querySelectorAll("#algo-list .combo-item").forEach(item => {
    item.classList.toggle("selected", Number(item.dataset.index) === safeIndex);
  });
  if (notify) {
    nativeParameter("algorithm", safeIndex, "begin");
    nativeParameter("algorithm", safeIndex, "change");
    nativeParameter("algorithm", safeIndex, "end");
  }
}

// Same 8 options as the original editor's slice ComboBox
const SLICE_OPTIONS = [4, 8, 16, 32, 64, 128, 256, 512];

function buildSliceCombo() {
  const combo = document.getElementById("slice-combo");
  const list = document.getElementById("slice-list");
  const valueEl = document.getElementById("slice-value");
  combo.dataset.tooltip = HELP_TEXT.slices;
  SLICE_OPTIONS.forEach(n => {
    const item = document.createElement("div");
    item.className = "combo-item" + (n === params.slices ? " selected" : "");
    item.dataset.value = String(n);
    item.innerHTML = `<span>${n}</span>`;
    item.onclick = () => {
      setSliceValue(n, true);
      combo.classList.remove("open");
    };
    list.appendChild(item);
  });
  combo.onclick = e => { combo.classList.toggle("open"); e.stopPropagation(); };
}

function setSliceValue(value, notify) {
  const safeValue = SLICE_OPTIONS.includes(Number(value)) ? Number(value) : 128;
  params.slices = safeValue;
  document.getElementById("slice-value").textContent = safeValue;
  document.querySelectorAll("#slice-list .combo-item").forEach(item => {
    item.classList.toggle("selected", Number(item.dataset.value) === safeValue);
  });
  updateStatus();
  if (notify) {
    nativeParameter("slices", safeValue, "begin");
    nativeParameter("slices", safeValue, "change");
    nativeParameter("slices", safeValue, "end");
  }
}

function buildButtons() {
  const turbo = document.getElementById("btn-turbo");
  const load = document.getElementById("btn-load");
  turbo.dataset.tooltip = HELP_TEXT.turbo;
  load.dataset.tooltip = HELP_TEXT.load;
  turbo.onclick = function () {
    params.turbo = !params.turbo;
    this.classList.toggle("on", params.turbo);
    nativeParameter("turbo", params.turbo ? 1 : 0, "begin");
    nativeParameter("turbo", params.turbo ? 1 : 0, "change");
    nativeParameter("turbo", params.turbo ? 1 : 0, "end");
  };
  load.onclick = () => {
    if (nativeBridgeConnected) nativeCall("loadFile");
    else document.getElementById("footer-status").textContent = "LOAD / NATIVE BRIDGE REQUIRED";
  };
  // About easter egg: click the brand, dismiss on click anywhere
  const about = document.getElementById("about");
  document.getElementById("brand").onclick = e => { e.stopPropagation(); about.classList.remove("hidden"); };
  about.onclick = () => about.classList.add("hidden");
}

function buildTooltips() {
  const tooltip = document.createElement("div");
  tooltip.id = "ui-tooltip";
  document.body.appendChild(tooltip);

  const show = target => {
    const text = target && target.dataset ? target.dataset.tooltip : "";
    if (!text) return;
    tooltip.textContent = text;
    tooltip.classList.add("show");

    const rect = target.getBoundingClientRect();
    const margin = 8;
    const width = tooltip.offsetWidth;
    const height = tooltip.offsetHeight;
    const left = Math.max(margin, Math.min(window.innerWidth - width - margin,
      rect.left + (rect.width - width) / 2));
    const below = rect.bottom + margin;
    const top = below + height <= window.innerHeight - margin
      ? below : Math.max(margin, rect.top - height - margin);
    tooltip.style.left = `${left}px`;
    tooltip.style.top = `${top}px`;
  };
  const hide = () => tooltip.classList.remove("show");

  document.querySelectorAll("[data-tooltip]").forEach(target => {
    target.setAttribute("aria-label", target.dataset.tooltip);
    target.addEventListener("pointerenter", () => show(target));
    target.addEventListener("pointermove", () => show(target));
    target.addEventListener("pointerleave", hide);
    target.addEventListener("focus", () => show(target));
    target.addEventListener("blur", hide);
  });

  window.addEventListener("resize", hide);
}

/* ================= static sample overview ================= */
class WaveView {
  constructor(canvas) {
    this.cv = canvas; this.cx = canvas.getContext("2d");
    this.points = null;
    this.slices = 0;
    this.position = null;
    window.addEventListener("resize", () => this.size());
    this.size();
  }
  size() {
    const r = this.cv.getBoundingClientRect();
    this.cv.width = r.width * 2; this.cv.height = r.height * 2;
    this.cx.setTransform(2, 0, 0, 2, 0, 0);
    this.w = r.width; this.h = r.height;
    this.render();
  }
  setSampleOverview(data) {
    const points = Array.isArray(data && data.points) && data.points.length > 1
      ? data.points : null;
    this.points = points;
    this.slices = Number(data && data.slices) || 0;
    this.position = null;
    const hint = document.getElementById("wave-hint");
    const hasSample = points !== null;
    hint.classList.toggle("hidden", hasSample);
    hint.setAttribute("aria-hidden", hasSample ? "true" : "false");
    this.render();
  }
  setPlaybackPosition(data) {
    if (!this.points || !data || typeof data.normalized !== "number") return;
    this.position = Math.max(0, Math.min(1, data.normalized));
    this.render();
  }
  render() {
    const { cx, w, h } = this;
    cx.clearRect(0, 0, w, h);
    cx.strokeStyle = "rgba(255,255,255,0.045)";
    cx.lineWidth = 1;
    for (let x = 0; x <= w; x += w / 12) { cx.beginPath(); cx.moveTo(x, 0); cx.lineTo(x, h); cx.stroke(); }
    cx.beginPath(); cx.moveTo(0, h / 2); cx.lineTo(w, h / 2); cx.stroke();

    if (!this.points) return;

    const mid = h / 2;
    const grad = cx.createLinearGradient(0, 0, 0, h);
    grad.addColorStop(0, "rgba(157,92,255,0.30)");
    grad.addColorStop(0.5, "rgba(157,92,255,0.10)");
    grad.addColorStop(1, "rgba(157,92,255,0.30)");
    cx.beginPath();
    this.points.forEach((point, index) => {
      const u = index / (this.points.length - 1);
      const amplitude = Array.isArray(point) ? point[1] : Math.abs(point);
      const y = mid - Math.max(-1, Math.min(1, amplitude)) * (h / 2 - 8);
      if (index === 0) cx.moveTo(u * w, y); else cx.lineTo(u * w, y);
    });
    for (let index = this.points.length - 1; index >= 0; --index) {
      const point = this.points[index];
      const u = index / (this.points.length - 1);
      const amplitude = Array.isArray(point) ? point[0] : -Math.abs(point);
      cx.lineTo(u * w, mid - Math.max(-1, Math.min(1, amplitude)) * (h / 2 - 8));
    }
    cx.closePath();
    cx.fillStyle = grad; cx.fill();
    cx.strokeStyle = "rgba(201,166,255,0.85)"; cx.lineWidth = 1.4;
    cx.stroke();

    if (this.slices > 1) {
      cx.strokeStyle = "rgba(255,255,255,0.16)"; cx.lineWidth = 1;
      for (let i = 1; i < this.slices; ++i) {
        const x = (i / this.slices) * w;
        cx.beginPath(); cx.moveTo(x, 4); cx.lineTo(x, h - 4); cx.stroke();
      }
    }

    if (this.position !== null) {
      const px = this.position * w;
      cx.strokeStyle = "rgba(255,255,255,0.9)"; cx.lineWidth = 1;
      cx.beginPath(); cx.moveTo(px, 2); cx.lineTo(px, h - 2); cx.stroke();
      cx.fillStyle = "#fff";
      cx.beginPath(); cx.moveTo(px - 4, 0); cx.lineTo(px + 4, 0); cx.lineTo(px, 6); cx.closePath(); cx.fill();
    }
  }
}

/* ================= sort visualizer ================= */
const VIZ_BARS = 56;
class VoiceRow {
  constructor(rowEl, active) {
    this.rowEl = rowEl;
    this.active = active;
    this.cv = rowEl.querySelector("canvas");
    this.cx = this.cv.getContext("2d");
    this.arr = new Array(VIZ_BARS).fill(0);
    this.hasNativeState = false;
    this.size();
  }
  size() {
    const r = this.cv.getBoundingClientRect();
    this.cv.width = r.width * 2; this.cv.height = r.height * 2;
    this.cx.setTransform(2, 0, 0, 2, 0, 0);
    this.w = r.width; this.h = r.height;
    this.draw();
  }
  setState(state) {
    if (!state || !Array.isArray(state.currentValues)) return;
    this.hasNativeState = true;
    this.active = Boolean(state.active) && !Boolean(state.paused);
    const sourceScale = Math.max(1, Number(state.sliceCount) - 1 || state.currentValues.length - 1);
    this.arr = state.currentValues.slice(0, VIZ_BARS)
      .map(value => (Number(value) / sourceScale) * (VIZ_BARS - 1));
    while (this.arr.length < VIZ_BARS) this.arr.push(0);
    this.progressValue = typeof state.progress === "number"
      ? Math.max(0, Math.min(1, state.progress))
      : (state.totalSteps > 0 ? state.stepIndex / state.totalSteps : 0);
    this.done = Boolean(state.completed);
    this.draw();
  }
  progress() { return this.done ? 1 : (this.progressValue || 0); }
  draw() {
    const { cx, w, h } = this;
    cx.clearRect(0, 0, w, h);
    const bw = w / VIZ_BARS;
    for (let i = 0; i < VIZ_BARS; i++) {
      const v = this.hasNativeState ? this.arr[i] / Math.max(1, VIZ_BARS - 1) : 0;
      const bh = 2 + v * (h - 4);
      const sorted = this.done;
      let col;
      if (this.active) {
        col = sorted ? "rgba(157,92,255,0.75)" : "rgba(157,92,255,0.45)";
      } else {
        col = "rgba(255,255,255,0.035)";
      }
      cx.fillStyle = col;
      cx.fillRect(i * bw + 0.5, h - bh, Math.max(1, bw - 1.5), bh);
    }
  }
}

let rows = [];
function buildViz() {
  const container = document.getElementById("viz-rows");
  for (let i = 0; i < 4; i++) {
    const row = document.createElement("div");
    row.className = "viz-row";
    row.innerHTML = `<div class="viz-tag"><span class="viz-led"></span>V${i + 1}</div><canvas></canvas>`;
    container.appendChild(row);
    rows.push(new VoiceRow(row, false));
    row.classList.remove("active");
  }
}

/* ================= status bar ================= */
function updateStatus() {
  const active = rows.filter(r => r.active).length;
  const parts = rows.map((r, i) =>
    r.active ? `V${i + 1} ${Math.round(r.progress() * 100)}%` : null).filter(Boolean);
  document.getElementById("footer-status").textContent =
    `${active}/4 voices :: ${parts.join(" | ") || "\u2014"} :: ${params.slices} grains`;
}

/* ================= boot ================= */
const knobs = {};
document.querySelectorAll(".knob").forEach(el => {
  const knob = new Knob(el);
  knobs[knob.param] = knob;
});
buildCombo();
buildSliceCombo();
buildButtons();
document.getElementById("panel-wave").dataset.tooltip = HELP_TEXT.waveform;
document.getElementById("panel-viz").dataset.tooltip = HELP_TEXT.visualizer;
buildViz();
updateStatus();
buildTooltips();

const wave = new WaveView(document.getElementById("wave-canvas"));

const DROP_MAX_BYTES = 100 * 1024 * 1024;
const DROP_EXTENSIONS = new Set(["wav", "aif", "aiff", "flac", "ogg", "mp3"]);

function setDropActive(active) {
  document.getElementById("panel-wave").classList.toggle("drop-active", Boolean(active));
}

function isSupportedDrop(file) {
  const name = String(file && file.name || "");
  const dot = name.lastIndexOf(".");
  return dot > 0 && DROP_EXTENSIONS.has(name.slice(dot + 1).toLowerCase());
}

function fileToBase64(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(new Error("file read failed"));
    reader.onload = () => {
      const dataUrl = String(reader.result || "");
      const comma = dataUrl.indexOf(",");
      resolve(comma >= 0 ? dataUrl.slice(comma + 1) : dataUrl);
    };
    reader.readAsDataURL(file);
  });
}

async function importDroppedFile(file) {
  if (!nativeBridgeConnected) {
    document.getElementById("footer-status").textContent = "DROP / NATIVE BRIDGE REQUIRED";
    return;
  }
  if (!isSupportedDrop(file)) {
    document.getElementById("footer-status").textContent = "DROP FAILED: AUDIO FILE REQUIRED";
    return;
  }
  if (file.size <= 0 || file.size > DROP_MAX_BYTES) {
    document.getElementById("footer-status").textContent = "DROP FAILED: FILE OVER 100 MB";
    return;
  }

  document.getElementById("footer-status").textContent = "READING SAMPLE...";
  try {
    const encoded = await fileToBase64(file);
    const loaded = await nativeCall("loadFileData", file.name, encoded);
    if (!loaded)
      document.getElementById("footer-status").textContent = "DROP FAILED: SAMPLE NOT LOADED";
  } catch (error) {
    document.getElementById("footer-status").textContent = "DROP FAILED: FILE READ ERROR";
  }
}

function bindFileDrop() {
  const target = document.getElementById("panel-wave");
  let dragDepth = 0;

  const clearDrag = () => {
    dragDepth = 0;
    setDropActive(false);
  };

  target.addEventListener("dragenter", event => {
    event.preventDefault();
    event.stopPropagation();
    dragDepth += 1;
    setDropActive(true);
  });
  target.addEventListener("dragover", event => {
    event.preventDefault();
    event.stopPropagation();
    if (event.dataTransfer) event.dataTransfer.dropEffect = "copy";
    setDropActive(true);
  });
  target.addEventListener("dragleave", event => {
    event.preventDefault();
    event.stopPropagation();
    dragDepth = Math.max(0, dragDepth - 1);
    if (dragDepth === 0) setDropActive(false);
  });
  target.addEventListener("drop", event => {
    event.preventDefault();
    event.stopPropagation();
    clearDrag();
    const files = event.dataTransfer && event.dataTransfer.files;
    if (files && files.length > 0) importDroppedFile(files[0]);
  });

  // Never let an audio file dropped just outside the panel navigate the page.
  window.addEventListener("dragover", event => event.preventDefault());
  window.addEventListener("drop", event => event.preventDefault());
  window.addEventListener("dragend", clearDrag);
  window.addEventListener("blur", clearDrag);
}

// Native bridge contract: the sample map is static data, and the white marker
// is only drawn after a real normalized playback position is provided.
window.SieveUI = {
  setSampleOverview: data => wave.setSampleOverview(data),
  setPlaybackPosition: data => wave.setPlaybackPosition(data),
  setVoiceStates: states => {
    if (!Array.isArray(states)) return;
    states.slice(0, rows.length).forEach((state, index) => {
      rows[index].setState(state);
      rows[index].rowEl.classList.toggle("active", rows[index].active);
    });
    updateStatus();
  },
  setParameterState: state => applyParameterState(state),
  setUiFrame: frame => applyUiFrame(frame),
};

function applyParameterState(state) {
  if (!state) return;

  if (Number.isFinite(Number(state.algorithm)))
    setAlgorithmValue(Number(state.algorithm), false);
  if (Number.isFinite(Number(state.slices)))
    setSliceValue(Number(state.slices), false);

  Object.keys(knobs).forEach(param => {
    if (typeof state[param] === "number") knobs[param].setFromNative(state[param]);
  });

  if (typeof state.turbo === "boolean") {
    params.turbo = state.turbo;
    document.getElementById("btn-turbo").classList.toggle("on", state.turbo);
  }
  updateStatus();
}

function applyUiFrame(frame) {
  if (!frame) return;
  if (Array.isArray(frame.voices)) window.SieveUI.setVoiceStates(frame.voices);
  if (typeof frame.playbackPosition === "number")
    window.SieveUI.setPlaybackPosition({ normalized: frame.playbackPosition });
}

bindFileDrop();
bindNativeBridge();
if (nativeBridgeConnected) nativeCall("uiReady");
