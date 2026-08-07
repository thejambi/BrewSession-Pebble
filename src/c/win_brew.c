#include "ui.h"
#include "session.h"
#include "opts.h"
#include "touch.h"

// The brew screen: ready, pour countdown, steeping, alarm — one window,
// four faces, because they share their bones and flow into each other
// (§7). Session state is truth; the only thing living here is the pour
// countdown, which isn't worth remembering (a pour you left is a pour
// that didn't happen).

static Window *s_win;
static Layer *s_layer;
static bool s_pouring;
static int s_pour_left;
static AppTimer *s_pour_timer;
static bool s_confirm;   // abandon? awaiting the confirming Select

static void redraw(void) { if (s_layer) layer_mark_dirty(s_layer); }

// ---- pour countdown (R7, R7a) ----------------------------------------------
// Felt, not just seen: a tick each second, a double-pulse at "go", so the
// wrist carries the count through the steam.

static void pour_tick_pulse(void) {
  static const uint32_t TICK[] = { 40 };
  vibes_enqueue_custom_pattern((VibePattern){ TICK, ARRAY_LENGTH(TICK) });
}

static void pour_go(void) {
  static const uint32_t GO[] = { 70, 80, 70 };
  vibes_enqueue_custom_pattern((VibePattern){ GO, ARRAY_LENGTH(GO) });
  s_pouring = false;
  session_start_steep();
  redraw();
}

static void pour_step(void *ctx) {
  s_pour_timer = NULL;
  if (!s_pouring) return;
  s_pour_left--;
  if (s_pour_left <= 0) {
    pour_go();
  } else {
    pour_tick_pulse();
    s_pour_timer = app_timer_register(1000, pour_step, NULL);
    redraw();
  }
}

static void pour_begin(void) {
  if (g_opts.pour_s == 0) { pour_go(); return; }   // Off: Start means now (R9)
  s_pouring = true;
  s_pour_left = g_opts.pour_s;
  pour_tick_pulse();
  s_pour_timer = app_timer_register(1000, pour_step, NULL);
  redraw();
}

static void pour_cancel(void) {
  s_pouring = false;
  if (s_pour_timer) { app_timer_cancel(s_pour_timer); s_pour_timer = NULL; }
  redraw();
}

// ---- picker callbacks ------------------------------------------------------

static void incr_done(int s) {
  session_set_increment(s);
  pour_begin();   // commitment was the question; brewing is the answer (R13)
}

static void adjust_done(int s) {
  session_adjust_once(s);
  redraw();
}

// ---- controls --------------------------------------------------------------

static void start_pressed(void) {
  if (session_needs_increment()) {
    win_picker_push_incr("EACH NEXT INFUSION", 0, incr_done);
  } else {
    pour_begin();
  }
}

static void adjust_pressed(void) {
  int cur = session_steep_s();
  if (cur < 0) cur = g_session.base_s;
  win_picker_push_time("THIS INFUSION", cur, adjust_done);
}

static void click_sel(ClickRecognizerRef r, void *ctx) {
  if (s_confirm) {
    s_confirm = false;
    session_abandon();
    window_stack_pop(true);
    return;
  }
  switch (g_session.phase) {
    case PH_ALARM: session_dismiss(); break;
    case PH_READY: s_pouring ? pour_go() : start_pressed(); break;
    default: break;   // steeping: Select rests; you can't pause leaves
  }
}

static void click_sel_long(ClickRecognizerRef r, void *ctx) {
  // Long Select asks the only destructive question here. (Long Back is the
  // system's own exit and can't be borrowed for this.)
  if (g_session.phase == PH_READY || g_session.phase == PH_STEEPING) {
    if (s_pouring) return;
    s_confirm = true;
    redraw();
  }
}

static void updown(int d, bool repeated) {
  if (s_confirm) { s_confirm = false; redraw(); return; }
  switch (g_session.phase) {
    case PH_ALARM:
      session_dismiss();
      break;
    case PH_STEEPING:
      session_adjust_running(d * 5);   // hold to keep feeding it (R11)
      redraw();
      break;
    case PH_READY:
      if (s_pouring || repeated) break;   // a held button shouldn't skip twice
      if (d > 0) { session_skip(); redraw(); }
      else adjust_pressed();
      break;
    default: break;
  }
}

static void click_up(ClickRecognizerRef r, void *ctx) {
  updown(-1, click_number_of_clicks_counted(r) > 1);
}
static void click_down(ClickRecognizerRef r, void *ctx) {
  updown(1, click_number_of_clicks_counted(r) > 1);
}

static void click_back(ClickRecognizerRef r, void *ctx) {
  if (s_confirm) { s_confirm = false; redraw(); return; }
  if (s_pouring) { pour_cancel(); return; }   // back to ready, kettle still up
  if (g_session.phase == PH_ALARM) { session_dismiss(); return; }
  window_stack_pop(true);   // walk away; the session keeps (R20, R22)
}

static void on_tap(GPoint p) {
  switch (g_session.phase) {
    case PH_ALARM: session_dismiss(); break;
    case PH_READY: click_sel(NULL, NULL); break;
    default: break;   // a stray finger shouldn't nudge a running steep
  }
}

static void on_swipe(int d) {
  if (g_session.phase == PH_STEEPING) updown(d, false);
  else if (g_session.phase == PH_READY && !s_pouring) updown(d, false);
}

static void click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 200, click_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200, click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, click_sel);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, click_sel_long, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, click_back);
}

// ---- drawing ---------------------------------------------------------------

static void draw_line(GContext *ctx, GRect b, const char *text, int y,
                      GColor col, const char *font_key) {
  graphics_context_set_text_color(ctx, col);
  graphics_draw_text(ctx, text, fonts_get_system_font(font_key),
                     GRect(0, y, b.size.w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  bool compact = IS_COMPACT(b);
  char buf[32], t[12];
  int scale = compact ? 3 : 4;   // blocky digits: 39px or 52px tall
  int y_top = IS_ROUND ? (compact ? 16 : 26) : 8;
  int cy = b.size.h / 2 - (compact ? 26 : 30);

  snprintf(buf, sizeof buf, "INFUSION %d", g_session.infusion);

  if (s_pouring) {
    draw_line(ctx, b, "POUR NOW", y_top + (compact ? 6 : 14), COL_GOLD,
              FONT_KEY_GOTHIC_28_BOLD);
    snprintf(t, sizeof t, "%d", s_pour_left);
    draw_blocky(ctx, t, b.size.w / 2, cy + 10, scale, GColorWhite);
    draw_line(ctx, b, "select = start now", b.size.h - (IS_ROUND ? 36 : 22),
              COL_FAINT, FONT_KEY_GOTHIC_14);

  } else if (g_session.phase == PH_ALARM) {
    draw_line(ctx, b, buf, y_top, COL_TEA, FONT_KEY_GOTHIC_18_BOLD);
    draw_line(ctx, b, "DONE", cy - 6, COL_GOLD, FONT_KEY_GOTHIC_28_BOLD);
    // The full cup, steaming: the reward, drawn instead of described.
    draw_teacup(ctx, GPoint(b.size.w / 2, cy + (compact ? 40 : 48)),
                compact ? 12 : 16, 100, time(NULL));
    draw_line(ctx, b, "shake to stop", b.size.h - (IS_ROUND ? 36 : 22),
              COL_FAINT, FONT_KEY_GOTHIC_14);

  } else if (g_session.phase == PH_STEEPING) {
    draw_line(ctx, b, buf, y_top, COL_TEA, FONT_KEY_GOTHIC_18_BOLD);
    fmt_mmss(t, sizeof t, session_remaining_s());
    draw_blocky(ctx, t, b.size.w / 2, cy + 2, scale, GColorWhite);
    draw_line(ctx, b, "up/down: +/-5s", cy + (compact ? 44 : 58),
              COL_FAINT, FONT_KEY_GOTHIC_14);
    // The cup fills as the steep goes — the progress gauge is the tea.
    int total = g_session.end_epoch != 0 ? session_steep_s() : 0;
    if (total > 0) {
      int done = total - session_remaining_s();
      if (done < 0) done = 0;
      if (done > total) done = total;
      draw_teacup(ctx, GPoint(b.size.w / 2, b.size.h - (IS_ROUND ? 40 : 30)),
                  compact ? 12 : 16, 100 * done / total, time(NULL));
    }

  } else {   // READY
    draw_line(ctx, b, buf, y_top, COL_TEA, FONT_KEY_GOTHIC_18_BOLD);
    int steep = session_steep_s();
    fmt_mmss(t, sizeof t, steep >= 0 ? steep : g_session.base_s);
    draw_blocky(ctx, t, b.size.w / 2, cy + 2, scale, GColorWhite);
    // The line under the time tells the increment's story: known, one-off,
    // or the honest "+ ?" of a question not yet asked (R13).
    if (g_session.override_s > 0) {
      snprintf(buf, sizeof buf, "this one only");
    } else if (steep < 0) {
      snprintf(buf, sizeof buf, "+ ?");
    } else if (g_session.increment_s != INCR_UNSET) {
      fmt_incr(t, sizeof t, g_session.increment_s);
      snprintf(buf, sizeof buf, "%s each", t);
    } else {
      buf[0] = '\0';
    }
    if (buf[0])
      draw_line(ctx, b, buf, cy + (compact ? 44 : 58),
                steep < 0 ? COL_GOLD : COL_DIM, FONT_KEY_GOTHIC_18_BOLD);
    draw_line(ctx, b, "select = start   down = skip",
              b.size.h - (IS_ROUND ? 36 : 22), COL_FAINT, FONT_KEY_GOTHIC_14);
  }

  if (s_confirm) {
    graphics_context_set_fill_color(ctx, COL_BAD);
    graphics_fill_rect(ctx, GRect(0, b.size.h - 26, b.size.w, 26), 0, GCornerNone);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "ABANDON? SELECT = YES",
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(0, b.size.h - 26, b.size.w, 24),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
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
  s_confirm = false;
  touch_begin_full(on_tap, on_swipe);
  session_set_listener(redraw);
  redraw();
}
static void win_disappear(Window *w) {
  touch_end();
  session_set_listener(NULL);
}

void win_brew_push(void) {
  if (!s_win) {
    s_win = window_create();
    window_set_background_color(s_win, GColorBlack);
    window_set_window_handlers(s_win, (WindowHandlers){
      .load = win_load, .unload = win_unload, .appear = win_appear,
      .disappear = win_disappear });
    window_set_click_config_provider(s_win, click_config);
  }
  // The alarm hook may fire while we're already up, or buried under a
  // picker — pushing a window twice onto the stack helps no one.
  if (!window_stack_contains_window(s_win)) window_stack_push(s_win, true);
}
