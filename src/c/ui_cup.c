#include "ui.h"

// The launcher teacup, grown up: drawn with primitives instead of pixels so
// it stays crisp at any size on any screen. The bowl doubles as the app's
// only gauge — it fills with tea as the steep progresses, because "how done
// is my tea" should look like tea, not like a loading bar.

void draw_teacup(GContext *ctx, GPoint rim, int r, int fill_pct, int steam) {
  graphics_context_set_stroke_width(ctx, r >= 16 ? 3 : 2);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  // Tea first, level set by covering the unfilled top with background —
  // the outline then paints over the liquid's cut edge.
  if (fill_pct > 0) {
    graphics_context_set_fill_color(ctx, COL_TEA);
    graphics_fill_radial(ctx, GRect(rim.x - r + 2, rim.y - r + 2,
                                    2 * (r - 2), 2 * (r - 2)),
                         GOvalScaleModeFitCircle, r - 2,
                         DEG_TO_TRIGANGLE(90), DEG_TO_TRIGANGLE(270));
    if (fill_pct < 100) {
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, GRect(rim.x - r + 1, rim.y + 1, 2 * r - 2,
                                    (r - 2) * (100 - fill_pct) / 100),
                         0, GCornerNone);
    }
  }

  // bowl, rim, handle, saucer
  graphics_draw_arc(ctx, GRect(rim.x - r, rim.y - r, 2 * r, 2 * r),
                    GOvalScaleModeFitCircle,
                    DEG_TO_TRIGANGLE(90), DEG_TO_TRIGANGLE(270));
  graphics_draw_line(ctx, GPoint(rim.x - r - 3, rim.y),
                     GPoint(rim.x + r + 3, rim.y));
  int rh = r / 2;
  graphics_draw_arc(ctx, GRect(rim.x + r - rh, rim.y + 2, 2 * rh, 2 * rh),
                    GOvalScaleModeFitCircle,
                    DEG_TO_TRIGANGLE(20), DEG_TO_TRIGANGLE(160));
  graphics_draw_line(ctx, GPoint(rim.x - r - 1, rim.y + r + 3),
                     GPoint(rim.x + r + 1, rim.y + r + 3));
  graphics_draw_line(ctx, GPoint(rim.x - r + 5, rim.y + r + 6),
                     GPoint(rim.x + r - 5, rim.y + r + 6));

  // steam: two wisps of offset dashes; the phase flickers them side to
  // side once a second, which is all the animation hot water needs
  if (steam >= 0) {
    int o = steam & 1 ? 2 : 0;
    for (int i = -1; i <= 1; i += 2) {
      int x = rim.x + i * (r / 2 + 1);
      graphics_draw_line(ctx, GPoint(x + o, rim.y - r - 2),
                         GPoint(x + o, rim.y - r + 2));
      graphics_draw_line(ctx, GPoint(x + 2 - o, rim.y - r + 5),
                         GPoint(x + 2 - o, rim.y - r + 9));
    }
  }
}
