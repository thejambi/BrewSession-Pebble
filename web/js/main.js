// Plumbing: the canvas is the watch's framebuffer (200x228, integer-scaled
// and pixelated), the bezel buttons and keyboard are the four buttons, and
// pointer gestures on the screen are the Time 2's touch layer — thresholds
// ported from touch.c. Launch shape ported from main.c.

import { COL } from './colors.js';
import * as S from './session.js';
import * as Win from './windows.js';

const canvas = document.getElementById('screen');
const ctx = canvas.getContext('2d');

function draw() {
  ctx.fillStyle = COL.black;
  ctx.fillRect(0, 0, Win.W, Win.H);
  Win.top()?.draw(ctx);
}

let dirty = false;
Win.setRenderer(() => {
  if (dirty) return;
  dirty = true;
  requestAnimationFrame(() => { dirty = false; draw(); });
});

// ---- buttons ---------------------------------------------------------------
// Click = click; holding Up/Down repeats at the window's own rate (the
// picker runs, the brew walks); holding Select 500ms is the long press.

const REPEAT_DEFAULT = 200;
let repeatTimer = null, longTimer = null, longFired = false;

function press(btn, repeated = false) {
  const w = Win.top();
  if (!w) return;
  if (btn === 'up') w.onUp?.(repeated);
  else if (btn === 'down') w.onDown?.(repeated);
  else if (btn === 'select') w.onSelect?.();
  else if (btn === 'back') { if (!w.onBack?.()) Win.pop(); }
}

function buttonDown(btn) {
  if (btn === 'select') {
    longFired = false;
    longTimer = setTimeout(() => {
      longFired = true;
      Win.top()?.onLongSelect?.();
    }, 500);
    return;
  }
  if (btn === 'up' || btn === 'down') {
    press(btn, false);
    const ms = Win.top()?.repeatMs ?? REPEAT_DEFAULT;
    repeatTimer = setInterval(() => press(btn, true), ms);
    return;
  }
  press(btn);
}

function buttonUp(btn) {
  if (repeatTimer) { clearInterval(repeatTimer); repeatTimer = null; }
  if (btn === 'select') {
    if (longTimer) { clearTimeout(longTimer); longTimer = null; }
    if (!longFired) press('select');
    longFired = true;   // one press per down; a stray later event finds it spent
  }
}

// Leaving a held button cancels it — a slide off the bezel is a change of
// mind, not a press.
function buttonCancel(btn) {
  if (repeatTimer) { clearInterval(repeatTimer); repeatTimer = null; }
  if (btn === 'select') {
    if (longTimer) { clearTimeout(longTimer); longTimer = null; }
    longFired = true;
  }
}

for (const el of document.querySelectorAll('[data-btn]')) {
  const btn = el.dataset.btn;
  el.addEventListener('pointerdown', (e) => { e.preventDefault(); buttonDown(btn); });
  el.addEventListener('pointerup', () => buttonUp(btn));
  el.addEventListener('pointerleave', () => buttonCancel(btn));
  el.addEventListener('pointercancel', () => buttonCancel(btn));
}

const KEYS = { ArrowUp: 'up', ArrowDown: 'down', Enter: 'select',
               Backspace: 'back', Escape: 'back',
               Up: 'up', Down: 'down', Return: 'select', Esc: 'back' };
const heldKeys = new Set();
document.addEventListener('keydown', (e) => {
  const btn = KEYS[e.key];
  if (!btn) return;
  e.preventDefault();
  if (heldKeys.has(e.key)) return;   // let our own repeat logic own repeats
  heldKeys.add(e.key);
  buttonDown(btn);
});
document.addEventListener('keyup', (e) => {
  const btn = KEYS[e.key];
  if (!btn) return;
  heldKeys.delete(e.key);
  buttonUp(btn);
});

// ---- touch (touch.c) -------------------------------------------------------
// Tap within the slop, swipe past it, and — where the window rides along —
// a drag that reports deltas as the finger moves. Same thresholds.

const TAP_SLOP = 14, SWIPE_MIN = 26;
let touch = null;

function toScreen(e) {
  const r = canvas.getBoundingClientRect();
  return { x: (e.clientX - r.left) * Win.W / r.width,
           y: (e.clientY - r.top) * Win.H / r.height };
}

canvas.addEventListener('pointerdown', (e) => {
  e.preventDefault();
  canvas.setPointerCapture(e.pointerId);
  const p = toScreen(e);
  touch = { down: p, lastY: p.y, dragging: false };
});
canvas.addEventListener('pointercancel', () => { touch = null; });

canvas.addEventListener('pointermove', (e) => {
  if (!touch) return;
  const w = Win.top();
  if (!w?.onDrag) return;
  const p = toScreen(e);
  if (!touch.dragging) {
    const t = p.y - touch.down.y;
    if (t < TAP_SLOP && t > -TAP_SLOP) return;
    touch.dragging = true;
    touch.lastY = touch.down.y;
  }
  if (p.y !== touch.lastY) {
    w.onDrag(p.y - touch.lastY);
    touch.lastY = p.y;
  }
});

canvas.addEventListener('pointerup', (e) => {
  if (!touch) return;
  const w = Win.top();
  const p = toScreen(e);
  const dx = p.x - touch.down.x, dy = p.y - touch.down.y;
  const adx = Math.abs(dx), ady = Math.abs(dy);
  if (w?.onDrag) {
    if (touch.dragging) { if (p.y !== touch.lastY) w.onDrag(p.y - touch.lastY); }
    else if (ady >= TAP_SLOP && ady > adx) w.onDrag(dy);
    else if (adx * adx + ady * ady <= TAP_SLOP * TAP_SLOP) w.onTap?.(touch.down.x, touch.down.y);
  } else if (w?.onSwipe && ady >= SWIPE_MIN && ady > adx) {
    w.onSwipe(dy < 0 ? 1 : -1);
  } else if (adx * adx + ady * ady <= TAP_SLOP * TAP_SLOP) {
    w.onTap?.(touch.down.x, touch.down.y);
  }
  touch = null;
});

// Shake to stop, the desk edition: DeviceMotion where it exists.
window.addEventListener('devicemotion', (e) => {
  const a = e.accelerationIncludingGravity;
  if (!a) return;
  if (Math.abs(a.x) + Math.abs(a.y) + Math.abs(a.z) > 35 &&
      S.session.phase === S.PH.ALARM) S.dismiss();
});

// ---- integer scaling -------------------------------------------------------

function fit() {
  const avail = Math.min((window.innerWidth - 120) / Win.W,
                         (window.innerHeight - 120) / Win.H);
  const k = Math.max(1, Math.min(3, Math.floor(avail)));
  canvas.style.width = `${Win.W * k}px`;
  canvas.style.height = `${Win.H * k}px`;
}
window.addEventListener('resize', fit);

// ---- launch (main.c) -------------------------------------------------------

S.init();
S.setListener(Win.render);
S.setAlarmHook(() => Win.push(Win.winBrew));

Win.push(Win.winTitle);
if (S.session.phase === S.PH.STEEPING || S.session.phase === S.PH.ALARM) {
  Win.push(Win.winBrew);
} else if (S.opts.auto_open) {
  if (S.live()) Win.push(Win.winBrew);
  else Win.push(Win.winSession);
}

fit();
Win.render();

if ('serviceWorker' in navigator) {
  navigator.serviceWorker.register('./sw.js').catch(() => {});
}
