#include "ui.h"

static Window *s_win;
static Layer *s_layer;

static void draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  bool compact = IS_COMPACT(b);
  int y = IS_ROUND ? (compact ? 20 : 32) : 10;

  graphics_context_set_text_color(ctx, COL_TEA);
  graphics_draw_text(ctx, "BREWSESSION", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(0, y, b.size.w, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, COL_DIM);
  graphics_draw_text(ctx, "v0.1.0", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, y + 28, b.size.w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx,
      "A timer that knows\nwhich infusion\nyou're on.\n\nshake stops the buzz",
      fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GRect(10, y + 52, b.size.w - 20, b.size.h - y - 56),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void win_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, draw);
  layer_add_child(root, s_layer);
}
static void win_unload(Window *w) { layer_destroy(s_layer); s_layer = NULL; }

void win_about_push(void) {
  if (!s_win) {
    s_win = window_create();
    window_set_background_color(s_win, GColorBlack);
    window_set_window_handlers(s_win, (WindowHandlers){
      .load = win_load, .unload = win_unload });
  }
  window_stack_push(s_win, true);
}
