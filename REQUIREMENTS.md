# BrewSession — Requirements & Design

A minimal tea timer watchapp for Pebble. Target hardware: Pebble Time 2
(with touch), but built like the other apps in this family — every button
keeps working everywhere, touch is only ever a second way in.

## 1. Philosophy

Existing tea apps over-structure the problem. They front-load decisions
(pick a tea type, pick which infusion you're on) that the brewer either
doesn't care about or shouldn't have to answer *yet*. The result is that
a plain countdown timer app beats them on usability.

BrewSession's bet: the winning move is not more tea knowledge, it's less
friction. Three principles:

1. **Timer-first.** The core object is a custom time, set in seconds, as
   fast as a stock timer app — everything else is layered gently on top.
2. **Ask when needed, not before.** The app never asks a question whose
   answer isn't needed yet. The infusion increment isn't requested until
   infusion 1 is done and infusion 2 is actually being considered.
3. **The app tracks state so the user doesn't.** Which infusion you're
   on, what the next steep time is — that's bookkeeping, and bookkeeping
   is the app's job. The user never selects "Infusion 3" from a menu.

## 2. Definitions

- **Steep time** — the countdown for one infusion.
- **Increment** — seconds added to the steep time for each subsequent
  infusion (multiples of 5s).
- **Session** — a base steep time plus an increment, plus the live state
  of a brewing run: which infusion is current and what its time is.
  A session is what gets remembered as a "recent."
- **Infusion** — one steep within a session, numbered from 1.
- **Pour countdown** — a short lead-in (default 5s) between pressing
  Start and the timer actually starting, so the button can be pressed
  right before the pour begins rather than before or after it.

## 3. Platform & constraints

- Pebble SDK 3, C watchapp (`"watchface": false`), targets: basalt,
  chalk, diorite, emery, flint, gabbro — same set as Solfarer/Lighthaul.
- Primary target: Pebble Time 2 (200x228 color e-paper, 4 buttons,
  touchscreen). Layouts must still work on the smaller/round platforms.
- Touch via the shared `touch.c` pattern (tap + vertical swipe),
  subscribed only where the hardware reports it. Buttons are always
  sufficient on their own.
- No phone companion for v1. All settings live on the watch
  (Solfarer `opts.c` pattern).
- Watchapps die when closed — a timer that silently dies with the app is
  a broken kettle. See §8: the Wakeup API is the safety net.

## 4. User stories

**U1 — First brew, custom time (the canonical story).**
User opens app, picks Brew Session → New Custom Time, dials 2:30, hits
Start. Screen: "Pour now… starting in 5… 4…" then the timer runs. At
0:00 the wrist buzzes; user shakes wrist (or presses any button) to
dismiss. Enjoys tea.

**U2 — Second infusion.**
User returns, ready for round two. The ready screen shows "Infusion 2 —
2:30 + ?". They hit Start — and *because* they've now said they want a
second infusion, the app asks for the increment: a picker in 5s steps.
User selects 25s, and it rolls straight into the pour countdown for
2:55. Steep, buzz, shake, tea. (Had they been done for the day, no
question would ever have been asked.)

**U3 — Repeat a recent session.**
Next day, same tea. User opens app; the previous session (2:30 + 25s) is
the top item in Brew Session. One press and they're at "Infusion 1 —
2:30 — Start."

**U4 — Skip an infusion.**
On the "Infusion 3 ready" screen the user decides to jump ahead (maybe
they want doubled increment for a long-neglected pot). Skip advances to
"Infusion 4" with another increment applied. Skips can repeat.

**U5 — Accidental exit / glance elsewhere mid-steep.**
User backs out of the app or checks a notification while steeping. The
alarm still fires at the right moment (wakeup relaunches the app into
the alarm screen). Reopening the app mid-steep resumes the countdown
exactly where it truly is.

**U6 — Straight to business.**
User enables the "open to Brew Session" setting. Launching the app skips
the title menu entirely; Back from Brew Session still reaches the menu.

## 5. Functional requirements

### Launch & menu
- R1. App opens to a title/menu screen: **Brew Session** first, then
  **Settings**, **About**.
- R2. If a session is live (steeping, alarming, or between infusions),
  the menu's first item becomes **Resume Session** (with a one-line
  status, e.g. "Infusion 2 — 1:04 left"), and a **New Session** item
  follows it — switching teas mid-session must not require abandoning
  from inside the brew screen. Starting a new session ends the old one
  (R17).
- R3. Setting: **auto-open to Brew Session** — skip the menu on launch;
  Back navigates to the menu rather than exiting.
- R3a. Independent of that setting, a launch lands directly in the brew
  screen only while the session is time-sensitive (steeping or
  alarming). A session resting between infusions waits on the menu's
  Resume row — otherwise auto-open OFF would be a dead letter from the
  first brew onward, since a session exists almost always (R17).

### Starting a session
- R4. Brew Session screen lists, in order: Resume (if live), **New
  Custom Time**, then recent sessions, most recent first.
- R5. Recents show base time + increment at a glance ("2:30 +25s").
  Up to 6 kept, most-recently-*used* first, deduplicated (same base +
  increment = same session, reordered to top on use).
- R6. New Custom Time: a min:sec picker, seconds in 5s steps, buttons
  and touch-swipe both scroll it. Range 0:10–20:00. Starting a custom
  session does *not* ask for an increment (see R13).

### Pour countdown
- R7. Pressing Start shows "Pour now" and counts down (default 5s),
  then starts the steep timer. Rationale: the button gets pressed *as*
  the pour begins, not before or after.
- R7a. The pour countdown is *felt*, not just seen: a tiny vibe pulse
  each second and a distinct double-pulse at "go," so the wrist carries
  the count through the steam without being watched.
- R8. During pour countdown: Select starts the steep immediately
  (skips the rest), Back cancels back to the ready screen.
- R9. Setting: pour countdown length — Off / 3s / 5s / 10s (default 5s).
  Off means Start begins the steep instantly.

### Steeping
- R10. Steep screen shows: big remaining time, infusion number, and a
  progress indication. Updates every second.
- R11. Up/Down adjust the running timer ±5s (hold to repeat). A long
  **Select** abandons the session (with confirm — long Back is the
  system's own app-exit and can't be borrowed); short Back just leaves
  the app, under the background contract of R21/R22. There is no pause
  — you can't pause leaves, and live adjust covers the real case
  ("phone rang, give it 15 more").
- R12. Timekeeping is wall-clock based (a stored end timestamp, not a
  decrementing counter), so remaining time is always true regardless of
  redraw hiccups or app relaunch.

### Between infusions
- R13. The increment is asked for only when the user *commits* to a
  second infusion: on a custom session with no increment yet, the
  infusion 2 ready screen shows its time as "2:30 + ?", and pressing
  Start opens the increment picker first, then proceeds straight into
  the pour countdown. If they never brew a second infusion, they're
  never asked. Asked once per session; the answer becomes part of the
  session (and its recent).
- R13a. The increment picker runs in 5s steps from **−1:00 to +2:00**,
  touch-scrollable, defaulting to 0:00 at the center. Negative
  increments are first-class — some brewing styles steep *shorter* as
  they go, and the brewers who hope for that support should find it
  waiting for them. Computed steep times floor at 0:10 (R6's minimum);
  the ready screen shows the clamped value.
- R14. The ready screen for infusion N shows "Infusion N" and its
  computed time (base + (N−1) × increment, floored per R13a). Controls:
  **Select = Start**, **Down = Skip**, **Up = adjust this time**.
  Touch: tap Start, swipe to skip/adjust.
- R15. Skip advances the infusion counter and applies another increment,
  repeatably (U4). No confirmation and no skip-back in v1 — the
  infusion number is informational; the timer is still right.
- R16. Up (adjust) opens the same min:sec picker preloaded with the
  computed time; the adjustment applies to this infusion only and does
  not rewrite the session's base or increment. On a "+ ?" screen this
  works too — a one-off time for infusion 2 without ever setting an
  increment; the question then simply waits for the next unadjusted
  Start.
- R17. Infusions have no upper limit. The session ends only when the
  user abandons it or starts a different one.

### Alarm
- R18. At 0:00 the watch vibrates in an escalating pattern until
  dismissed or 45s elapse (then it gives up quietly but stays on the
  alarm screen).
- R19. Dismissal: shake/flick the wrist, tap the screen, or any button.
- R20. Dismissing the alarm advances directly to the next infusion's
  ready screen — "dismiss" and "advance" are one gesture, no extra
  press. The ready screen is passive: it asks nothing (any pending
  increment question waits for Start — R13), so a brewer who's done
  for the day can simply walk away.

### Reliability & persistence
- R21. When a steep starts, a system wakeup is scheduled for the end
  time. If the app is closed when the steep ends, the OS relaunches it
  straight into the alarm screen. The wakeup is cancelled on dismissal
  or abandon, and rescheduled on a live ±5s adjust.
- R22. Session state (base, increment, infusion number, end timestamp,
  phase) is persisted on every transition. Killing and reopening the
  app mid-anything resumes correctly.
- R23. Recents and settings persist under separate storage keys (the
  Solfarer opts/journey split — neither can clobber the other).

### Settings
- R24. Settings, kept deliberately tiny: auto-open to Brew Session
  (R3), pour countdown length (R9), vibe pattern (short / double /
  long). Everything else is a decision, not a setting.

## 6. Non-goals (v1)

- No tea database, no named tea types, no suggested steep times. Recents
  *are* the presets.
- No naming of sessions (v1.1 candidate: long-press a recent to name it).
- No voice input.
- No brew history/statistics.
- No phone companion or config page.
- No water temperature anything.

## 7. Design — screens & flow

```
                 ┌─────────────┐
                 │ Title menu  │  Brew Session / Settings / About
                 └──────┬──────┘  (skipped when auto-open is on)
                        │
                 ┌──────▼──────┐
                 │ Brew Session│  Resume? / New Custom Time / recents…
                 └──────┬──────┘
            new custom  │   recent picked
              ┌─────────▼─────────┐
              │  Time picker      │  (custom only)
              └─────────┬─────────┘
                        │
              ┌─────────▼─────────┐   Up: adjust once (R16)
        ┌────►│ Infusion N ready  │◄──────────────┐
        │     │  "2:55"  [Start]  │── Down: skip ─┤ (N+1, +increment)
        │     └─────────┬─────────┘               │
        │          Select: start                  │
        │      increment unknown? ──► increment picker (once, R13)
        │               │                          then straight on ↓
        │     ┌─────────▼─────────┐
        │     │ Pour countdown    │  "Pour now… 5… 4…"  felt ticks (R7a)
        │     └─────────┬─────────┘  Select: start now, Back: cancel
        │               │
        │     ┌─────────▼─────────┐
        │     │ Steeping  2:29    │  Up/Down ±5s
        │     └─────────┬─────────┘  (wakeup scheduled — R21)
        │               │ 0:00
        │     ┌─────────▼─────────┐
        │     │ ALARM  bzzz       │  shake / tap / any button
        │     └─────────┬─────────┘
        │               │ dismissed
        └───────────────┘
              next infusion ready (passive — R20)
```

Screen notes:

- **Ready screen** is the session's home base. Big time, "Infusion N"
  above it, an action-bar-style hint of Start/Skip/Adjust. This is also
  what Resume lands on when between infusions.
- **Steeping screen** favors the countdown: it should be readable at
  arm's length through steam. Infusion number small, remaining time
  huge, thin progress ring or bar.
- **Alarm screen** says which infusion just finished ("Infusion 2
  done") so a glance after a distracted minute still makes sense.
- Pickers are shared: one min:sec picker component used for custom
  time, per-infusion adjust, and (mm omitted) the increment picker.

## 8. Technical design

Structure follows the Solfarer/Lighthaul house style:

```
src/c/
  main.c          app init, wakeup-launch dispatch, window plumbing
  session.c/.h    the state machine: phases, infusion math, timestamps,
                  persistence, wakeup scheduling  (no UI)
  win_title.c     title menu
  win_session.c   Brew Session list (resume / new / recents)
  win_picker.c    shared min:sec picker (custom, adjust, increment modes)
  win_brew.c      ready + pour countdown + steeping + alarm (one window,
                  four phases — they share layout bones and transitions)
  opts.c/.h       settings, persisted (Solfarer pattern, versioned struct)
  touch.c/.h      shared touch layer, reused as-is
```

Key decisions:

- **`session.c` owns truth; windows render it.** The session struct:
  base_s, increment_s (int16_t — negative is legal; INT16_MIN = not yet
  asked), infusion_n, phase (READY/POURING/STEEPING/ALARM), end_epoch.
  Steep-time math lives here too: `base + (n−1) × increment`, floored
  at 10s, in exactly one function. Persisted whole under its own key,
  version-prefixed.
- **Wall-clock timekeeping (R12):** on steep start, `end_epoch = now +
  steep_s`. Rendering computes `end_epoch − now`; a tick handler only
  triggers redraws.
- **Wakeup as safety net (R21):** `wakeup_schedule(end_epoch, …)` on
  steep start. On launch, `launch_reason() == APP_LAUNCH_WAKEUP` →
  restore session, jump to alarm. Normal launch with a persisted live
  session → jump to wherever it left off (recomputing STEEPING vs ALARM
  from end_epoch, since time passed while closed).
- **Alarm dismissal (R19):** `accel_tap_service` for the wrist
  shake/flick, click handlers for buttons, `touch.c` tap for the
  screen. All three route to the same `session_dismiss()`.
- **Vibes:** two custom patterns. The alarm is an escalating
  `VibePattern` looped by an app timer until dismissed or the 45s cap
  (R18) — not a single `vibes_*` call, so dismissal is instant. The
  pour countdown (R7a) is a short pulse per second and a double-pulse
  at "go," driven by the same countdown timer that redraws the digits.
- **Recents:** fixed array of 6 (base_s, increment_s) pairs persisted
  under their own key; move-to-front on use, drop duplicates.

## 9. Settled questions

Decisions made during review, kept here so the reasoning survives:

1. **No skip-back in v1.** If the infusion count drifts from reality,
   shrug — the number is informational and the timer is still right.
2. **Negative increments are in (R13a).** Some brewing styles steep
   shorter as they go; supporting that is a small love letter to the
   brewers who'd hope for it. Floor: computed steep ≥ 0:10.
3. **No pause.** You can't pause leaves. Up/Down live adjust covers the
   real interruptions.
4. **The pour countdown is felt (R7a).** A pulse per second and a
   double-pulse at "go" — the wrist carries the count through steam.
5. **The increment ask waits for commitment (R13).** Dismissing
   infusion 1 lands on a passive ready screen; the question comes only
   when Start says "yes, infusion 2." A brewer who's done for the day
   is never interrogated on the way out.

## 10. Phasing

- **v1 (MVP):** everything in §5 except vibe-pattern setting; single
  hardcoded escalating pattern. Emery/basalt layouts first.
- **v1.1:** vibe patterns, named recents (long-press to name), round
  (chalk) layout polish.
- **Someday/maybe:** brew history, phone config page, temperature notes.
