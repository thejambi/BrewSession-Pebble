#include "ui.h"
#include "digits.h"

// String-level layout over Emberline's per-glyph digit_draw. Slots are
// ink-width plus one column of air; each glyph centers by its ink, not its
// box (the drawings carry a blank column that would otherwise shove the
// whole line half a column over — see digits.h).
#define INK_COLS   (DIGIT_INK_R - DIGIT_INK_L + 1)
#define COLON_COLS (DIGIT_COLON_R - DIGIT_COLON_L + 1)

int blocky_height(int scale) { return DIGIT_H * scale; }

int blocky_width(const char *s, int scale) {
  int w = 0;
  for (; *s; s++) w += ((*s == ':') ? COLON_COLS : INK_COLS) * scale + scale;
  return w - scale;   // no air after the last glyph
}

void draw_blocky(GContext *ctx, const char *s, int cx, int top, int scale,
                 GColor col) {
  graphics_context_set_fill_color(ctx, col);
  int x = cx - blocky_width(s, scale) / 2;
  for (; *s; s++) {
    bool colon = *s == ':';
    int l = colon ? DIGIT_COLON_L : DIGIT_INK_L;
    digit_draw(ctx, colon ? DIGIT_COLON : *s - '0', x - l * scale, top, scale);
    x += ((colon ? COLON_COLS : INK_COLS) + 1) * scale;
  }
}

void fmt_mmss(char *buf, size_t cap, int s) {
  if (s < 0) s = 0;
  snprintf(buf, cap, "%d:%02d", s / 60, s % 60);
}

// Increments read the way a brewer says them: "+25s each", "-15s each".
// Past a minute the seconds form stops scanning, so it switches shape.
void fmt_incr(char *buf, size_t cap, int s) {
  char sign = s < 0 ? '-' : '+';
  int a = s < 0 ? -s : s;
  if (a < 60) snprintf(buf, cap, "%c%ds", sign, a);
  else snprintf(buf, cap, "%c%d:%02d", sign, a / 60, a % 60);
}
