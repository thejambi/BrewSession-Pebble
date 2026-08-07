# BrewSession, the web edition

Two faces, one truth:

- **`watch.html`** — the shot-for-shot watch port: the 200x228 emery
  framebuffer on a canvas, integer-scaled square pixels, in a Time 2-ish
  body whose four buttons work. Just for fun.
- **`index.html`** (the front door, and what the PWA installs) — the same
  app designed for a phone: no bezel, no
  emulator feel. The watch's visual language (blocky digits, the filling
  teacup, tea green on black) grown into full-bleed mobile UI — the lit
  selection row becomes the primary button, the picker becomes two big
  drag wheels, and the phone chips in a wake lock at the kettle and the
  system back gesture. Same `session.js` truth and localStorage, so the
  two faces share sessions and recents.

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
