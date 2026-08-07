#pragma once
#include <pebble.h>

// Touch, on the hardware that has it — which the watch is asked at runtime
// rather than assumed here. The code compiles wherever the API exists and
// then subscribes only if touch_service_is_enabled() agrees, so a panel the
// platform config doesn't know about still works, and a watch without one
// simply never subscribes. Every button keeps doing exactly what it did:
// touch is only ever a second way in, never the only one.
//
// (Watchfaces need not apply — the platform doesn't deliver touch to them.)
//
// The service hands over raw touchdown / position / liftoff, not gestures, so
// the two this app needs are resolved here, both judged from where the finger
// landed and where it left:
//
//   tap    down and up within a thumb's width, reported at the landing point,
//          which is where the eye was aimed;
//   swipe  a mostly-vertical drag, reported as -1 (up) or +1 (down) and wired
//          to whatever Up and Down already do.

typedef void (*TouchTapHandler)(GPoint p);
typedef void (*TouchSwipeHandler)(int dir);   // -1 = up, +1 = down
typedef void (*TouchDragHandler)(int dy);     // incremental px, + = finger down

void touch_begin(TouchTapHandler on_tap);     // taps only
void touch_begin_full(TouchTapHandler on_tap, TouchSwipeHandler on_swipe);
// The third way in, for wheels: the drag reports pixel deltas as the finger
// moves, so a value can travel several steps in one stroke instead of one
// per swipe. A stroke short enough to be a tap is still a tap; a flick fast
// enough to produce no position updates lands as one big delta at liftoff.
void touch_begin_drag(TouchTapHandler on_tap, TouchDragHandler on_drag);
void touch_end(void);                         // from a window's .disappear
bool touch_available(void);                   // hardware present and enabled
