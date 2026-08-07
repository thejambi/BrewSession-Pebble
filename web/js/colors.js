// The emery palette, hex for hex (ui.h). State is carried by text and
// shape, never by color alone — same discipline, easier here where every
// display is color.
export const COL = {
  black: '#000000',
  white: '#ffffff',
  tea:   '#00aa55',   // GColorMayGreen
  gold:  '#ffaa00',   // GColorChromeYellow
  dim:   '#aaaaaa',   // GColorLightGray
  faint: '#555555',   // GColorDarkGray
  bad:   '#ff0000',   // GColorRed
};

// Pebble's Raster Gothic, approximated by whatever condensed bold sans the
// browser carries. Sizes are tuned to the C layout's line boxes.
const STACK = '"Arial Narrow", "Helvetica Neue Condensed", Arial, sans-serif';
export const FONT = {
  g14:  `12px ${STACK}`,
  g18b: `bold 15px ${STACK}`,
  g24b: `bold 20px ${STACK}`,
  g28b: `bold 24px ${STACK}`,
};

export function fmtMmss(s) {
  if (s < 0) s = 0;
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

export function fmtIncr(s) {
  const sign = s < 0 ? '-' : '+';
  const a = Math.abs(s);
  if (a < 60) return `${sign}${a}s`;
  return `${sign}${Math.floor(a / 60)}:${String(a % 60).padStart(2, '0')}`;
}
