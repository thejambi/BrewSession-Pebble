// The teacup, ported from src/c/ui_cup.c. The bowl doubles as the app's
// only gauge — it fills with tea as the steep progresses.
// Pebble angles run clockwise from 12 o'clock; canvas from 3 o'clock —
// the bowl's 90..270 becomes 0..180, the handle's 20..160 becomes -70..70.

import { COL } from './colors.js';

export function drawTeacup(ctx, cx, rimY, r, fillPct, steam) {
  const lw = r >= 16 ? 3 : 2;
  ctx.lineWidth = lw;
  ctx.strokeStyle = COL.white;

  // Tea first, level set by covering the unfilled top with background —
  // the outline then paints over the liquid's cut edge.
  if (fillPct > 0) {
    ctx.fillStyle = COL.tea;
    ctx.beginPath();
    ctx.arc(cx, rimY, r - 2, 0, Math.PI);
    ctx.closePath();
    ctx.fill();
    if (fillPct < 100) {
      ctx.fillStyle = COL.black;
      const cover = Math.floor((r - 2) * (100 - fillPct) / 100);
      ctx.fillRect(cx - r + 1, rimY, 2 * r - 2, cover + 1);
    }
  }

  // bowl, rim, handle, saucer
  ctx.beginPath();
  ctx.arc(cx, rimY, r - lw / 2, 0, Math.PI);
  ctx.stroke();
  line(ctx, cx - r - 3, rimY, cx + r + 3, rimY);
  const rh = Math.floor(r / 2);
  ctx.beginPath();
  ctx.arc(cx + r, rimY + 2 + rh, rh - lw / 2, -70 * Math.PI / 180, 70 * Math.PI / 180);
  ctx.stroke();
  line(ctx, cx - r - 1, rimY + r + 3, cx + r + 1, rimY + r + 3);
  line(ctx, cx - r + 5, rimY + r + 6, cx + r - 5, rimY + r + 6);

  // steam: two wisps of offset dashes, flickering with the phase's low bit
  if (steam >= 0) {
    const o = (steam & 1) ? 2 : 0;
    for (const i of [-1, 1]) {
      const x = cx + i * (Math.floor(r / 2) + 1);
      line(ctx, x + o, rimY - r - 2, x + o, rimY - r + 2);
      line(ctx, x + 2 - o, rimY - r + 5, x + 2 - o, rimY - r + 9);
    }
  }
}

function line(ctx, x1, y1, x2, y2) {
  // Crisp pixel lines: offset by half the stroke on the minor axis.
  ctx.beginPath();
  if (y1 === y2) { ctx.moveTo(x1, y1 + 0.5); ctx.lineTo(x2, y2 + 0.5); }
  else { ctx.moveTo(x1 + 0.5, y1); ctx.lineTo(x2 + 0.5, y2); }
  ctx.stroke();
}
