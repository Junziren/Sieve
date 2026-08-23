/* Sieve WebView UI — visual shell only, no DSP connection.
   Control set mirrors the plugin 1:1:
   algorithm / slices / speed / duration / attack / decay / sustain /
   release / gain / pan / turbo + load / shuffle actions + waveform +
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
  speed:    { label: "SPEED",    min: 1,     max: 400,  def: 30,   fmt: v => v.toFixed(0) + " ms" },
  duration: { label: "DURATION", min: 0.05,  max: 5.0,  def: 1.0,  fmt: v => v.toFixed(2) + " \u00D7" },
  attack:   { label: "ATTACK",   min: 1,     max: 500,  def: 5,    fmt: v => v.toFixed(0) + " ms" },
  decay:    { label: "DECAY",    min: 5,     max: 1000, def: 100,  fmt: v => v.toFixed(0) + " ms" },
  sustain:  { label: "SUSTAIN",  min: 0,     max: 1,    def: 0.7,  fmt: v => Math.round(v * 100) + " %" },
  release:  { label: "RELEASE",  min: 5,     max: 2000, def: 80,   fmt: v => v.toFixed(0) + " ms" },
  gain:     { label: "GAIN",     min: 0,     max: 1.5,  def: 0.8,  fmt: v => v.toFixed(2) },
  pan:      { label: "PAN",      min: -1,    max: 1,    def: 0,    fmt: v => (v === 0 ? "C" : (v < 0 ? "L" : "R") + Math.round(Math.abs(v) * 100)) },
};

/* ================= knob ================= */
class Knob {
  constructor(root) {
    this.param = root.dataset.param;
    this.def_ = KNOB_DEFS[this.param];
    this.value = this.def_.def;
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
    root.addEventListener("pointerdown", e => {
      dragging = true; lastY = e.clientY;
      root.classList.add("dragging");
      root.setPointerCapture(e.pointerId);
    });
    root.addEventListener("pointermove", e => {
      if (!dragging) return;
      const fine = e.shiftKey ? 0.2 : 1;
      const dy = lastY - e.clientY; lastY = e.clientY;
      const range = this.def_.max - this.def_.min;
      this.set(this.value + (dy / 180) * range * fine);
    });
    root.addEventListener("pointerup", e => {
      dragging = false;
      root.classList.remove("dragging");
      root.releasePointerCapture(e.pointerId);
    });
    root.addEventListener("dblclick", () => this.set(this.def_.def));
    root.addEventListener("wheel", e => {
      e.preventDefault();
      const range = this.def_.max - this.def_.min;
      this.set(this.value + Math.sign(-e.deltaY) * range * 0.03);
    }, { passive: false });
  }
  set(v) {
    this.value = Math.min(this.def_.max, Math.max(this.def_.min, v));
    params[this.param] = this.value;
    this.render();
    if (this.param === "speed") restartDemo();
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
  ALGOS.forEach(a => {
    const item = document.createElement("div");
    item.className = "combo-item" + (a.id === params.algorithm ? " selected" : "");
    item.innerHTML = `<span>${a.name}</span><span class="badge">${a.complexity}</span>`;
    item.onclick = () => {
      params.algorithm = a.id;
      nameEl.textContent = a.name;
      badgeEl.textContent = a.complexity;
      list.querySelectorAll(".combo-item").forEach(el => el.classList.remove("selected"));
      item.classList.add("selected");
      combo.classList.remove("open");
      restartDemo();
    };
    list.appendChild(item);
  });
  combo.onclick = e => { combo.classList.toggle("open"); e.stopPropagation(); };
  document.addEventListener("click", () => combo.classList.remove("open"));
}

// Same 8 options as the original editor's slice ComboBox
const SLICE_OPTIONS = [4, 8, 16, 32, 64, 128, 256, 512];

function buildSliceCombo() {
  const combo = document.getElementById("slice-combo");
  const list = document.getElementById("slice-list");
  const valueEl = document.getElementById("slice-value");
  SLICE_OPTIONS.forEach(n => {
    const item = document.createElement("div");
    item.className = "combo-item" + (n === params.slices ? " selected" : "");
    item.innerHTML = `<span>${n}</span>`;
    item.onclick = () => {
      params.slices = n;
      valueEl.textContent = n;
      list.querySelectorAll(".combo-item").forEach(el => el.classList.remove("selected"));
      item.classList.add("selected");
      combo.classList.remove("open");
      updateStatus();
    };
    list.appendChild(item);
  });
  combo.onclick = e => { combo.classList.toggle("open"); e.stopPropagation(); };
}

function buildButtons() {
  document.getElementById("btn-turbo").onclick = function () {
    params.turbo = !params.turbo;
    this.classList.toggle("on", params.turbo);
  };
  document.getElementById("btn-shuffle").onclick = () => restartDemo(true);
  document.getElementById("btn-load").onclick = () => {
    document.getElementById("wave-hint").classList.add("hidden");
  };
  // About easter egg: click the brand, dismiss on click anywhere
  const about = document.getElementById("about");
  document.getElementById("brand").onclick = e => { e.stopPropagation(); about.classList.remove("hidden"); };
  about.onclick = () => about.classList.add("hidden");
}

/* ================= waveform placeholder ================= */
class WaveView {
  constructor(canvas) {
    this.cv = canvas; this.cx = canvas.getContext("2d");
    this.t = 0;
    window.addEventListener("resize", () => this.size());
    this.size();
  }
  size() {
    const r = this.cv.getBoundingClientRect();
    this.cv.width = r.width * 2; this.cv.height = r.height * 2;
    this.cx.setTransform(2, 0, 0, 2, 0, 0);
    this.w = r.width; this.h = r.height;
  }
  draw(dt) {
    this.t += dt;
    const { cx, w, h } = this;
    cx.clearRect(0, 0, w, h);
    cx.strokeStyle = "rgba(255,255,255,0.045)";
    cx.lineWidth = 1;
    for (let x = 0; x <= w; x += w / 12) { cx.beginPath(); cx.moveTo(x, 0); cx.lineTo(x, h); cx.stroke(); }
    cx.beginPath(); cx.moveTo(0, h / 2); cx.lineTo(w, h / 2); cx.stroke();

    const mid = h / 2;
    const grad = cx.createLinearGradient(0, 0, 0, h);
    grad.addColorStop(0, "rgba(157,92,255,0.30)");
    grad.addColorStop(0.5, "rgba(157,92,255,0.10)");
    grad.addColorStop(1, "rgba(157,92,255,0.30)");
    cx.beginPath();
    cx.moveTo(0, mid);
    for (let x = 0; x <= w; x += 2) {
      const u = x / w;
      const env = Math.pow(Math.sin(u * Math.PI), 0.6);
      const s = Math.sin(u * 42 + this.t * 1.4) * 0.55
              + Math.sin(u * 90 - this.t * 2.2) * 0.3
              + Math.sin(u * 9 + this.t * 0.5) * 0.5;
      cx.lineTo(x, mid - s * env * (h / 2 - 8));
    }
    cx.lineTo(w, mid);
    cx.closePath();
    cx.fillStyle = grad; cx.fill();
    cx.strokeStyle = "rgba(201,166,255,0.85)"; cx.lineWidth = 1.4;
    cx.stroke();

    const px = ((this.t * 0.06) % 1) * w;
    cx.strokeStyle = "rgba(255,255,255,0.9)"; cx.lineWidth = 1;
    cx.beginPath(); cx.moveTo(px, 2); cx.lineTo(px, h - 2); cx.stroke();
    cx.fillStyle = "#fff";
    cx.beginPath(); cx.moveTo(px - 4, 0); cx.lineTo(px + 4, 0); cx.lineTo(px, 6); cx.closePath(); cx.fill();
  }
}

/* ================= sort visualizer (demo animation) ================= */
const VIZ_BARS = 56;
class VoiceRow {
  constructor(rowEl, active) {
    this.rowEl = rowEl;
    this.active = active;
    this.cv = rowEl.querySelector("canvas");
    this.cx = this.cv.getContext("2d");
    this.flash = new Array(VIZ_BARS).fill(0);
    this.arr = Array.from({ length: VIZ_BARS }, (_, i) => i);
    this.shuffle();
    this.stepAcc = 0;
    this.size();
  }
  size() {
    const r = this.cv.getBoundingClientRect();
    this.cv.width = r.width * 2; this.cv.height = r.height * 2;
    this.cx.setTransform(2, 0, 0, 2, 0, 0);
    this.w = r.width; this.h = r.height;
  }
  shuffle() {
    for (let i = this.arr.length - 1; i > 0; i--) {
      const j = Math.floor(Math.random() * (i + 1));
      [this.arr[i], this.arr[j]] = [this.arr[j], this.arr[i]];
    }
    this.i = 0; this.pass = 0; this.done = false; this.stepAcc = 0;
  }
  progress() { return this.done ? 1 : this.pass / (VIZ_BARS - 1); }
  step() {
    const a = this.arr, n = a.length;
    if (this.done) return;
    if (this.i >= n - 1 - this.pass) {
      this.i = 0; this.pass++;
      if (this.pass >= n - 1) { this.done = true; return; }
    }
    if (a[this.i] > a[this.i + 1]) {
      [a[this.i], a[this.i + 1]] = [a[this.i + 1], a[this.i]];
      this.flash[this.i] = 1; this.flash[this.i + 1] = 1;
    }
    this.i++;
  }
  draw(dt) {
    this.stepAcc += dt;
    if (this.active && !params.turbo) {
      while (this.stepAcc > params.speed) { this.step(); this.stepAcc -= params.speed; }
    }
    const { cx, w, h } = this;
    cx.clearRect(0, 0, w, h);
    const bw = w / VIZ_BARS;
    for (let i = 0; i < VIZ_BARS; i++) {
      const v = this.arr[i] / (VIZ_BARS - 1);
      const bh = 2 + v * (h - 4);
      this.flash[i] = Math.max(0, this.flash[i] - dt * 4);
      const f = this.flash[i];
      const sorted = this.done;
      let col;
      if (this.active) {
        col = f > 0.02
          ? `rgba(0,212,255,${0.55 + 0.45 * f})`
          : sorted ? "rgba(157,92,255,0.75)" : "rgba(157,92,255,0.45)";
      } else {
        col = "rgba(255,255,255,0.07)";
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
    rows.push(new VoiceRow(row, i === 0));
    row.classList.toggle("active", i === 0);
  }
}

function restartDemo(reshuffle) {
  if (reshuffle) rows.forEach(r => r.shuffle());
  updateStatus();
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
document.querySelectorAll(".knob").forEach(el => new Knob(el));
buildCombo();
buildSliceCombo();
buildButtons();
buildViz();
updateStatus();

const wave = new WaveView(document.getElementById("wave-canvas"));
let last = performance.now(), statusAcc = 0;
(function loop(now) {
  const dt = Math.min(0.05, (now - last) / 1000); last = now;
  wave.draw(dt);
  rows.forEach(r => r.draw(dt));
  statusAcc += dt;
  if (statusAcc > 0.25) { statusAcc = 0; updateStatus(); }
  requestAnimationFrame(loop);
})(last);
