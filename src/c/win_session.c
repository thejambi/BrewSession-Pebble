#include "ui.h"
#include "session.h"
#include "touch.h"

// The Brew Session list: resume if one is live, a new custom time, then the
// recents — most recently used on top, because tomorrow's tea is usually
// yesterday's (R4, R5). Recents *are* the presets.

static Window *s_win;
static Layer *s_layer;
static int s_sel;
static int s_top;   // first visible row, so long lists scroll instead of clip

static int n_rows(void) {
  return (session_live() ? 1 : 0) + 1 + recents_count();
}

// Row identity: 0 = resume (when live), then new custom, then recents.
static bool row_is_resume(int i) { return session_live() && i == 0; }
static bool row_is_custom(int i) { return i == (session_live() ? 1 : 0); }
static int  row_recent(int i)    { return i - (session_live() ? 2 : 1); }

static void row_label(int i, char *buf, size_t cap) {
  char t[12], inc[12];
  if (row_is_resume(i)) {
    if (g_session.phase == PH_STEEPING) {
      fmt_mmss(t, sizeof t, session_remaining_s());
      snprintf(buf, cap, "RESUME - %s LEFT", t);
    } else if (g_session.phase == PH_ALARM) {
      snprintf(buf, cap, "RESUME - DONE!");
    } else {
      snprintf(buf, cap, "RESUME - INF %d", g_session.infusion);
    }
  } else if (row_is_custom(i)) {
    snprintf(buf, cap, "NEW CUSTOM TIME");
  } else {
    Recent r = recents_get(row_recent(i));
    fmt_mmss(t, sizeof t, r.base_s);
    if (r.increment_s == INCR_UNSET) snprintf(buf, cap, "%s", t);
    else {
      fmt_incr(inc, sizeof inc, r.increment_s);
      snprintf(buf, cap, "%s  %s", t, inc);
    }
  }
}

static void row_geom(GRect b, int *y0, int *row_h, int *bx, int *visible) {
  bool round = IS_ROUND, compact = IS_COMPACT(b);
  *row_h = compact ? 24 : 28;
  *bx = round ? 34 : 12;
  *y0 = round ? (compact ? 34 : 44) : (compact ? 26 : 32);
  *visible = (b.size.h - *y0 - (round ? 24 : 6)) / *row_h;
  if (*visible < 1) *visible = 1;
}

static void draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  bool compact = IS_COMPACT(b);
  char buf[32];

  graphics_context_set_text_color(ctx, COL_TEA);
  graphics_draw_text(ctx, "BREW SESSION",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, IS_ROUND ? (compact ? 8 : 16) : 2, b.size.w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  int row_h, bx, y, visible;
  row_geom(b, &y, &row_h, &bx, &visible);
  int n = n_rows();
  if (s_sel < s_top) s_top = s_sel;
  if (s_sel >= s_top + visible) s_top = s_sel - visible + 1;

  for (int v = 0; v < visible && s_top + v < n; v++, y += row_h) {
    int i = s_top + v;
    row_label(i, buf, sizeof buf);
    bool resume = row_is_resume(i);
    if (i == s_sel) {
      graphics_context_set_fill_color(ctx, resume ? COL_GOLD : COL_TEA);
      graphics_fill_rect(ctx, GRect(bx, y, b.size.w - 2 * bx, row_h - 4), 3, GCornersAll);
      graphics_context_set_text_color(ctx, GColorBlack);
    } else {
      graphics_context_set_text_color(ctx, resume ? COL_GOLD : GColorWhite);
    }
    graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(bx, y - 3, b.size.w - 2 * bx, 24),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void custom_done(int seconds) {
  session_new(seconds, INCR_UNSET);   // the increment question waits (R13)
  win_brew_push();
}

static void click_sel(ClickRecognizerRef r, void *ctx) {
  if (row_is_resume(s_sel)) {
    win_brew_push();
  } else if (row_is_custom(s_sel)) {
    // Yesterday's base is the best guess at today's (recents may be empty:
    // 2:30 is the doc's canonical cup).
    int initial = recents_count() > 0 ? recents_get(0).base_s : 150;
    win_picker_push_time("STEEP TIME", initial, custom_done);
  } else {
    Recent rec = recents_get(row_recent(s_sel));
    session_new(rec.base_s, rec.increment_s);
    win_brew_push();
  }
}

static void move(int d) {
  int n = n_rows();
  s_sel = (s_sel + d + n) % n;
  layer_mark_dirty(s_layer);
}
static void on_swipe(int d) { move(d); }
static void click_up(ClickRecognizerRef r, void *ctx)   { move(-1); }
static void click_down(ClickRecognizerRef r, void *ctx) { move(1); }

static void on_tap(GPoint p) {
  if (!s_layer) return;
  GRect b = layer_get_bounds(s_layer);
  int row_h, bx, y, visible;
  row_geom(b, &y, &row_h, &bx, &visible);
  int n = n_rows();
  for (int v = 0; v < visible && s_top + v < n; v++, y += row_h) {
    if (p.y < y || p.y >= y + row_h) continue;
    if (p.x < bx || p.x >= b.size.w - bx) return;
    s_sel = s_top + v;
    click_sel(NULL, NULL);
    return;
  }
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, click_sel);
}

static void redraw(void) { if (s_layer) layer_mark_dirty(s_layer); }

static void win_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, draw);
  layer_add_child(root, s_layer);
}
static void win_unload(Window *w) { layer_destroy(s_layer); s_layer = NULL; }
static void win_appear(Window *w) {
  // The list reshapes between visits (a session started or died, recents
  // reordered) — land on the top row, which is also the likeliest pick.
  s_sel = 0;
  s_top = 0;
  touch_begin_full(on_tap, on_swipe);
  session_set_listener(redraw);
  redraw();
}
static void win_disappear(Window *w) {
  touch_end();
  session_set_listener(NULL);
}

void win_session_push(void) {
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
