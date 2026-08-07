#pragma once
#include <pebble.h>

// Platform-adaptive palette, same discipline as the Solfarer apps: black &
// white watches render everything white on black — state is carried by text
// and shape, never by color alone.
#define COL_TEA   PBL_IF_COLOR_ELSE(GColorMayGreen, GColorWhite)
#define COL_GOLD  PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite)
#define COL_DIM   PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)
#define COL_FAINT PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite)
#define COL_BAD   PBL_IF_COLOR_ELSE(GColorRed, GColorWhite)

#define IS_ROUND PBL_IF_ROUND_ELSE(true, false)
#define IS_COMPACT(b) ((b).size.h < 200)

// Every number the eye has to track is drawn in Emberline's blocky digits
// (digits.c) rather than a system font: LECO turned out proportional (its
// "1" is narrower), and a countdown that shimmies as digits change width
// isn't a countdown you trust. These are tabular by construction. Digits
// and the colon only; anything wordy stays Gothic.
void draw_blocky(GContext *ctx, const char *s, int cx, int top, int scale,
                 GColor col);                  // centered on cx, by ink
int blocky_width(const char *s, int scale);
int blocky_height(int scale);

void fmt_mmss(char *buf, size_t cap, int s);   // 150 -> "2:30"
void fmt_incr(char *buf, size_t cap, int s);   // 25 -> "+25s", -15 -> "-15s",
                                               // 90 -> "+1:30"

// The teacup, at rim-center, bowl radius r. fill_pct pours the tea (it is
// the app's progress gauge); steam >= 0 draws wisps, alternating with the
// phase's low bit for a once-a-second flicker. -1 = no steam.
void draw_teacup(GContext *ctx, GPoint rim, int r, int fill_pct, int steam);

void win_title_push(void);
void win_session_push(void);
void win_brew_push(void);
void win_options_push(void);
void win_about_push(void);

// Shared picker, two shapes: a min:sec time (custom base, one-off adjust)
// and a single signed increment in 5s steps. `done` runs after the picker
// pops, so it may push the next window itself.
typedef void (*PickerDone)(int seconds);
void win_picker_push_time(const char *title, int initial_s, PickerDone done);
void win_picker_push_incr(const char *title, int initial_s, PickerDone done);
