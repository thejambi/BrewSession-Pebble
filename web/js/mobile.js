// BrewSession for a phone. Same truth (session.js), same pixel identity
// (digits.js, cup.js — drawn small and integer-upscaled so they stay
// chunky), new body: the watch's lit selection row becomes the primary
// button, button-only gestures become touch targets, and the phone chips
// in what a watch never could — a wake lock at the kettle and the
// system's own back gesture.

import { COL, fmtMmss, fmtIncr } from './colors.js';
import { drawBlocky, blockyWidth, blockyHeight } from './digits.js';
import { drawTeacup } from './cup.js';
import * as S from './session.js';

const root = document.getElementById('app');
const UP = 3;   // CSS upscale for pixel-art canvases: chunky on purpose

// ---- tiny DOM helper -------------------------------------------------------

function h(tag, attrs = {}, ...kids) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === 'class') el.className = v;
    else if (k.startsWith('on')) el.addEventListener(k.slice(2), v);
    else el.setAttribute(k, v);
  }
  for (const kid of kids) {
    if (kid == null) continue;
    el.append(kid.nodeType ? kid : document.createTextNode(kid));
  }
  return el;
}

// ---- pixel-art components --------------------------------------------------

// A blocky digit string on its own canvas, redrawable in place.
function blockyEl(scale, color = COL.white) {
  const el = h('canvas');
  let last = '';
  const set = (str) => {
    if (str === last) return;
    last = str;
    el.width = blockyWidth(str, scale);
    el.height = blockyHeight(scale);
    el.style.width = `${el.width * UP}px`;
    el.style.height = `${el.height * UP}px`;
    const ctx = el.getContext('2d');
    ctx.clearRect(0, 0, el.width, el.height);
    drawBlocky(ctx, str, Math.floor(el.width / 2), 0, scale, color);
  };
  return { el, set };
}

// The teacup, redrawable: fill and steam change, geometry doesn't.
function cupEl(r, up = UP) {
  const w = 3 * r + 8, hgt = 2 * r + 14;
  const el = h('canvas', { width: w, height: hgt });
  el.style.width = `${w * up}px`;
  el.style.height = `${hgt * up}px`;
  const set = (fill, steam) => {
    const ctx = el.getContext('2d');
    ctx.clearRect(0, 0, w, hgt);
    drawTeacup(ctx, Math.floor(w / 2), r + 4, r, fill, steam);
  };
  return { el, set };
}

function steamPhase() { return Math.floor(Date.now() / 1000); }

// ---- the wheel -------------------------------------------------------------
// Five values in a column: smaller above, larger below (the phone-picker
// convention the watch settled on). Drag follows the finger one step per
// row; tapping a neighbor walks to it.

function wheel({ peek, bump, format, style = 'blocky' }) {
  const scale = 2;
  const pad = 8;
  const rowH = style === 'blocky' ? blockyHeight(scale) + 2 * pad : 40;
  // Wide enough for the widest value ("00") plus box padding — a lit digit
  // must sit inside its box, never bleed through the wall.
  const wCss = style === 'blocky' ? (blockyWidth('00', scale) + 12) * UP : 132;
  const el = h('canvas', { class: `wheel ${style}` });
  let dragPx = 0;

  const dpr = style === 'text' ? Math.min(2, window.devicePixelRatio || 1) : 1;
  if (style === 'text') el.style.imageRendering = 'auto';
  const wLog = style === 'blocky' ? Math.floor(wCss / UP) : wCss * dpr;
  const hLog = style === 'blocky' ? rowH * 5 : rowH * 5 * dpr;
  el.width = wLog;
  el.height = hLog;
  el.style.width = style === 'blocky' ? `${wLog * UP}px` : `${wCss}px`;
  el.style.height = style === 'blocky' ? `${hLog * UP}px` : `${rowH * 5}px`;

  const draw = () => {
    const ctx = el.getContext('2d');
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    if (style === 'text') ctx.scale(dpr, dpr);
    const wid = style === 'blocky' ? wLog : wCss;
    ctx.clearRect(0, 0, wid, rowH * 5);
    for (let i = -2; i <= 2; i++) {
      const v = format(peek(i));
      const y = (i + 2) * rowH;
      if (i === 0) {
        ctx.fillStyle = COL.tea;
        ctx.beginPath();
        ctx.roundRect(2, y + 2, wid - 4, rowH - 4, 6);
        ctx.fill();
      }
      const col = i === 0 ? COL.black : COL.faint;
      if (style === 'blocky') {
        drawBlocky(ctx, v, Math.floor(wid / 2), y + pad + 1, scale, col);
      } else {
        ctx.fillStyle = col;
        ctx.font = `bold 24px "Arial Narrow", Arial, sans-serif`;
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(v, wid / 2, y + rowH / 2 + 1);
      }
    }
  };
  draw();

  const rowCss = style === 'blocky' ? rowH * UP : rowH;
  let touch = null;
  el.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    el.setPointerCapture(e.pointerId);
    touch = { y0: e.clientY, lastY: e.clientY, moved: false };
    dragPx = 0;
  });
  el.addEventListener('pointermove', (e) => {
    if (!touch) return;
    if (Math.abs(e.clientY - touch.y0) > 8) touch.moved = true;
    if (!touch.moved) return;
    dragPx += e.clientY - touch.lastY;
    touch.lastY = e.clientY;
    while (dragPx >= rowCss) { bump(-1); dragPx -= rowCss; }
    while (dragPx <= -rowCss) { bump(1); dragPx += rowCss; }
    draw();
  });
  el.addEventListener('pointerup', (e) => {
    if (touch && !touch.moved) {
      const r = el.getBoundingClientRect();
      const row = Math.floor((e.clientY - r.top) / rowCss);
      if (row !== 2 && row >= 0 && row <= 4) { bump(row - 2); draw(); }
    }
    touch = null;
  });
  el.addEventListener('pointercancel', () => { touch = null; });

  return { el, draw };
}

// ---- router ----------------------------------------------------------------
// A stack, mirrored into browser history so the system back gesture is the
// Back button. Each screen: mount(container) -> optional { onTick }.

const stack = [];
let current = null;

function mount() {
  root.innerHTML = '';
  const screen = stack[stack.length - 1];
  current = screen.mount(root) || {};
}

function show(screen) {
  stack.push(screen);
  history.pushState({ depth: stack.length }, '');
  mount();
}

function internalPop() {
  if (stack.length <= 1) return;
  stack.pop();
  mount();
}

window.addEventListener('popstate', () => internalPop());
function goBack() { if (stack.length > 1) history.back(); }

function topbar(title) {
  return h('div', { class: 'topbar' },
    h('button', { class: 'back', onclick: goBack, 'aria-label': 'Back' }, '‹'),
    h('div', { class: 'screen-title' }, title));
}

// ---- home ------------------------------------------------------------------

const scrHome = {
  mount(el) {
    const refs = {};
    el.append(
      h('div', { class: 'wordmark' }, 'BREWSESSION'),
      h('div', { class: 'tagline' }, 'tea, infusion by infusion'));

    if (S.live()) {
      const mini = blockyEl(2);
      const stateEl = h('div', { class: 'state' });
      el.append(h('button', { class: 'status-card', onclick: () => show(scrBrew) },
        h('div', { class: 'label-infusion' }, `INFUSION ${S.session.infusion}`),
        mini.el,
        stateEl));
      refs.update = () => {
        if (S.session.phase === S.PH.STEEPING) {
          mini.set(fmtMmss(S.remainingS()));
          stateEl.textContent = 'steeping — tap to open';
        } else if (S.session.phase === S.PH.ALARM) {
          mini.set('0:00');
          stateEl.textContent = 'done! tap to open';
        } else {
          const s = S.steepS();
          mini.set(fmtMmss(s >= 0 ? s : S.session.base_s));
          stateEl.textContent = s >= 0 ? 'ready — tap to open' : 'ready  + ?  tap to open';
        }
      };
      refs.update();
    } else {
      const cup = cupEl(16);
      cup.set(100, -1);
      el.append(h('div', { style: 'display:flex;justify-content:center;margin:6px 0 18px' }, cup.el));
    }

    const rows = h('div', { class: 'rows' },
      h('button', { class: 'btn btn-primary', onclick: () => {
        const initial = S.recentsCount() > 0 ? S.recentsGet(0).base_s : 150;
        showPickerTime('STEEP TIME', initial, (s) => {
          S.sessionNew(s, S.INCR_UNSET);
          show(scrBrew);
        });
      } }, 'New custom time'));
    for (let i = 0; i < S.recentsCount(); i++) {
      const r = S.recentsGet(i);
      const label = fmtMmss(r.base_s);
      const sub = r.increment_s === S.INCR_UNSET ? '' : `${fmtIncr(r.increment_s)} each`;
      rows.append(h('button', { class: 'row-btn', onclick: () => {
        S.sessionNew(r.base_s, r.increment_s);
        show(scrBrew);
      } }, label, sub ? h('span', { class: 'sub' }, sub) : null));
    }
    el.append(rows);

    el.append(h('div', { class: 'footer-links' },
      h('button', { onclick: () => show(scrSettings) }, 'Settings'),
      h('button', { onclick: () => show(scrAbout) }, 'About')));

    return { onTick: refs.update };
  },
};

// ---- pickers ---------------------------------------------------------------

function showPickerTime(title, initial, done) {
  show({
    mount(el) {
      let min = Math.floor(Math.max(0, initial) / 60);
      let sec = Math.floor((Math.max(0, initial) % 60) / 5) * 5;
      const wMin = wheel({
        peek: (d) => (min + d + 21) % 21,
        bump: (d) => { min = (min + d + 21) % 21; },
        format: (v) => String(v),
      });
      const wSec = wheel({
        peek: (d) => (sec + d * 5 + 60) % 60,
        bump: (d) => { sec = (sec + d * 5 + 60) % 60; },
        format: (v) => String(v).padStart(2, '0'),
      });
      const colon = blockyEl(2, COL.dim);
      colon.set(':');
      el.append(
        topbar(title),
        h('div', { class: 'wheelbox' }, wMin.el, h('div', { class: 'wheel-colon' }, colon.el), wSec.el),
        h('div', { class: 'actions' },
          h('button', { class: 'btn btn-primary btn-big', onclick: () => {
            const v = Math.min(S.STEEP_MAX_S, Math.max(S.STEEP_MIN_S, min * 60 + sec));
            history.back();
            setTimeout(() => done(v), 0);
          } }, 'Set')));
    },
  });
}

function showPickerIncr(title, done) {
  show({
    mount(el) {
      let incr = 0;
      const wrap = (v) => v > S.INCR_MAX_S ? S.INCR_MIN_S : v < S.INCR_MIN_S ? S.INCR_MAX_S : v;
      const w = wheel({
        peek: (d) => wrap(incr + d * 5),
        bump: (d) => { incr = wrap(incr + d * 5); },
        format: fmtIncr,
        style: 'text',
      });
      el.append(
        topbar(title),
        h('div', { class: 'hintline', style: 'text-align:center' },
          'added to each next infusion — negative is allowed'),
        h('div', { class: 'wheelbox' }, w.el),
        h('div', { class: 'actions' },
          h('button', { class: 'btn btn-primary btn-big', onclick: () => {
            history.back();
            setTimeout(() => done(incr), 0);
          } }, 'Set')));
    },
  });
}

// ---- brew ------------------------------------------------------------------

const scrBrew = {
  pouring: false, pourLeft: 0, pourTimer: null,

  pourBegin() {
    if (S.opts.pour_s === 0) { this.pourGo(); return; }
    this.pouring = true;
    this.pourLeft = S.opts.pour_s;
    S.playPattern([40]);
    this.pourTimer = setTimeout(() => this.pourStep(), 1000);
    this.rerender();
  },
  pourStep() {
    this.pourTimer = null;
    if (!this.pouring) return;
    this.pourLeft--;
    if (this.pourLeft <= 0) this.pourGo();
    else {
      S.playPattern([40]);
      this.pourTimer = setTimeout(() => this.pourStep(), 1000);
      this.rerender();
    }
  },
  pourGo() {
    S.playPattern([500]);
    this.pouring = false;
    // The repaint must happen even if the steep's side quests (permission
    // prompts, wake locks) throw — a stuck "1" over a running steep is
    // worse than a missing beep.
    try { S.startSteep(); } finally { this.rerender(); }
  },
  pourCancel() {
    this.pouring = false;
    if (this.pourTimer) { clearTimeout(this.pourTimer); this.pourTimer = null; }
    this.rerender();
  },
  startPressed() {
    if (S.needsIncrement()) showPickerIncr('EACH NEXT INFUSION', (s) => {
      S.setIncrement(s);
      this.pourBegin();
    });
    else this.pourBegin();
  },

  rerender() { if (stack[stack.length - 1] === this) mount(); },

  mount(el) {
    const phase = S.session.phase;
    const stage = h('div', { class: 'stage' });
    let onTick = null;

    if (this.pouring) {
      const big = blockyEl(6);
      big.set(String(this.pourLeft));
      stage.append(
        h('div', { class: 'label-gold' }, 'POUR NOW'),
        big.el,
        h('div', { class: 'actions' },
          h('button', { class: 'btn btn-primary btn-big', onclick: () => this.pourGo() }, 'Start now'),
          h('button', { class: 'btn btn-outline', onclick: () => this.pourCancel() }, 'Cancel')));
      el.append(topbar(''), stage);
      return {};
    }

    if (phase === S.PH.ALARM) {
      const cup = cupEl(20);
      cup.set(100, steamPhase());
      stage.append(
        h('div', { class: 'label-infusion' }, `INFUSION ${S.session.infusion}`),
        h('div', { class: 'label-gold' }, 'DONE'),
        cup.el,
        h('div', { class: 'hintline' }, 'tap anywhere to stop'));
      el.append(stage);
      el.addEventListener('pointerdown', () => S.dismiss(), { once: true });
      onTick = () => {
        cup.set(100, steamPhase());
        if (S.session.phase !== S.PH.ALARM) this.rerender();
      };
      return { onTick };
    }

    if (phase === S.PH.STEEPING) {
      const big = blockyEl(3);
      const cup = cupEl(16);
      const paint = () => {
        big.set(fmtMmss(S.remainingS()));
        const total = S.steepS();
        const done = Math.min(total, Math.max(0, total - S.remainingS()));
        cup.set(total > 0 ? Math.floor(100 * done / total) : 0, steamPhase());
        if (S.session.phase !== S.PH.STEEPING) this.rerender();
      };
      paint();
      stage.append(
        h('div', { class: 'label-infusion' }, `INFUSION ${S.session.infusion}`),
        big.el,
        cup.el,
        h('div', { class: 'actions' },
          h('div', { class: 'pair' },
            h('button', { class: 'btn btn-outline', onclick: () => { S.adjustRunning(-5); paint(); } }, '−5s'),
            h('button', { class: 'btn btn-outline', onclick: () => { S.adjustRunning(5); paint(); } }, '+5s')),
          this.abandonLink()));
      el.append(topbar(''), stage);
      return { onTick: paint };
    }

    if (phase === S.PH.READY) {
      const steep = S.steepS();
      const big = blockyEl(3);
      big.set(fmtMmss(steep >= 0 ? steep : S.session.base_s));
      let sub = null;
      if (S.session.override_s > 0) sub = h('div', { class: 'subline' }, 'this one only');
      else if (steep < 0) sub = h('div', { class: 'subline gold' }, '+ ?');
      else if (S.session.increment_s !== S.INCR_UNSET)
        sub = h('div', { class: 'subline' }, `${fmtIncr(S.session.increment_s)} each`);
      stage.append(
        h('div', { class: 'label-infusion' }, `INFUSION ${S.session.infusion}`),
        big.el, sub,
        h('div', { class: 'actions' },
          h('button', { class: 'btn btn-primary btn-big', onclick: () => this.startPressed() }, 'Start'),
          h('div', { class: 'pair' },
            h('button', { class: 'btn btn-outline', onclick: () => {
              let cur = S.steepS();
              if (cur < 0) cur = S.session.base_s;
              showPickerTime('THIS INFUSION', cur, (s) => { S.adjustOnce(s); this.rerender(); });
            } }, 'Adjust'),
            h('button', { class: 'btn btn-outline', onclick: () => { S.skip(); this.rerender(); } }, 'Skip')),
          this.abandonLink()));
      el.append(topbar(''), stage);
      return {};
    }

    // No session (abandoned elsewhere): nothing to brew here.
    goBack();
    return {};
  },

  abandonLink() {
    return h('button', { class: 'abandon', onclick: () => {
      const bar = h('div', { class: 'confirmbar' },
        'Abandon this session?',
        h('button', { onclick: () => { S.abandon(); bar.remove(); goBack(); } }, 'Yes'),
        h('button', { onclick: () => bar.remove() }, 'No'));
      document.body.append(bar);
    } }, 'abandon session');
  },
};

// ---- settings & about ------------------------------------------------------

const scrSettings = {
  mount(el) {
    const rows = h('div', { class: 'rows' });
    const build = () => {
      rows.innerHTML = '';
      rows.append(
        h('button', { class: 'setting', onclick: () => { S.opts.auto_open = !S.opts.auto_open; S.saveOpts(); build(); } },
          h('span', { class: 'name' }, 'Open to session'),
          h('span', { class: 'val' }, S.opts.auto_open ? 'ON' : 'OFF')),
        h('button', { class: 'setting', onclick: () => {
          S.opts.pour_s = { 0: 3, 3: 5, 5: 10, 10: 0 }[S.opts.pour_s] ?? 5;
          S.saveOpts(); build();
        } },
          h('span', { class: 'name' }, 'Pour countdown'),
          h('span', { class: 'val' }, S.opts.pour_s === 0 ? 'OFF' : `${S.opts.pour_s}S`)));
    };
    build();
    el.append(topbar('SETTINGS'), rows);
  },
};

const scrAbout = {
  mount(el) {
    const cup = cupEl(16);
    cup.set(100, -1);
    el.append(topbar('ABOUT'),
      h('div', { class: 'stage' },
        h('div', { class: 'wordmark' }, 'BREWSESSION'),
        h('div', { class: 'tagline' }, 'v1.0.0 (web)'),
        h('div', { class: 'about-text' }, 'A timer that knows which infusion you’re on.'),
        cup.el,
        h('div', { class: 'about-faint' },
          'Keep this tab open while steeping — a closed tab is a silent kettle. Also lives on Pebble watches.')));
  },
};

// ---- wake lock: the phone stays lit at the kettle --------------------------

let wakeLock = null;
async function syncWakeLock() {
  const want = (S.session.phase === S.PH.STEEPING || S.session.phase === S.PH.ALARM) &&
               document.visibilityState === 'visible';
  try {
    if (want && !wakeLock && 'wakeLock' in navigator) {
      wakeLock = await navigator.wakeLock.request('screen');
      wakeLock.addEventListener('release', () => { wakeLock = null; });
    } else if (!want && wakeLock) {
      await wakeLock.release();
      wakeLock = null;
    }
  } catch { wakeLock = null; }
}
document.addEventListener('visibilitychange', syncWakeLock);

// ---- launch ----------------------------------------------------------------

S.init();
S.setListener(() => { current?.onTick?.(); syncWakeLock(); });
S.setAlarmHook(() => { if (stack[stack.length - 1] !== scrBrew) show(scrBrew); });

history.replaceState({ depth: 1 }, '');
stack.push(scrHome);
mount();
if (S.session.phase === S.PH.STEEPING || S.session.phase === S.PH.ALARM) show(scrBrew);
else if (S.opts.auto_open && S.live()) show(scrBrew);
syncWakeLock();

if ('serviceWorker' in navigator) {
  navigator.serviceWorker.register('./sw.js').catch(() => {});
}
