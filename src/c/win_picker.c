#include "ui.h"
#include "session.h"
#include "touch.h"

// The one picker, worn two ways: a min:sec time (custom base, one-off
// adjust) and a signed increment in 5s steps — where below zero is real
// territory, for the brewers whose infusions get shorter as they go (R13a).
//
// It draws as a wheel: the lit value in its box, its neighbors above and
// below — larger below, the way every phone picker turns, so a drag up
// climbs toward larger, one step per STEP_PX, several values in one
// stroke. The Up button still increases: that's number-entry muscle
// memory, and the two conventions simply disagree; the thumb gets the
// phone's rule, the button gets the watch's. Tapping the unlit field
// switches to it; tapping anywhere else is Select.

enum { MODE_TIME, MODE_INCR };

#define STEP_PX 32

static Window *s_win;
static Layer *s_layer;
static int s_mode;
static const char *s_title;
static PickerDone s_done;
static int s_min, s_sec;    // MODE_TIME fields
static int s_incr;          // MODE_INCR value
static int s_field;         // 0 = minutes, 1 = seconds
static int s_drag_px;       // finger travel not yet turned into steps

static void bump(int d) {
  if (s_mode == MODE_INCR) {
    s_incr += d * 5;
    if (s_incr > INCR_MAX_S) s_incr = INCR_MIN_S;
    if (s_incr < INCR_MIN_S) s_incr = INCR_MAX_S;
  } else if (s_field == 0) {
    s_min = (s_min + d + 21) % 21;
  } else {
    s_sec = (s_sec + d * 5 + 60) % 60;
  }
  layer_mark_dirty(s_layer);
}

// The value one step either side, wrapped the same way bump wraps, so the
// wheel never shows a number a turn couldn't reach.
static int peek(int d) {
  if (s_mode == MODE_INCR) {
    int v = s_incr + d * 5;
    if (v > INCR_MAX_S) return INCR_MIN_S;
    if (v < INCR_MIN_S) return INCR_MAX_S;
    return v;
  }
  if (s_field == 0) return (s_min + d + 21) % 21;
  return (s_sec + d * 5 + 60) % 60;
}

static void commit(void) {
  PickerDone done = s_done;
  int v;
  if (s_mode == MODE_INCR) {
    v = s_incr;
  } else {
    v = s_min * 60 + s_sec;
    if (v < STEEP_MIN_S) v = STEEP_MIN_S;
    if (v > STEEP_MAX_S) v = STEEP_MAX_S;
  }
  // Pop first: done may push the next window (the pour, the brew screen).
  window_stack_pop(true);
  if (done) done(v);
}

static void click_sel(ClickRecognizerRef r, void *ctx) {
  if (s_mode == MODE_TIME && s_field == 0) {
    s_field = 1;
    s_drag_px = 0;
    layer_mark_dirty(s_layer);
  } else {
    commit();
  }
}

static void click_back(ClickRecognizerRef r, void *ctx) {
  if (s_mode == MODE_TIME && s_field == 1) {
    s_field = 0;   // step back to minutes before leaving altogether
    s_drag_px = 0;
    layer_mark_dirty(s_layer);
  } else {
    window_stack_pop(true);
  }
}

static void click_up(ClickRecognizerRef r, void *ctx)   { bump(1); }
static void click_down(ClickRecognizerRef r, void *ctx) { bump(-1); }

// Content follows the finger: drag up and the column slides up, so the
// larger value living below arrives at the center.
static void on_drag(int dy) {
  s_drag_px += dy;
  while (s_drag_px >= STEP_PX)  { bump(-1); s_drag_px -= STEP_PX; }
  while (s_drag_px <= -STEP_PX) { bump(1);  s_drag_px += STEP_PX; }
}

// ---- layout, one source of truth for draw and tap --------------------------

static void geom(GRect b, int *cy, int *box_h, int *fw, int *gap, int *x0,
                 int *ny_above, int *ny_below) {
  bool compact = IS_COMPACT(b);
  *box_h = compact ? 34 : 44;
  *cy = b.size.h / 2 - (compact ? 20 : 24);
  if (s_mode == MODE_INCR) {
    *fw = 90;
    *gap = 0;
    *x0 = b.size.w / 2 - 45;
  } else {
    *fw = compact ? 40 : 52;
    *gap = compact ? 14 : 16;
    *x0 = b.size.w / 2 - *fw - *gap / 2;
  }
  *ny_above = *cy - (compact ? 26 : 32);
  *ny_below = *cy + *box_h + (compact ? 2 : 4);
}

static void on_tap(GPoint p) {
  if (!s_layer) return;
  if (s_mode == MODE_TIME) {
    GRect b = layer_get_bounds(s_layer);
    int cy, box_h, fw, gap, x0, na, nb;
    geom(b, &cy, &box_h, &fw, &gap, &x0, &na, &nb);
    // A generous vertical band: the finger aims at the column, not the box.
    if (p.y >= cy - box_h && p.y < cy + 2 * box_h) {
      int other = s_field == 0 ? 1 : 0;
      int ox = other == 0 ? x0 : x0 + fw + gap;
      if (p.x >= ox && p.x < ox + fw) {
        s_field = other;
        s_drag_px = 0;
        layer_mark_dirty(s_layer);
        return;
      }
    }
  }
  click_sel(NULL, NULL);
}

static void click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 120, click_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 120, click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, click_sel);
  window_single_click_subscribe(BUTTON_ID_BACK, click_back);
}

// ---- drawing ---------------------------------------------------------------

static void draw_box(GContext *ctx, GRect r, const char *text, bool lit,
                     const char *font) {
  if (lit) {
    graphics_context_set_fill_color(ctx, COL_TEA);
    graphics_fill_rect(ctx, r, 4, GCornersAll);
    graphics_context_set_text_color(ctx, GColorBlack);
  } else {
    graphics_context_set_text_color(ctx, GColorWhite);
  }
  graphics_draw_text(ctx, text, fonts_get_system_font(font),
                     GRect(r.origin.x, r.origin.y + 2, r.size.w, r.size.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_neighbor(GContext *ctx, int x, int y, int w, const char *text,
                          const char *font) {
  graphics_context_set_text_color(ctx, COL_FAINT);
  graphics_draw_text(ctx, text, fonts_get_system_font(font),
                     GRect(x, y, w, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  bool compact = IS_COMPACT(b);
  char buf[12];

  graphics_context_set_text_color(ctx, COL_GOLD);
  graphics_draw_text(ctx, s_title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, IS_ROUND ? (compact ? 12 : 22) : 6, b.size.w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  int cy, box_h, fw, gap, x0, na, nb;
  geom(b, &cy, &box_h, &fw, &gap, &x0, &na, &nb);
  const char *nfont = compact ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_24_BOLD;

  if (s_mode == MODE_INCR) {
    fmt_incr(buf, sizeof buf, s_incr);
    // Signs live outside the numbers font's world; Gothic carries them.
    draw_box(ctx, GRect(x0, cy, fw, box_h), buf, true, FONT_KEY_GOTHIC_28_BOLD);
    fmt_incr(buf, sizeof buf, peek(-1));
    draw_neighbor(ctx, x0, na, fw, buf, nfont);
    fmt_incr(buf, sizeof buf, peek(1));
    draw_neighbor(ctx, x0, nb, fw, buf, nfont);
  } else {
    const char *vfont = compact ? FONT_KEY_GOTHIC_28_BOLD
                                : FONT_KEY_BITHAM_34_MEDIUM_NUMBERS;
    int lx = s_field == 0 ? x0 : x0 + fw + gap;   // the lit column
    snprintf(buf, sizeof buf, "%d", s_min);
    draw_box(ctx, GRect(x0, cy, fw, box_h), buf, s_field == 0, vfont);
    graphics_context_set_text_color(ctx, COL_DIM);
    graphics_draw_text(ctx, ":", fonts_get_system_font(vfont),
                       GRect(x0 + fw, cy + 2, gap, box_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    snprintf(buf, sizeof buf, "%02d", s_sec);
    draw_box(ctx, GRect(x0 + fw + gap, cy, fw, box_h), buf, s_field == 1, vfont);

    // The wheel: smaller above, larger below, only on the column that turns.
    snprintf(buf, sizeof buf, s_field == 0 ? "%d" : "%02d", peek(-1));
    draw_neighbor(ctx, lx, na, fw, buf, nfont);
    snprintf(buf, sizeof buf, s_field == 0 ? "%d" : "%02d", peek(1));
    draw_neighbor(ctx, lx, nb, fw, buf, nfont);
  }

  graphics_context_set_text_color(ctx, COL_FAINT);
  graphics_draw_text(ctx, "select = next", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, b.size.h - (IS_ROUND ? 34 : 20), b.size.w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// ---- window ----------------------------------------------------------------

static void win_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, draw);
  layer_add_child(root, s_layer);
}
static void win_unload(Window *w) { layer_destroy(s_layer); s_layer = NULL; }
static void win_appear(Window *w) {
  s_drag_px = 0;
  touch_begin_drag(on_tap, on_drag);
  if (s_layer) layer_mark_dirty(s_layer);
}
static void win_disappear(Window *w) { touch_end(); }

static void push(int mode, const char *title, int initial_s, PickerDone done) {
  s_mode = mode;
  s_title = title;
  s_done = done;
  s_field = 0;
  if (mode == MODE_INCR) {
    s_incr = initial_s;
  } else {
    if (initial_s < 0) initial_s = 0;
    s_min = initial_s / 60;
    s_sec = (initial_s % 60) / 5 * 5;
  }
  if (!s_win) {
    s_win = window_create();
    window_set_background_color(s_win, GColorBlack);
    window_set_window_handlers(s_win, (WindowHandlers){
      .load = win_load, .unload = win_unload, .appear = win_appear,
      .disappear = win_disappear });
    window_set_click_config_provider(s_win, click_config);
  }
  window_stack_push(s_win, true);
}

void win_picker_push_time(const char *title, int initial_s, PickerDone done) {
  push(MODE_TIME, title, initial_s, done);
}
void win_picker_push_incr(const char *title, int initial_s, PickerDone done) {
  push(MODE_INCR, title, initial_s, done);
}
