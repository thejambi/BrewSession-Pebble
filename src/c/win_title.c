#include "ui.h"
#include "session.h"
#include "touch.h"

// Title: brew session / settings / about, with the live session's status
// along the bottom — because a glance at the menu should already answer
// "where's my tea?"

static Window *s_win;
static Layer *s_layer;
static int s_sel;

// A live session earns a fourth row: Resume jumps back into the brew, and
// New Session keeps the list of customs and recents reachable — switching
// teas mid-session shouldn't require abandoning from inside the brew screen.
static int n_rows(void) { return session_live() ? 4 : 3; }

static const char *row_label(int i) {
  if (!session_live()) i++;   // skip RESUME; BREW SESSION takes slot 1's path
  switch (i) {
    case 0: return "RESUME SESSION";
    case 1: return session_live() ? "NEW SESSION" : "BREW SESSION";
    case 2: return "SETTINGS";
    default: return "ABOUT";
  }
}

// One source of truth for where the rows sit, so a tap lands on the row the
// eye is looking at.
static void row_geom(GRect b, int *y0, int *row_h, int *bx) {
  bool round = IS_ROUND, compact = IS_COMPACT(b);
  *row_h = compact ? 26 : 32;
  *bx = round ? 32 : 12;
  *y0 = round ? (compact ? 52 : 78) : (compact ? 46 : 66);
  // The fourth row would crowd a round bezel's bottom; give back half a row.
  if (round && n_rows() == 4) *y0 -= *row_h / 2;
}

static void draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  bool round = IS_ROUND, compact = IS_COMPACT(b);
  char buf[48], t[12];

  const char *tf = (round && compact) ? FONT_KEY_GOTHIC_18_BOLD
                 : compact            ? FONT_KEY_GOTHIC_24_BOLD
                                      : FONT_KEY_GOTHIC_28_BOLD;
  graphics_context_set_text_color(ctx, COL_TEA);
  graphics_draw_text(ctx, "BREWSESSION", fonts_get_system_font(tf),
                     GRect(0, round ? (compact ? 14 : 18) : 4, b.size.w, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  if (!compact) {
    graphics_context_set_text_color(ctx, COL_DIM);
    graphics_draw_text(ctx, "tea, infusion by infusion",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, round ? 48 : 34, b.size.w, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  int row_h, bx, y;
  row_geom(b, &y, &row_h, &bx);
  for (int i = 0; i < n_rows(); i++) {
    if (i == s_sel) {
      graphics_context_set_fill_color(ctx, COL_TEA);
      graphics_fill_rect(ctx, GRect(bx, y, b.size.w - 2 * bx, row_h - 4), 3, GCornersAll);
      graphics_context_set_text_color(ctx, GColorBlack);
    } else {
      graphics_context_set_text_color(ctx, GColorWhite);
    }
    graphics_draw_text(ctx, row_label(i), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(bx, y - 3, b.size.w - 2 * bx, 24),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    y += row_h;
  }

  // where's my tea?
  if (session_live()) {
    if (g_session.phase == PH_STEEPING) {
      fmt_mmss(t, sizeof t, session_remaining_s());
      snprintf(buf, sizeof buf, "infusion %d - %s left", g_session.infusion, t);
    } else if (g_session.phase == PH_ALARM) {
      snprintf(buf, sizeof buf, "infusion %d done!", g_session.infusion);
    } else {
      snprintf(buf, sizeof buf, "infusion %d ready", g_session.infusion);
    }
    graphics_context_set_text_color(ctx, COL_GOLD);
    graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, b.size.h - (round ? 36 : (compact ? 22 : 26)), b.size.w, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void click_sel(ClickRecognizerRef r, void *ctx) {
  int i = s_sel + (session_live() ? 0 : 1);   // same slot map as row_label
  switch (i) {
    case 0: win_brew_push(); break;
    case 1: win_session_push(); break;
    case 2: win_options_push(); break;
    default: win_about_push(); break;
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

// Tapping a row means it, the way pressing Select on it does.
static void on_tap(GPoint p) {
  if (!s_layer) return;
  GRect b = layer_get_bounds(s_layer);
  int row_h, bx, y;
  row_geom(b, &y, &row_h, &bx);
  for (int i = 0; i < n_rows(); i++, y += row_h) {
    if (p.y < y || p.y >= y + row_h) continue;
    if (p.x < bx || p.x >= b.size.w - bx) return;
    if (i != s_sel) s_sel = i;
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
  // The row count changes with the session's fate between visits.
  if (s_sel >= n_rows()) s_sel = 0;
  touch_begin_full(on_tap, on_swipe);
  session_set_listener(redraw);   // the status line ticks with the steep
  redraw();
}
static void win_disappear(Window *w) {
  touch_end();
  session_set_listener(NULL);
}

void win_title_push(void) {
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
