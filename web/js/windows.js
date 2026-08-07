// The window stack and every screen, ported from the win_*.c files with
// emery's layout constants (200x228, the non-compact branch). Each window
// is draw + handlers; session.js stays the only truth.

import { COL, FONT, fmtMmss, fmtIncr } from './colors.js';
import { drawBlocky, blockyHeight } from './digits.js';
import { drawTeacup } from './cup.js';
import * as S from './session.js';

export const W = 200, H = 228;

const stack = [];
let requestRender = () => {};
export function setRenderer(cb) { requestRender = cb; }
export function render() { requestRender(); }
export function top() { return stack[stack.length - 1]; }

export function push(w) {
  if (stack.includes(w)) return;   // the alarm hook may fire while we're up
  stack.push(w);
  w.appear?.();
  requestRender();
}
export function pop() {
  if (stack.length <= 1) return;   // the title is the floor; Back rests there
  stack.pop().disappear?.();
  top().appear?.();
  requestRender();
}

function line(ctx, text, y, col, font) {
  ctx.fillStyle = col;
  ctx.font = font;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'top';
  ctx.fillText(text, W / 2, y + 3);
}

function steamPhase() { return Math.floor(Date.now() / 1000); }

// ---- title (win_title.c) ---------------------------------------------------

export const winTitle = {
  sel: 0,
  nRows() { return S.live() ? 4 : 3; },
  rowLabel(i) {
    if (!S.live()) i++;
    switch (i) {
      case 0: return 'RESUME SESSION';
      case 1: return S.live() ? 'NEW SESSION' : 'BREW SESSION';
      case 2: return 'SETTINGS';
      default: return 'ABOUT';
    }
  },
  rowGeom() { return { y0: 66, rowH: 32, bx: 12 }; },
  appear() { if (this.sel >= this.nRows()) this.sel = 0; },
  draw(ctx) {
    line(ctx, 'BREWSESSION', 4, COL.tea, FONT.g28b);
    line(ctx, 'tea, infusion by infusion', 34, COL.dim, FONT.g14);
    let { y0: y, rowH, bx } = this.rowGeom();
    for (let i = 0; i < this.nRows(); i++, y += rowH) {
      if (i === this.sel) {
        ctx.fillStyle = COL.tea;
        roundRect(ctx, bx, y, W - 2 * bx, rowH - 4, 3);
        ctx.fillStyle = COL.black;
      }
      line(ctx, this.rowLabel(i), y - 1, i === this.sel ? COL.black : COL.white, FONT.g18b);
    }
    if (S.live()) {
      let status;
      if (S.session.phase === S.PH.STEEPING)
        status = `infusion ${S.session.infusion} - ${fmtMmss(S.remainingS())} left`;
      else if (S.session.phase === S.PH.ALARM)
        status = `infusion ${S.session.infusion} done!`;
      else
        status = `infusion ${S.session.infusion} ready`;
      line(ctx, status, H - 26, COL.gold, FONT.g14);
    } else {
      drawTeacup(ctx, W / 2, H - 36, 13, 100, -1);
    }
  },
  activate() {
    const i = this.sel + (S.live() ? 0 : 1);
    if (i === 0) push(winBrew);
    else if (i === 1) push(winSession);
    else if (i === 2) push(winOptions);
    else push(winAbout);
  },
  move(d) { this.sel = (this.sel + d + this.nRows()) % this.nRows(); render(); },
  onUp() { this.move(-1); },
  onDown() { this.move(1); },
  onSelect() { this.activate(); },
  onSwipe(d) { this.move(d); },
  onTap(x, y) {
    let { y0, rowH, bx } = this.rowGeom();
    for (let i = 0; i < this.nRows(); i++, y0 += rowH) {
      if (y < y0 || y >= y0 + rowH) continue;
      if (x < bx || x >= W - bx) return;
      this.sel = i;
      this.activate();
      return;
    }
  },
};

// ---- brew session list (win_session.c) -------------------------------------

export const winSession = {
  sel: 0, topRow: 0,
  nRows() { return (S.live() ? 1 : 0) + 1 + S.recentsCount(); },
  isResume(i) { return S.live() && i === 0; },
  isCustom(i) { return i === (S.live() ? 1 : 0); },
  recentIdx(i) { return i - (S.live() ? 2 : 1); },
  rowLabel(i) {
    if (this.isResume(i)) {
      if (S.session.phase === S.PH.STEEPING) return `RESUME - ${fmtMmss(S.remainingS())} LEFT`;
      if (S.session.phase === S.PH.ALARM) return 'RESUME - DONE!';
      return `RESUME - INF ${S.session.infusion}`;
    }
    if (this.isCustom(i)) return 'NEW CUSTOM TIME';
    const r = S.recentsGet(this.recentIdx(i));
    if (r.increment_s === S.INCR_UNSET) return fmtMmss(r.base_s);
    return `${fmtMmss(r.base_s)}  ${fmtIncr(r.increment_s)}`;
  },
  rowGeom() {
    const y0 = 32, rowH = 28;
    return { y0, rowH, bx: 12, visible: Math.floor((H - y0 - 6) / rowH) };
  },
  appear() { this.sel = 0; this.topRow = 0; },
  draw(ctx) {
    line(ctx, 'BREW SESSION', 2, COL.tea, FONT.g18b);
    const { y0, rowH, bx, visible } = this.rowGeom();
    const n = this.nRows();
    if (this.sel < this.topRow) this.topRow = this.sel;
    if (this.sel >= this.topRow + visible) this.topRow = this.sel - visible + 1;
    let y = y0;
    for (let v = 0; v < visible && this.topRow + v < n; v++, y += rowH) {
      const i = this.topRow + v;
      const resume = this.isResume(i);
      if (i === this.sel) {
        ctx.fillStyle = resume ? COL.gold : COL.tea;
        roundRect(ctx, bx, y, W - 2 * bx, rowH - 4, 3);
      }
      line(ctx, this.rowLabel(i), y - 1,
           i === this.sel ? COL.black : resume ? COL.gold : COL.white, FONT.g18b);
    }
  },
  activate() {
    if (this.isResume(this.sel)) { push(winBrew); return; }
    if (this.isCustom(this.sel)) {
      const initial = S.recentsCount() > 0 ? S.recentsGet(0).base_s : 150;
      pushPickerTime('STEEP TIME', initial, (s) => {
        S.sessionNew(s, S.INCR_UNSET);
        push(winBrew);
      });
      return;
    }
    const r = S.recentsGet(this.recentIdx(this.sel));
    S.sessionNew(r.base_s, r.increment_s);
    push(winBrew);
  },
  move(d) { const n = this.nRows(); this.sel = (this.sel + d + n) % n; render(); },
  onUp() { this.move(-1); },
  onDown() { this.move(1); },
  onSelect() { this.activate(); },
  onSwipe(d) { this.move(d); },
  onTap(x, y) {
    const { y0, rowH, bx, visible } = this.rowGeom();
    const n = this.nRows();
    let yy = y0;
    for (let v = 0; v < visible && this.topRow + v < n; v++, yy += rowH) {
      if (y < yy || y >= yy + rowH) continue;
      if (x < bx || x >= W - bx) return;
      this.sel = this.topRow + v;
      this.activate();
      return;
    }
  },
};

// ---- picker (win_picker.c) -------------------------------------------------

const STEP_PX = 32;

export const winPicker = {
  mode: 'time', title: '', done: null,
  min: 0, sec: 0, incr: 0, field: 0, dragPx: 0,
  repeatMs: 120,
  geom() {
    const boxH = 44, cy = H / 2 - 24;
    if (this.mode === 'incr') return { cy, boxH, fw: 90, gap: 0, x0: W / 2 - 45 };
    const fw = 60, gap = 16;
    return { cy, boxH, fw, gap, x0: W / 2 - fw - gap / 2 };
  },
  bump(d) {
    if (this.mode === 'incr') {
      this.incr += d * 5;
      if (this.incr > S.INCR_MAX_S) this.incr = S.INCR_MIN_S;
      if (this.incr < S.INCR_MIN_S) this.incr = S.INCR_MAX_S;
    } else if (this.field === 0) this.min = (this.min + d + 21) % 21;
    else this.sec = (this.sec + d * 5 + 60) % 60;
    render();
  },
  peek(d) {
    if (this.mode === 'incr') {
      const v = this.incr + d * 5;
      if (v > S.INCR_MAX_S) return S.INCR_MIN_S;
      if (v < S.INCR_MIN_S) return S.INCR_MAX_S;
      return v;
    }
    if (this.field === 0) return (this.min + d + 21) % 21;
    return (this.sec + d * 5 + 60) % 60;
  },
  commit() {
    const done = this.done;
    let v;
    if (this.mode === 'incr') v = this.incr;
    else v = Math.min(S.STEEP_MAX_S, Math.max(S.STEEP_MIN_S, this.min * 60 + this.sec));
    pop();   // pop first: done may push the next window
    done?.(v);
  },
  onSelect() {
    if (this.mode === 'time' && this.field === 0) { this.field = 1; this.dragPx = 0; render(); }
    else this.commit();
  },
  onBack() {
    if (this.mode === 'time' && this.field === 1) { this.field = 0; this.dragPx = 0; render(); return true; }
    return false;
  },
  onUp() { this.bump(1); },
  onDown() { this.bump(-1); },
  // Content follows the finger: drag up and the column slides up, so the
  // larger value living below arrives at the center.
  onDrag(dy) {
    this.dragPx += dy;
    while (this.dragPx >= STEP_PX) { this.bump(-1); this.dragPx -= STEP_PX; }
    while (this.dragPx <= -STEP_PX) { this.bump(1); this.dragPx += STEP_PX; }
  },
  onTap(x, y) {
    if (this.mode === 'time') {
      const { cy, boxH, fw, gap, x0 } = this.geom();
      if (y >= cy - boxH && y < cy + 2 * boxH) {
        const other = this.field === 0 ? 1 : 0;
        const ox = other === 0 ? x0 : x0 + fw + gap;
        if (x >= ox && x < ox + fw) { this.field = other; this.dragPx = 0; render(); return; }
      }
    }
    this.onSelect();
  },
  draw(ctx) {
    line(ctx, this.title, 6, COL.gold, FONT.g18b);
    const { cy, boxH, fw, gap, x0 } = this.geom();
    if (this.mode === 'incr') {
      boxText(ctx, x0, cy, fw, boxH, fmtIncr(this.incr), true, FONT.g28b);
      line2(ctx, fmtIncr(this.peek(-1)), x0 + fw / 2, cy - 32, COL.faint, FONT.g24b);
      line2(ctx, fmtIncr(this.peek(1)), x0 + fw / 2, cy + boxH + 4, COL.faint, FONT.g24b);
    } else {
      const scale = 3;
      const lx = this.field === 0 ? x0 : x0 + fw + gap;
      boxBlocky(ctx, x0, cy, fw, boxH, String(this.min), this.field === 0, scale);
      drawBlocky(ctx, ':', x0 + fw + gap / 2, cy + (boxH - blockyHeight(scale)) / 2, scale, COL.dim);
      boxBlocky(ctx, x0 + fw + gap, cy, fw, boxH,
                String(this.sec).padStart(2, '0'), this.field === 1, scale);
      const ns = scale - 1, nh = blockyHeight(ns);
      const above = this.field === 0 ? String(this.peek(-1)) : String(this.peek(-1)).padStart(2, '0');
      const below = this.field === 0 ? String(this.peek(1)) : String(this.peek(1)).padStart(2, '0');
      drawBlocky(ctx, above, lx + fw / 2, cy - 6 - nh, ns, COL.faint);
      drawBlocky(ctx, below, lx + fw / 2, cy + boxH + 6, ns, COL.faint);
    }
    line(ctx, 'select = next', H - 20, COL.faint, FONT.g14);
  },
};

function pushPicker(mode, title, initial, done) {
  winPicker.mode = mode;
  winPicker.title = title;
  winPicker.done = done;
  winPicker.field = 0;
  winPicker.dragPx = 0;
  if (mode === 'incr') winPicker.incr = initial;
  else {
    initial = Math.max(0, initial);
    winPicker.min = Math.floor(initial / 60);
    winPicker.sec = Math.floor((initial % 60) / 5) * 5;
  }
  push(winPicker);
}
export const pushPickerTime = (t, i, d) => pushPicker('time', t, i, d);
export const pushPickerIncr = (t, i, d) => pushPicker('incr', t, i, d);

// ---- brew (win_brew.c) -----------------------------------------------------

export const winBrew = {
  pouring: false, pourLeft: 0, pourTimer: null, confirm: false,
  repeatMs: 200,
  appear() { this.confirm = false; },
  pourBegin() {
    if (S.opts.pour_s === 0) { this.pourGo(); return; }
    this.pouring = true;
    this.pourLeft = S.opts.pour_s;
    S.playPattern([40]);
    this.pourTimer = setTimeout(() => this.pourStep(), 1000);
    render();
  },
  pourStep() {
    this.pourTimer = null;
    if (!this.pouring) return;
    this.pourLeft--;
    if (this.pourLeft <= 0) this.pourGo();
    else {
      S.playPattern([40]);
      this.pourTimer = setTimeout(() => this.pourStep(), 1000);
      render();
    }
  },
  pourGo() {
    S.playPattern([500]);   // one long buzz, wrist-tested at 500ms
    this.pouring = false;
    // Repaint even if the steep's side quests throw (see mobile.js).
    try { S.startSteep(); } finally { render(); }
  },
  pourCancel() {
    this.pouring = false;
    if (this.pourTimer) { clearTimeout(this.pourTimer); this.pourTimer = null; }
    render();
  },
  startPressed() {
    if (S.needsIncrement()) {
      pushPickerIncr('EACH NEXT INFUSION', 0, (s) => {
        S.setIncrement(s);
        this.pourBegin();
      });
    } else this.pourBegin();
  },
  adjustPressed() {
    let cur = S.steepS();
    if (cur < 0) cur = S.session.base_s;
    pushPickerTime('THIS INFUSION', cur, (s) => { S.adjustOnce(s); render(); });
  },
  onSelect() {
    if (this.confirm) { this.confirm = false; S.abandon(); pop(); return; }
    switch (S.session.phase) {
      case S.PH.ALARM: S.dismiss(); break;
      case S.PH.READY: this.pouring ? this.pourGo() : this.startPressed(); break;
      default: break;   // steeping: Select rests; you can't pause leaves
    }
  },
  onLongSelect() {
    if ((S.session.phase === S.PH.READY || S.session.phase === S.PH.STEEPING) && !this.pouring) {
      this.confirm = true;
      render();
    }
  },
  updown(d, repeated) {
    if (this.confirm) { this.confirm = false; render(); return; }
    switch (S.session.phase) {
      case S.PH.ALARM: S.dismiss(); break;
      case S.PH.STEEPING:
        // Up gives the leaves more time, Down takes it away.
        S.adjustRunning(-d * 5);
        render();
        break;
      case S.PH.READY:
        if (this.pouring || repeated) break;
        if (d > 0) { S.skip(); render(); }
        else this.adjustPressed();
        break;
    }
  },
  onUp(repeated) { this.updown(-1, repeated); },
  onDown(repeated) { this.updown(1, repeated); },
  onBack() {
    if (this.confirm) { this.confirm = false; render(); return true; }
    if (this.pouring) { this.pourCancel(); return true; }
    if (S.session.phase === S.PH.ALARM) { S.dismiss(); return true; }
    return false;   // walk away; the session keeps
  },
  onTap() {
    if (S.session.phase === S.PH.ALARM) S.dismiss();
    else if (S.session.phase === S.PH.READY) this.onSelect();
  },
  onSwipe(d) {
    if (S.session.phase === S.PH.STEEPING) this.updown(d, false);
    else if (S.session.phase === S.PH.READY && !this.pouring) this.updown(d, false);
  },
  draw(ctx) {
    const scale = 4, yTop = 8, cy = H / 2 - 30;
    const label = `INFUSION ${S.session.infusion}`;
    if (this.pouring) {
      line(ctx, 'POUR NOW', yTop + 14, COL.gold, FONT.g28b);
      drawBlocky(ctx, String(this.pourLeft), W / 2, cy + 10, scale, COL.white);
      line(ctx, 'select = start now', H - 22, COL.faint, FONT.g14);
    } else if (S.session.phase === S.PH.ALARM) {
      line(ctx, label, yTop, COL.tea, FONT.g18b);
      line(ctx, 'DONE', cy - 6, COL.gold, FONT.g28b);
      drawTeacup(ctx, W / 2, cy + 48, 16, 100, steamPhase());
      line(ctx, 'shake to stop', H - 22, COL.faint, FONT.g14);
    } else if (S.session.phase === S.PH.STEEPING) {
      line(ctx, label, yTop, COL.tea, FONT.g18b);
      drawBlocky(ctx, fmtMmss(S.remainingS()), W / 2, cy + 2, scale, COL.white);
      line(ctx, 'up/down: +/-5s', cy + 58, COL.faint, FONT.g14);
      const total = S.session.end_epoch !== 0 ? S.steepS() : 0;
      if (total > 0) {
        const done = Math.min(total, Math.max(0, total - S.remainingS()));
        drawTeacup(ctx, W / 2, H - 30, 16, Math.floor(100 * done / total), steamPhase());
      }
    } else {   // READY
      line(ctx, label, yTop, COL.tea, FONT.g18b);
      const steep = S.steepS();
      drawBlocky(ctx, fmtMmss(steep >= 0 ? steep : S.session.base_s), W / 2, cy + 2, scale, COL.white);
      let sub = '';
      if (S.session.override_s > 0) sub = 'this one only';
      else if (steep < 0) sub = '+ ?';
      else if (S.session.increment_s !== S.INCR_UNSET) sub = `${fmtIncr(S.session.increment_s)} each`;
      if (sub) line(ctx, sub, cy + 58, steep < 0 ? COL.gold : COL.dim, FONT.g18b);
      line(ctx, 'select = start   down = skip', H - 22, COL.faint, FONT.g14);
    }
    if (this.confirm) {
      ctx.fillStyle = COL.bad;
      ctx.fillRect(0, H - 26, W, 26);
      line(ctx, 'ABANDON? SELECT = YES', H - 25, COL.white, FONT.g18b);
    }
  },
};

// ---- options (win_options.c) -----------------------------------------------

export const winOptions = {
  sel: 0,
  rowGeom() { return { y0: 44, rowH: 46, bx: 12 }; },
  rowText(i) {
    if (i === 0) return ['OPEN TO SESSION', S.opts.auto_open ? 'ON' : 'OFF'];
    return ['POUR COUNTDOWN', S.opts.pour_s === 0 ? 'OFF' : `${S.opts.pour_s}S`];
  },
  cycle(i) {
    if (i === 0) S.opts.auto_open = !S.opts.auto_open;
    else S.opts.pour_s = { 0: 3, 3: 5, 5: 10, 10: 0 }[S.opts.pour_s] ?? 5;
    S.saveOpts();
    render();
  },
  draw(ctx) {
    line(ctx, 'SETTINGS', 4, COL.tea, FONT.g18b);
    let { y0: y, rowH, bx } = this.rowGeom();
    for (let i = 0; i < 2; i++, y += rowH) {
      const [name, val] = this.rowText(i);
      if (i === this.sel) {
        ctx.fillStyle = COL.tea;
        roundRect(ctx, bx, y, W - 2 * bx, rowH - 6, 3);
      }
      const col = i === this.sel ? COL.black : COL.white;
      line(ctx, name, y + 2, col, FONT.g14);
      line(ctx, val, y + 16, col, FONT.g18b);
    }
    line(ctx, 'select = change', H - 20, COL.faint, FONT.g14);
  },
  move(d) { this.sel = (this.sel + d + 2) % 2; render(); },
  onUp() { this.move(-1); },
  onDown() { this.move(1); },
  onSelect() { this.cycle(this.sel); },
  onSwipe(d) { this.move(d); },
  onTap(x, y) {
    let { y0, rowH } = this.rowGeom();
    for (let i = 0; i < 2; i++, y0 += rowH) {
      if (y >= y0 && y < y0 + rowH) { this.sel = i; this.cycle(i); return; }
    }
  },
};

// ---- about (win_about.c) ---------------------------------------------------

export const winAbout = {
  draw(ctx) {
    line(ctx, 'BREWSESSION', 10, COL.tea, FONT.g24b);
    line(ctx, 'v1.0.0 (web)', 38, COL.dim, FONT.g14);
    ['A timer that knows', 'which infusion', "you're on."].forEach((t, i) =>
      line(ctx, t, 66 + i * 20, COL.white, FONT.g18b));
    line(ctx, 'a shot-for-shot port of', 136, COL.faint, FONT.g14);
    line(ctx, 'the Pebble watchapp', 152, COL.faint, FONT.g14);
    drawTeacup(ctx, W / 2, H - 40, 13, 100, -1);
  },
};

// ---- shared shapes ---------------------------------------------------------

function roundRect(ctx, x, y, w, h, r) {
  ctx.beginPath();
  ctx.roundRect(x, y, w, h, r);
  ctx.fill();
}

function boxText(ctx, x, y, w, h, text, lit, font) {
  if (lit) { ctx.fillStyle = COL.tea; roundRect(ctx, x, y, w, h, 4); }
  ctx.fillStyle = lit ? COL.black : COL.white;
  ctx.font = font;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(text, x + w / 2, y + h / 2 + 1);
  ctx.textBaseline = 'top';
}

function boxBlocky(ctx, x, y, w, h, text, lit, scale) {
  if (lit) { ctx.fillStyle = COL.tea; roundRect(ctx, x, y, w, h, 4); }
  drawBlocky(ctx, text, x + w / 2, y + Math.floor((h - blockyHeight(scale)) / 2),
             scale, lit ? COL.black : COL.white);
}

function line2(ctx, text, cx, y, col, font) {
  ctx.fillStyle = col;
  ctx.font = font;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'top';
  ctx.fillText(text, cx, y + 3);
}
