"use strict";

// APVTS remains authoritative. These offline defaults mirror the existing ABI;
// the native handshake supplies ranges/defaults before an installed editor is used.
const $ = (id) => document.getElementById(id);
const params = {
  algorithm: 0,
  slices: 128,
  speed: 30,
  duration: 1,
  attack: 5,
  decay: 100,
  sustain: 0.7,
  release: 80,
  gain: 0.8,
  pan: 0,
  turbo: false,
};
const ALGOS = [
  ["Bubble", "相邻比较，细密而逐步展开的节奏。"],
  ["Insertion", "逐粒插入，让片段逐步归位。"],
  ["Selection", "逐次寻找最小值，交换更稀疏。"],
  ["Quick", "分区排序，快速跨越不同片段。"],
  ["Merge", "由小到大合并，形成分层的推进。"],
  ["Shell", "从大跨度到小跨度，逐渐收拢。"],
  ["Heap", "树形重排，产生跳跃的粒子次序。"],
  ["Shaker", "双向扫描，往返推进。"],
  ["Bogo", "随机重排；受步数限制，可能无法排完。"],
];
const DEFS = {
  speed: {
    label: "步进间隔",
    id: "sortSpeed",
    min: 1,
    max: 400,
    step: 1,
    skew: 0.4,
    def: 30,
    fmt: (v) => `${v.toFixed(0)} ms`,
    help: "相邻排序步的间隔。数值越小，推进越快；极速模式下节奏不同。",
  },
  duration: {
    label: "粒子长度",
    id: "grainDuration",
    min: 0.05,
    max: 5,
    step: 0.01,
    skew: 0.45,
    def: 1,
    fmt: (v) => `${v.toFixed(2)} ×`,
    help: "按原始切片时长缩放每粒声音的长度。",
  },
  attack: {
    label: "起音 A",
    id: "attack",
    min: 1,
    max: 500,
    step: 1,
    skew: 0.4,
    def: 5,
    fmt: (v) => `${v.toFixed(0)} ms`,
    help: "每粒声音从静音升到峰值所需的时间。",
  },
  decay: {
    label: "衰减 D",
    id: "decay",
    min: 5,
    max: 1000,
    step: 1,
    skew: 0.4,
    def: 100,
    fmt: (v) => `${v.toFixed(0)} ms`,
    help: "每粒声音从峰值降到持续电平所需的时间。",
  },
  sustain: {
    label: "持续 S",
    id: "sustain",
    min: 0,
    max: 1,
    step: 0.01,
    skew: 1,
    def: 0.7,
    fmt: (v) => `${Math.round(v * 100)} %`,
    help: "持续阶段的电平。输入数值使用百分比，例如 70。",
    input: (v) => v * 100,
    parse: (v) => v / 100,
  },
  release: {
    label: "释音 R",
    id: "release",
    min: 5,
    max: 2000,
    step: 1,
    skew: 0.35,
    def: 80,
    fmt: (v) => `${v.toFixed(0)} ms`,
    help: "粒子释放阶段的淡出时间。",
  },
  gain: {
    label: "输出增益",
    id: "gain",
    min: 0,
    max: 1.5,
    step: 0.01,
    skew: 0.5,
    def: 0.8,
    fmt: (v) => `${v.toFixed(2)} ×`,
    help: "全部粒子的输出增益倍率。",
  },
  pan: {
    label: "声像",
    id: "pan",
    min: -1,
    max: 1,
    step: 0.01,
    skew: 1,
    def: 0,
    fmt: (v) =>
      Math.abs(v) < 0.005
        ? "中央"
        : `${v < 0 ? "L" : "R"} ${Math.round(Math.abs(v) * 100)}`,
    help: "左右声像。输入 -100 到 100；0 为中央。",
    input: (v) => v * 100,
    parse: (v) => v / 100,
  },
};
let connected = false,
  ready = false,
  loading = false,
  aboutOpen = false;
let nextPromise = 0,
  sampleLoaded = false,
  actionMessage = null;
const pending = new Map(),
  knobs = {},
  voices = [];
const idleHelp = "拖动旋钮调节 · Shift 精调 · 双击复位 · 点击数值输入";
function callNative(name, ...args) {
  const backend = window.__JUCE__?.backend;
  if (!backend) return Promise.resolve(false);
  const id = nextPromise++;
  return new Promise((resolve) => {
    const timer = setTimeout(() => {
      pending.delete(id);
      resolve(false);
    }, 30000);
    pending.set(id, { resolve, timer });
    backend.emitEvent("__juce__invoke", { name, params: args, resultId: id });
  });
}
function paramId(key) {
  return key === "slices"
    ? "sliceCount"
    : key === "turbo"
      ? "performanceMode"
      : DEFS[key]?.id || key;
}
function send(key, value, phase) {
  if (ready) callNative("setParameter", paramId(key), value, phase);
}
function commit(key, value) {
  send(key, value, "begin");
  send(key, value, "change");
  send(key, value, "end");
}
function notify(text, kind = "info") {
  actionMessage = text ? { text, kind } : null;
  renderStatus();
}
function renderStatus() {
  $("footer-status").textContent =
    actionMessage?.text ||
    (sampleLoaded ? "采样已就绪 · 使用 MIDI 键盘演奏" : "请载入音频采样");
  $("footer").classList.toggle("error", actionMessage?.kind === "error");
  $("dismiss-status").hidden =
    !actionMessage || actionMessage.kind === "loading";
}
function busy(value) {
  loading = value;
  $("btn-load").disabled = value;
  $("wave-hint").disabled = value;
  $("btn-load").textContent = value ? "正在载入…" : "载入音频 ↗";
}
function receiveStatus(text) {
  text = String(text || "");
  const isLoading = /loading|reading/i.test(text),
    failed = /fail|invalid/i.test(text);
  busy(isLoading);
  const messages = {
    "Loading sample...": "正在载入采样…",
    "Sample loaded": "采样已载入 · 使用 MIDI 音符触发",
    "Load cancelled": "已取消载入",
    "Load failed: file not found": "载入失败：找不到文件",
    "Load failed: unsupported format": "载入失败：不支持此音频格式",
    "Load failed: file is larger than 100 MB": "载入失败：文件超过 100 MB",
    "Load failed: empty file": "载入失败：文件没有音频数据",
    "Load failed: sample-rate mismatch": "载入失败：采样率不匹配",
  };
  notify(
    messages[text] || text,
    failed ? "error" : isLoading ? "loading" : "info",
  );
}
function clamp(v, min, max) {
  return Math.max(min, Math.min(max, v));
}
function editable(el) {
  return el?.matches("input,select,textarea");
}
class Knob {
  constructor(root) {
    this.root = root;
    this.key = root.dataset.param;
    this.d = DEFS[this.key];
    this.value = params[this.key];
    this.active = false;
    this.pointerId = null;
    this.wheelTimer = 0;
    root.tabIndex = 0;
    root.setAttribute("role", "slider");
    root.setAttribute("aria-label", this.d.label);
    root.dataset.help = this.d.help;
    root.innerHTML = `<span class="k-label">${this.d.label}</span><svg viewBox="0 0 52 52" aria-hidden="true"><path class="k-track" fill="none" stroke-width="3"/><path class="k-arc" fill="none" stroke-width="3"/><circle class="k-face" cx="26" cy="26" r="15.5" stroke-width="1"/><line class="k-pointer" x1="26" y1="26" x2="26" y2="14" stroke-width="2" stroke-linecap="round"/></svg><input class="k-value" type="text" inputmode="decimal" aria-label="${this.d.label}数值" spellcheck="false">`;
    this.input = root.querySelector("input");
    root.addEventListener("pointerdown", (e) => {
      if (e.button !== 0 || e.target === this.input || this.pointerId !== null)
        return;
      this.finish();
      root.focus();
      this.pointerId = e.pointerId;
      this.lastY = e.clientY;
      this.normal = this.toNormal(this.value);
      root.setPointerCapture(e.pointerId);
    });
    root.addEventListener("pointermove", (e) => {
      if (this.pointerId !== e.pointerId) return;
      const dy = this.lastY - e.clientY;
      this.lastY = e.clientY;
      if (!dy) return;
      this.normal = clamp(this.normal + dy / (e.shiftKey ? 1600 : 320), 0, 1);
      this.begin();
      this.set(this.fromNormal(this.normal), true);
    });
    for (const event of ["pointerup", "pointercancel", "lostpointercapture"])
      root.addEventListener(event, () => this.finish());
    root.addEventListener("dblclick", (e) => {
      if (e.target !== this.input) {
        this.finish();
        this.begin();
        this.set(this.d.def, true);
        this.finish();
      }
    });
    root.addEventListener(
      "wheel",
      (e) => {
        if (e.target === this.input) return;
        e.preventDefault();
        if (this.pointerId !== null) return;
        this.begin();
        this.nudge(
          clamp(
            this.toNormal(this.value) +
              Math.sign(-e.deltaY) * (e.shiftKey ? 0.001 : 0.01),
            0,
            1,
          ),
        );
        clearTimeout(this.wheelTimer);
        this.wheelTimer = setTimeout(() => this.finish(), 160);
      },
      { passive: false },
    );
    root.addEventListener("keydown", (e) => {
      if (e.target === this.input) return;
      const direction = {
        ArrowUp: 1,
        ArrowRight: 1,
        ArrowDown: -1,
        ArrowLeft: -1,
        PageUp: 10,
        PageDown: -10,
      }[e.key];
      if (direction === undefined && e.key !== "Home" && e.key !== "End")
        return;
      e.preventDefault();
      this.begin();
      const n =
        e.key === "Home"
          ? 0
          : e.key === "End"
            ? 1
            : clamp(
                this.toNormal(this.value) +
                  direction * (e.shiftKey ? 0.001 : 0.01),
                0,
                1,
              );
      this.nudge(n);
    });
    root.addEventListener("keyup", (e) => {
      if (e.target !== this.input) this.finish();
    });
    root.addEventListener("blur", () => this.finish());
    this.input.addEventListener("focus", () => {
      this.finish();
      this.input.value = String(
        Number(
          (this.d.input ? this.d.input(this.value) : this.value).toFixed(3),
        ),
      );
      this.input.select();
    });
    this.input.addEventListener("keydown", (e) => {
      e.stopPropagation();
      if (e.key === "Enter") {
        e.preventDefault();
        this.input.blur();
      }
      if (e.key === "Escape") {
        e.preventDefault();
        this.cancelInput = true;
        this.input.blur();
        root.focus();
      }
    });
    this.input.addEventListener("blur", () => {
      if (!this.cancelInput) {
        const raw = Number(this.input.value.trim());
        if (this.input.value.trim() && Number.isFinite(raw)) {
          this.begin();
          this.set(this.d.parse ? this.d.parse(raw) : raw, true);
          this.finish();
        } else notify("请输入有效数值", "error");
      }
      this.cancelInput = false;
      this.render();
    });
    this.render();
  }
  toNormal(v) {
    return Math.pow(
      clamp((v - this.d.min) / (this.d.max - this.d.min), 0, 1),
      this.d.skew,
    );
  }
  fromNormal(v) {
    return (
      this.d.min + (this.d.max - this.d.min) * Math.pow(v, 1 / this.d.skew)
    );
  }
  nudge(normalized) {
    const target = this.fromNormal(normalized);
    const delta = target - this.value;
    // Discrete parameters must remain operable near the low end of a skewed range.
    this.set(
      Math.abs(delta) < this.d.step && delta !== 0
        ? this.value + Math.sign(delta) * this.d.step
        : target,
      true,
    );
  }
  begin() {
    if (this.active) return;
    this.active = true;
    this.root.classList.add("dragging");
    send(this.key, this.value, "begin");
  }
  finish() {
    clearTimeout(this.wheelTimer);
    const pointer = this.pointerId;
    this.pointerId = null;
    if (this.active) {
      send(this.key, this.value, "end");
      this.active = false;
      this.root.classList.remove("dragging");
    }
    if (pointer !== null && this.root.hasPointerCapture(pointer))
      this.root.releasePointerCapture(pointer);
  }
  set(v, write = false) {
    if (!Number.isFinite(v)) return;
    this.value = Number(
      clamp(
        this.d.min + Math.round((v - this.d.min) / this.d.step) * this.d.step,
        this.d.min,
        this.d.max,
      ).toFixed(6),
    );
    params[this.key] = this.value;
    this.render();
    renderEnvelope();
    if (write) send(this.key, this.value, "change");
  }
  fromNative(v) {
    if (
      !this.active &&
      this.pointerId === null &&
      document.activeElement !== this.input
    )
      this.set(v);
  }
  render() {
    const angle = -135 + this.toNormal(this.value) * 270;
    const point = (a) => {
      const r = ((a - 90) * Math.PI) / 180;
      return [26 + 21 * Math.cos(r), 26 + 21 * Math.sin(r)];
    };
    const arc = (end) => {
      const [x, y] = point(-135),
        [xx, yy] = point(end);
      return `M${x} ${y}A21 21 0 ${end + 135 > 180 ? 1 : 0} 1 ${xx} ${yy}`;
    };
    this.root.querySelector(".k-track").setAttribute("d", arc(135));
    this.root
      .querySelector(".k-arc")
      .setAttribute("d", arc(Math.max(-134.99, angle)));
    this.root
      .querySelector(".k-pointer")
      .setAttribute("transform", `rotate(${angle} 26 26)`);
    if (document.activeElement !== this.input)
      this.input.value = this.d.fmt(this.value);
    this.root.setAttribute("aria-valuemin", this.d.min);
    this.root.setAttribute("aria-valuemax", this.d.max);
    this.root.setAttribute("aria-valuenow", this.value);
    this.root.setAttribute("aria-valuetext", this.d.fmt(this.value));
  }
}
function finishAll() {
  Object.values(knobs).forEach((k) => k.finish());
}
function renderEnvelope() {
  const stages = [
    Math.sqrt(params.attack),
    Math.sqrt(params.decay),
    15,
    Math.sqrt(params.release),
  ];
  const total = stages.reduce((a, b) => a + b, 0);
  const a = (stages[0] / total) * 380 + 10,
    d = a + (stages[1] / total) * 380,
    s = d + (stages[2] / total) * 380,
    y = 38 - params.sustain * 32;
  $("envelope-path").setAttribute(
    "d",
    `M2 38 L${a} 6 L${d} ${y} L${s} ${y} L398 38`,
  );
}
function setAlgorithm(v, write = false) {
  params.algorithm = clamp(Math.round(Number(v) || 0), 0, ALGOS.length - 1);
  $("algorithm").value = params.algorithm;
  $("algorithm-description").textContent = ALGOS[params.algorithm][1];
  if (write) commit("algorithm", params.algorithm);
}
function setSlices(v, write = false) {
  if (!Number.isFinite(Number(v))) return;
  params.slices = clamp(Math.round(Number(v)), 4, 512);
  if (document.activeElement !== $("slices") || write)
    $("slices").value = params.slices;
  document
    .querySelectorAll("[data-slices]")
    .forEach((b) =>
      b.classList.toggle(
        "selected",
        Number(b.dataset.slices) === params.slices,
      ),
    );
  $("slice-info").textContent = `${params.slices} 个粒子`;
  wave.slices = params.slices;
  wave.render();
  if (write) commit("slices", params.slices);
}
function setTurbo(on, write = false) {
  params.turbo = Boolean(on);
  $("btn-turbo").classList.toggle("on", params.turbo);
  $("btn-turbo").setAttribute("aria-pressed", String(params.turbo));
  $("turbo-state").textContent = params.turbo ? "开启" : "关闭";
  $("btn-turbo").querySelector(".switch").textContent = params.turbo
    ? "I"
    : "0";
  $("sort-note").textContent = params.turbo
    ? "极速推进中 · 节奏会改变，CPU 占用可能升高。"
    : "步进间隔越小，排序越快。";
  $("sort-note").classList.toggle("warning", params.turbo);
  if (write) commit("turbo", params.turbo ? 1 : 0);
}
function applyParameters(state) {
  if (!state) return;
  if (Number.isFinite(state.algorithm)) setAlgorithm(state.algorithm);
  if (Number.isFinite(state.slices)) setSlices(state.slices);
  Object.keys(knobs).forEach((key) => {
    if (Number.isFinite(state[key])) knobs[key].fromNative(state[key]);
  });
  if (typeof state.turbo === "boolean") setTurbo(state.turbo);
}
function sizeCanvas(canvas) {
  const r = canvas.getBoundingClientRect(),
    dpr = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = Math.round(r.width * dpr);
  canvas.height = Math.round(r.height * dpr);
  const ctx = canvas.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return { ctx, w: r.width, h: r.height };
}
class WaveView {
  constructor() {
    this.canvas = $("wave-canvas");
    this.points = null;
    this.position = null;
    this.slices = 128;
    this.resize();
  }
  resize() {
    Object.assign(this, sizeCanvas(this.canvas));
    this.render();
  }
  overview(data) {
    this.points =
      Array.isArray(data?.points) && data.points.length > 1
        ? data.points
        : null;
    this.position = null;
    sampleLoaded = Boolean(this.points);
    $("wave-hint").hidden = sampleLoaded;
    $("sample-state").textContent = sampleLoaded ? "采样已就绪" : "未载入采样";
    $("sample-info").textContent = sampleLoaded
      ? "波形总览 · 刻线代表切片边界"
      : "先载入采样，再用 MIDI 音符触发排序";
    if (Number.isFinite(data?.slices)) this.slices = data.slices;
    this.render();
    renderStatus();
  }
  playback(value) {
    this.position = Number.isFinite(value) ? clamp(value, 0, 1) : null;
    this.render();
  }
  render() {
    if (document.hidden || aboutOpen) return;
    const { ctx: c, w, h } = this;
    if (!w || !h) return;
    c.clearRect(0, 0, w, h);
    c.strokeStyle = "#373d48";
    c.lineWidth = 1;
    c.beginPath();
    c.moveTo(0, h / 2);
    c.lineTo(w, h / 2);
    c.stroke();
    if (!this.points) return;
    c.beginPath();
    this.points.forEach((p, i) => {
      const x = (i / (this.points.length - 1)) * w,
        y =
          h / 2 -
          clamp(Array.isArray(p) ? p[1] : Math.abs(p), -1, 1) * (h / 2 - 4);
      i ? c.lineTo(x, y) : c.moveTo(x, y);
    });
    for (let i = this.points.length - 1; i >= 0; i--) {
      const p = this.points[i];
      c.lineTo(
        (i / (this.points.length - 1)) * w,
        h / 2 -
          clamp(Array.isArray(p) ? p[0] : -Math.abs(p), -1, 1) * (h / 2 - 4),
      );
    }
    c.closePath();
    c.fillStyle = "#799de454";
    c.fill();
    c.strokeStyle = "#98b5ec";
    c.stroke();
    // Bound visible boundary density; do not change the underlying slice count.
    const stride = Math.max(1, Math.ceil(this.slices / (w / 7)));
    c.strokeStyle = "#becbe329";
    for (let i = stride; i < this.slices; i += stride) {
      const x = (i / this.slices) * w;
      c.beginPath();
      c.moveTo(x, 0);
      c.lineTo(x, h);
      c.stroke();
    }
    if (this.position !== null) {
      const x = this.position * w;
      c.strokeStyle = "#eef3fc";
      c.beginPath();
      c.moveTo(x, 0);
      c.lineTo(x, h);
      c.stroke();
    }
  }
}
class VoiceRow {
  constructor(i) {
    this.el = document.createElement("div");
    this.el.className = "viz-row";
    this.el.innerHTML = `<span class="viz-tag">V${i + 1}</span><span class="viz-note">—</span><canvas aria-label="声部 ${i + 1} 粒子次序"></canvas><span class="viz-state">空闲</span><span class="viz-percent">—</span>`;
    $("viz-rows").appendChild(this.el);
    this.canvas = this.el.querySelector("canvas");
    this.state = {};
    this.resize();
  }
  resize() {
    Object.assign(this, sizeCanvas(this.canvas));
    this.draw();
  }
  set(state) {
    this.state = state || {};
    const s = this.state;
    this.active = Boolean(s.active && !s.paused && !s.completed);
    this.el.classList.toggle("active", this.active);
    const status = s.completed
      ? "已完成"
      : s.paused
        ? "已暂停"
        : this.active
          ? "排序中"
          : "空闲";
    this.el.querySelector(".viz-state").textContent = status;
    const validNote =
      Number.isFinite(s.midiNote) &&
      s.midiNote >= 0 &&
      (s.active || s.paused || s.completed);
    this.el.querySelector(".viz-note").textContent = validNote
      ? ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"][
          s.midiNote % 12
        ] +
        (Math.floor(s.midiNote / 12) - 1)
      : "—";
    this.el.querySelector(".viz-percent").textContent =
      s.active || s.paused || s.completed
        ? Math.round(
            (s.completed ? 1 : clamp(Number(s.progress) || 0, 0, 1)) * 100,
          ) + "%"
        : "—";
    this.draw();
  }
  draw() {
    if (document.hidden || aboutOpen) return;
    const { ctx: c, w, h } = this;
    if (!w || !h) return;
    c.clearRect(0, 0, w, h);
    const values = this.state.currentValues;
    if (!Array.isArray(values) || !values.length) return;
    const scale = Math.max(1, (this.state.sliceCount || values.length) - 1),
      bw = w / values.length;
    c.fillStyle = this.state.completed
      ? "#5b817c"
      : this.active
        ? "#557dbb"
        : "#949ca9";
    values.forEach((v, i) => {
      const bh = 2 + clamp(Number(v) / scale, 0, 1) * (h - 3);
      c.fillRect(i * bw + 0.5, h - bh, Math.max(0.5, bw - 1), bh);
    });
  }
}
const wave = new WaveView();
document.querySelectorAll(".knob").forEach((root) => {
  const k = new Knob(root);
  knobs[k.key] = k;
});
for (let i = 0; i < 4; i++) voices.push(new VoiceRow(i));
ALGOS.forEach(([name], i) => {
  const o = document.createElement("option");
  o.value = i;
  o.textContent = name;
  $("algorithm").appendChild(o);
});
$("algorithm").addEventListener("change", (e) =>
  setAlgorithm(Number(e.target.value), true),
);
$("slices").addEventListener("change", (e) => {
  if (e.target.value.trim()) setSlices(Number(e.target.value), true);
  else e.target.value = params.slices;
});
$("slices").addEventListener("keydown", (e) => {
  if (e.key === "Enter") e.target.blur();
  if (e.key === "Escape") {
    e.target.value = params.slices;
    e.target.blur();
  }
});
document
  .querySelectorAll("[data-slices]")
  .forEach(
    (b) => (b.onclick = () => setSlices(Number(b.dataset.slices), true)),
  );
$("btn-turbo").onclick = () => setTurbo(!params.turbo, true);
$("dismiss-status").onclick = () => notify(null);
async function loadFile() {
  finishAll();
  if (!ready) {
    notify("请在插件中载入音频；当前为界面预览。", "info");
    return;
  }
  if (loading) return;
  busy(true);
  notify("请选择音频文件…", "loading");
  const opened = await callNative("loadFile");
  if (!opened) {
    busy(false);
    notify("无法打开文件选择器，请重试。", "error");
  }
}
$("btn-load").onclick = loadFile;
$("wave-hint").onclick = loadFile;
function setAbout(open) {
  finishAll();
  aboutOpen = open;
  $("about").hidden = !open;
  $("app").setAttribute("aria-hidden", String(open));
  if (ready) callNative("setUiActive", !open && !document.hidden);
  if (open) $("close-about").focus();
  else {
    $("brand").focus();
    wave.render();
    voices.forEach((v) => v.draw());
  }
}
$("brand").onclick = () => setAbout(true);
$("close-about").onclick = () => setAbout(false);
$("about").onclick = (e) => {
  if (e.target === $("about")) setAbout(false);
};
document.addEventListener("keydown", (e) => {
  if (aboutOpen) {
    if (e.key === "Escape") {
      e.preventDefault();
      setAbout(false);
    }
    if (e.key === "Tab") {
      e.preventDefault();
      $("close-about").focus();
    }
  }
  if (
    !editable(e.target) &&
    (((e.ctrlKey || e.metaKey) &&
      ["r", "+", "-", "=", "0"].includes(e.key.toLowerCase())) ||
      e.key === "F5")
  )
    e.preventDefault();
});
document.addEventListener("contextmenu", (e) => {
  if (editable(e.target) && e.target.tagName !== "SELECT") return;
  e.preventDefault();
  finishAll();
  const key =
    e.target.closest("[data-param]")?.dataset.param ||
    (e.target === $("algorithm")
      ? "algorithm"
      : e.target === $("slices")
        ? "slices"
        : e.target.closest("#btn-turbo")
          ? "turbo"
          : null);
  if (key && ready)
    callNative("parameterContext", paramId(key), e.clientX, e.clientY);
});
document.addEventListener("pointerover", (e) => {
  const help = e.target.closest("[data-help]");
  $("help-rail").textContent = help?.dataset.help || idleHelp;
});
document.addEventListener("focusin", (e) => {
  $("help-rail").textContent =
    e.target.closest("[data-help]")?.dataset.help || idleHelp;
});
window.addEventListener("blur", finishAll);
window.addEventListener("pagehide", finishAll);
document.addEventListener("visibilitychange", () => {
  finishAll();
  if (ready) callNative("setUiActive", !document.hidden && !aboutOpen);
  if (!document.hidden) {
    wave.render();
    voices.forEach((v) => v.draw());
  }
});
window.addEventListener("resize", () => {
  wave.resize();
  voices.forEach((v) => v.resize());
});
let dragDepth = 0;
function clearDrop() {
  dragDepth = 0;
  $("panel-wave").classList.remove("drop-active");
}
$("panel-wave").addEventListener("dragenter", (e) => {
  e.preventDefault();
  dragDepth++;
  $("panel-wave").classList.add("drop-active");
});
$("panel-wave").addEventListener("dragleave", () => {
  if (--dragDepth <= 0) clearDrop();
});
$("panel-wave").addEventListener("dragover", (e) => {
  e.preventDefault();
  if (e.dataTransfer) e.dataTransfer.dropEffect = loading ? "none" : "copy";
});
$("panel-wave").addEventListener("drop", async (e) => {
  e.preventDefault();
  clearDrop();
  const file = e.dataTransfer?.files?.[0];
  if (!file || loading) return;
  if (!ready) {
    notify("请在插件内拖入音频；当前为界面预览。");
    return;
  }
  if (!/\.(wav|aiff?|flac|ogg|mp3)$/i.test(file.name)) {
    notify("载入失败：请选择 WAV、AIFF、FLAC、OGG 或 MP3。", "error");
    return;
  }
  if (file.size === 0 || file.size > 100 * 1024 * 1024) {
    notify("载入失败：文件为空或超过 100 MB。", "error");
    return;
  }
  busy(true);
  notify("正在读取音频…", "loading");
  try {
    const data = await new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onerror = reject;
      reader.onload = () => resolve(String(reader.result).split(",")[1]);
      reader.readAsDataURL(file);
    });
    const ok = await callNative("loadFileData", file.name, data);
    if (!ok) notify("采样未载入，请检查文件后重试。", "error");
  } catch {
    notify("载入失败：无法读取文件。", "error");
  } finally {
    busy(false);
  }
});
window.addEventListener("dragover", (e) => e.preventDefault());
window.addEventListener("drop", (e) => e.preventDefault());
window.addEventListener("dragend", clearDrop);
window.addEventListener("blur", clearDrop);
function applyVoices(states) {
  if (!Array.isArray(states)) return;
  voices.forEach((v, i) => v.set(states[i]));
  $("voice-count").textContent =
    `${voices.filter((v) => v.active).length} / 4 运行`;
}
function applyFrame(frame) {
  if (!frame) return;
  if (Array.isArray(frame.voices)) applyVoices(frame.voices);
  wave.playback(frame.playbackPosition);
}
window.SieveUI = {
  setSampleOverview: (d) => wave.overview(d),
  setPlaybackPosition: (d) => wave.playback(d?.normalized),
  setVoiceStates: applyVoices,
  setParameterState: applyParameters,
  setUiFrame: applyFrame,
};
setAlgorithm(0);
setSlices(128);
setTurbo(false);
renderEnvelope();
renderStatus();
const backend = window.__JUCE__?.backend;
if (backend) {
  connected = true;
  $("connection").textContent = "正在连接音频引擎…";
  backend.addEventListener("__juce__complete", ({ promiseId, result }) => {
    const p = pending.get(promiseId);
    if (p) {
      clearTimeout(p.timer);
      pending.delete(promiseId);
      p.resolve(result);
    }
  });
  backend.addEventListener("sieveParameterState", applyParameters);
  backend.addEventListener("sieveParameterInfo", (info) => {
    Object.keys(knobs).forEach((key) => {
      const d = info?.[DEFS[key].id];
      if (d) {
        Object.assign(DEFS[key], d);
        knobs[key].render();
      }
    });
  });
  backend.addEventListener("sieveSampleOverview", (d) => wave.overview(d));
  backend.addEventListener("sieveUiFrame", applyFrame);
  backend.addEventListener("sieveStatus", receiveStatus);
  backend.addEventListener("sieveDropState", (active) =>
    $("panel-wave").classList.toggle("drop-active", Boolean(active)),
  );
  callNative("uiReady").then((ok) => {
    ready = Boolean(ok);
    $("connection").textContent = ready
      ? "音频引擎已连接"
      : "连接失败 · 请重新打开界面";
    $("connection").classList.toggle("connected", ready);
  });
}
