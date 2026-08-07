#include "ui.h"
#include "opts.h"
#include "touch.h"

// Settings, deliberately tiny (R24): open-to-session and the pour countdown.
// Select cycles the value; changes save as they're made.

static Window *s_win;
static Layer *s_layer;
static int s_sel;

#define N_ROWS 2

static void row_geom(GRect b, int *y0, int *row_h, int *bx) {
  bool round = IS_ROUND, compact = IS_COMPACT(b);
  *row_h = compact ? 40 : 46;
  *bx = round ? 30 : 12;
  *y0 = round ? (compact ? 44 : 58) : (compact ? 34 : 44);
}

static void row_text(int i, char *name, size_t ncap, char *val, size_t vcap) {
  if (i == 0) {
    snprintf(name, ncap, "OPEN TO SESSION");
    snprintf(val, vcap, g_opts.auto_open ? "ON" : "OFF");
  } else {
    snprintf(name, ncap, "POUR COUNTDOWN");
    if (g_opts.pour_s == 0) snprintf(val, vcap, "OFF");
    else snprintf(val, vcap, "%dS", g_opts.pour_s);
  }
}

static void draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  char name[24], val[8];

  graphics_context_set_text_color(ctx, COL_TEA);
  graphics_draw_text(ctx, "SETTINGS", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, IS_ROUND ? 16 : 4, b.size.w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  int row_h, bx, y;
  row_geom(b, &y, &row_h, &bx);
  for (int i = 0; i < N_ROWS; i++, y += row_h) {
    row_text(i, name, sizeof name, val, sizeof val);
    if (i == s_sel) {
      graphics_context_set_fill_color(ctx, COL_TEA);
      graphics_fill_rect(ctx, GRect(bx, y, b.size.w - 2 * bx, row_h - 6), 3, GCornersAll);
      graphics_context_set_text_color(ctx, GColorBlack);
    } else {
      graphics_context_set_text_color(ctx, GColorWhite);
    }
    graphics_draw_text(ctx, name, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(bx, y, b.size.w - 2 * bx, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, val, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(bx, y + 14, b.size.w - 2 * bx, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  graphics_context_set_text_color(ctx, COL_FAINT);
  graphics_draw_text(ctx, "select = change", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, b.size.h - (IS_ROUND ? 34 : 20), b.size.w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void cycle(int i) {
  if (i == 0) {
    g_opts.auto_open = !g_opts.auto_open;
  } else {
    // Off -> 3 -> 5 -> 10 -> Off: the whole R9 menu on one button.
    switch (g_opts.pour_s) {
      case 0: g_opts.pour_s = 3; break;
      case 3: g_opts.pour_s = 5; break;
      case 5: g_opts.pour_s = 10; break;
      default: g_opts.pour_s = 0; break;
    }
  }
  opts_save();
  layer_mark_dirty(s_layer);
}

static void click_sel(ClickRecognizerRef r, void *ctx) { cycle(s_sel); }
static void move(int d) {
  s_sel = (s_sel + d + N_ROWS) % N_ROWS;
  layer_mark_dirty(s_layer);
}
static void on_swipe(int d) { move(d); }
static void click_up(ClickRecognizerRef r, void *ctx)   { move(-1); }
static void click_down(ClickRecognizerRef r, void *ctx) { move(1); }

static void on_tap(GPoint p) {
  if (!s_layer) return;
  GRect b = layer_get_bounds(s_layer);
  int row_h, bx, y;
  row_geom(b, &y, &row_h, &bx);
  for (int i = 0; i < N_ROWS; i++, y += row_h) {
    if (p.y < y || p.y >= y + row_h) continue;
    if (i != s_sel) s_sel = i;
    cycle(i);
    return;
  }
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, click_sel);
}

static void win_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, draw);
  layer_add_child(root, s_layer);
}
static void win_unload(Window *w) { layer_destroy(s_layer); s_layer = NULL; }
static void win_appear(Window *w) { touch_begin_full(on_tap, on_swipe); }
static void win_disappear(Window *w) { touch_end(); }

void win_options_push(void) {
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
