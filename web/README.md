# BrewSession, the web edition

A shot-for-shot PWA port of the Pebble watchapp, down to the look: the
200x228 emery framebuffer drawn on a canvas, integer-scaled with square
pixels, sitting in a Time 2-ish body whose four buttons work. Just for fun.

The C maps across almost file for file:

| watch              | web               |
|--------------------|-------------------|
| `digits.c`         | `js/digits.js` (same bitmasks) |
| `ui_cup.c`         | `js/cup.js`       |
| `session.c`        | `js/session.js`   |
| `win_*.c`          | `js/windows.js`   |
| `touch.c` + buttons| `js/main.js`      |

Same state machine, same layouts, same recents dedup, same "+ ?" moment.
localStorage stands in for the persist keys; `navigator.vibrate` plus a
small square-wave beep stand in for the vibe motor; keyboard (arrows /
enter / esc) and the bezel buttons stand in for the physical buttons; and
pointer gestures are the Time 2 touch layer with the same thresholds —
the picker wheel drags at one step per 32px, exactly like the watch.

## Run it

Any static server over this directory:

```
python3 -m http.server 8642 -d web
```

Then http://localhost:8642. Installable as a PWA; the service worker
keeps it working offline (network-first, so updates land on next visit).

## The honest limitation

The watch schedules a system wakeup, so its alarm survives the app being
closed. A web page cannot: if the tab is alive (even backgrounded) the
alarm fires on time and posts a Notification; a closed tab is a silent
kettle. Reopening reconciles against the wall clock immediately — same as
the watch's relaunch path — so the state is never wrong, just quieter
than a wrist. The watch keeps that superpower; this is the desk toy.
