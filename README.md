# BrewSession

A tea timer for Pebble that knows which infusion you're on.

Set a custom steep time as fast as a stock timer app. Hit Start, pour
during the felt 5-second countdown, and when the wrist buzzes, shake it
off and drink. When you come back for round two, the app already knows
infusion 2's time — it asks for the per-infusion increment exactly once,
and only when you commit to a second steep. Negative increments welcome.

Design and requirements live in [REQUIREMENTS.md](REQUIREMENTS.md).

## Build & run

```
pebble build
pebble install --emulator emery
```

Targets basalt, chalk, diorite, emery, flint, gabbro. Touch (Pebble
Time 2) is a second way in everywhere; buttons always suffice.

## Layout

- `src/c/session.c` — the state machine; owns truth, timekeeping, the
  wakeup safety net, alarm vibes, and the recents list
- `src/c/win_brew.c` — ready / pour countdown / steeping / alarm
- `src/c/win_session.c` — resume, new custom time, recents
- `src/c/win_picker.c` — shared min:sec + increment picker
- `src/c/win_title.c`, `win_options.c`, `win_about.c` — menu, settings, about
- `src/c/touch.c` — shared touch layer (from Solfarer/Lighthaul)
