// BrewSession's skinny variant of Emberline's blocky digits: the same
// square-cornered dialect on a 7x12 grid — strokes thinned 3 columns to 2,
// then the whole footprint pulled in a pixel each way for breathing room —
// so the countdown carries the same pen weight as the teacup's line art.
// Drawn by hand here; Emberline's tools/digitgrid.py original stays
// untouched.
#pragma once
#include <pebble.h>

#define DIGIT_W 7
#define DIGIT_H 12
#define DIGIT_COLON 10
// The colon inks only these columns, so the one-line layout can give it
// a slot of its own rather than a full digit width.
#define DIGIT_COLON_L 2
#define DIGIT_COLON_R 3
// ...and these are the columns the digits ink. This set inks its full
// grid; the macros stay so layout centers by ink, not box, and a future
// redraw with a blank edge column cannot shove the line over.
#define DIGIT_INK_L 0
#define DIGIT_INK_R 6

// One bitmask per row, bit (DIGIT_W-1) is the leftmost column.
extern const uint16_t DIGIT_ROWS[11][DIGIT_H];

// Every set pixel as a `scale` x `scale` box, top-left at (x, top),
// in the context fill color. Runs are one rectangle, not one per pixel.
void digit_draw(GContext *ctx, int idx, int x, int top, int scale);
