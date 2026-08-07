#include "session.h"

BrewSession g_session;

#define KEY_SESSION 1
#define KEY_RECENTS 3

static void (*s_listener)(void);
static void (*s_alarm_hook)(void);

// ---- recents ---------------------------------------------------------------

static struct {
  uint8_t version;
  uint8_t count;
  Recent r[RECENTS_MAX];
} s_recents;
#define RECENTS_VERSION 1

int recents_count(void) { return s_recents.count; }
Recent recents_get(int i) { return s_recents.r[i]; }

static void recents_save(void) {
  persist_write_data(KEY_RECENTS, &s_recents, sizeof s_recents);
}

// Move-to-front with a dedup that understands "+ ?": a session that later
// learns its increment updates the entry it already made, rather than
// leaving a half-known twin behind.
static void recents_touch(int base_s, int increment_s) {
  int found = -1;
  for (int i = 0; i < s_recents.count; i++) {
    Recent *r = &s_recents.r[i];
    if (r->base_s != base_s) continue;
    if (r->increment_s == increment_s ||
        r->increment_s == INCR_UNSET || increment_s == INCR_UNSET) {
      found = i;
      // Never downgrade a known increment back to unknown.
      if (increment_s == INCR_UNSET) increment_s = r->increment_s;
      break;
    }
  }
  if (found < 0) {
    if (s_recents.count < RECENTS_MAX) s_recents.count++;
    found = s_recents.count - 1;
  }
  for (int i = found; i > 0; i--) s_recents.r[i] = s_recents.r[i - 1];
  s_recents.r[0] = (Recent){ .base_s = base_s, .increment_s = increment_s };
  recents_save();
}

// ---- steep math ------------------------------------------------------------

// The one place infusion time is computed, so the floor can't drift
// between screens (R13a).
static int steep_for(int infusion, int base_s, int increment_s) {
  if (infusion > 1 && increment_s == INCR_UNSET) return -1;
  int s = base_s + (infusion - 1) * (infusion > 1 ? increment_s : 0);
  if (s < STEEP_MIN_S) s = STEEP_MIN_S;
  if (s > STEEP_MAX_S) s = STEEP_MAX_S;
  return s;
}

int session_steep_s(void) {
  if (g_session.override_s > 0) return g_session.override_s;
  return steep_for(g_session.infusion, g_session.base_s, g_session.increment_s);
}

bool session_needs_increment(void) {
  return g_session.infusion > 1 && g_session.increment_s == INCR_UNSET &&
         g_session.override_s == 0;
}

int session_remaining_s(void) {
  if (g_session.phase != PH_STEEPING) return 0;
  int r = g_session.end_epoch - (int)time(NULL);
  return r > 0 ? r : 0;
}

// ---- alarm side effects ----------------------------------------------------
// Vibes and the shake-to-dismiss live with the state, not the window, so the
// wrist buzzes even if the alarm fired while the brewer was elsewhere in the
// app. Escalating stages, looped, instant to silence (§8).

static AppTimer *s_alarm_timer;
static time_t s_alarm_started;

static void alarm_buzz(void *ctx) {
  int elapsed = time(NULL) - s_alarm_started;
  if (g_session.phase != PH_ALARM || elapsed >= ALARM_CAP_S) {
    s_alarm_timer = NULL;
    return;   // gave up quietly; the alarm screen stays (R18)
  }
  static const uint32_t GENTLE[] = { 150, 100, 150 };
  static const uint32_t FIRM[]   = { 200, 100, 200, 100, 350 };
  static const uint32_t LOUD[]   = { 400, 150, 400, 150, 600 };
  VibePattern p;
  if (elapsed < 10)      p = (VibePattern){ GENTLE, ARRAY_LENGTH(GENTLE) };
  else if (elapsed < 25) p = (VibePattern){ FIRM, ARRAY_LENGTH(FIRM) };
  else                   p = (VibePattern){ LOUD, ARRAY_LENGTH(LOUD) };
  vibes_enqueue_custom_pattern(p);
  s_alarm_timer = app_timer_register(1600, alarm_buzz, NULL);
}

static void on_shake(AccelAxisType axis, int32_t dir) {
  if (g_session.phase == PH_ALARM) session_dismiss();
}

static void alarm_begin(void) {
  g_session.phase = PH_ALARM;
  g_session.end_epoch = 0;
  session_save();
  s_alarm_started = time(NULL);
  accel_tap_service_subscribe(on_shake);
  light_enable_interaction();
  alarm_buzz(NULL);
  if (s_alarm_hook) s_alarm_hook();
}

static void alarm_end(void) {
  accel_tap_service_unsubscribe();
  if (s_alarm_timer) { app_timer_cancel(s_alarm_timer); s_alarm_timer = NULL; }
  vibes_cancel();
}

// ---- heartbeat -------------------------------------------------------------

static void heartbeat(struct tm *t, TimeUnits u) {
  if (g_session.phase == PH_STEEPING &&
      (int)time(NULL) >= g_session.end_epoch) {
    alarm_begin();
  }
  if (s_listener) s_listener();
}

static void heartbeat_sync(void) {
  if (g_session.phase == PH_STEEPING || g_session.phase == PH_ALARM) {
    tick_timer_service_subscribe(SECOND_UNIT, heartbeat);
  } else {
    tick_timer_service_unsubscribe();
  }
}

void session_set_listener(void (*cb)(void)) { s_listener = cb; }
void session_set_alarm_hook(void (*hook)(void)) { s_alarm_hook = hook; }

// ---- wakeup safety net (R21) -----------------------------------------------
// Watchapps die when closed; the scheduled wakeup is what makes the alarm
// survive that. Only ever one, so cancel-all is exact.

static void wakeup_set(void) {
  wakeup_cancel_all();
  if (g_session.phase != PH_STEEPING) return;
  WakeupId id = wakeup_schedule(g_session.end_epoch, 0, true);
  // A conflict with another app's wakeup answers E_RANGE; a few seconds
  // late still beats never.
  if (id < 0) wakeup_schedule(g_session.end_epoch + 3, 0, true);
}

// ---- transitions -----------------------------------------------------------

void session_save(void) {
  persist_write_data(KEY_SESSION, &g_session, sizeof g_session);
}

bool session_live(void) { return g_session.phase != PH_NONE; }

void session_new(int base_s, int increment_s) {
  wakeup_cancel_all();
  alarm_end();
  memset(&g_session, 0, sizeof g_session);
  g_session.version = 1;
  g_session.phase = PH_READY;
  g_session.infusion = 1;
  g_session.base_s = base_s;
  g_session.increment_s = increment_s;
  recents_touch(base_s, increment_s);
  session_save();
  heartbeat_sync();
}

void session_set_increment(int s) {
  g_session.increment_s = s;
  recents_touch(g_session.base_s, s);
  session_save();
}

void session_adjust_once(int s) {
  g_session.override_s = s;
  session_save();
}

void session_skip(void) {
  g_session.infusion++;
  g_session.override_s = 0;
  session_save();
}

void session_start_steep(void) {
  int steep = session_steep_s();
  if (steep < 0) return;   // "+ ?" — the picker should have run first
  g_session.phase = PH_STEEPING;
  g_session.end_epoch = time(NULL) + steep;
  session_save();
  wakeup_set();
  heartbeat_sync();
}

void session_adjust_running(int ds) {
  if (g_session.phase != PH_STEEPING) return;
  g_session.end_epoch += ds;
  int now = time(NULL);
  if (g_session.end_epoch <= now) g_session.end_epoch = now + 1;
  session_save();
  wakeup_set();
}

void session_dismiss(void) {
  alarm_end();
  wakeup_cancel_all();
  g_session.phase = PH_READY;   // dismiss and advance are one gesture (R20)
  g_session.infusion++;
  g_session.override_s = 0;
  g_session.end_epoch = 0;
  session_save();
  heartbeat_sync();
  if (s_listener) s_listener();
}

void session_abandon(void) {
  alarm_end();
  wakeup_cancel_all();
  memset(&g_session, 0, sizeof g_session);
  session_save();
  heartbeat_sync();
}

void session_init(void) {
  memset(&s_recents, 0, sizeof s_recents);
  s_recents.version = RECENTS_VERSION;
  int n = persist_exists(KEY_RECENTS) ? persist_get_size(KEY_RECENTS) : 0;
  if (n > 0 && n <= (int)sizeof s_recents) {
    persist_read_data(KEY_RECENTS, &s_recents, n);
    if (s_recents.version != RECENTS_VERSION || s_recents.count > RECENTS_MAX) {
      memset(&s_recents, 0, sizeof s_recents);
      s_recents.version = RECENTS_VERSION;
    }
  }

  memset(&g_session, 0, sizeof g_session);
  n = persist_exists(KEY_SESSION) ? persist_get_size(KEY_SESSION) : 0;
  if (n > 0 && n <= (int)sizeof g_session) {
    persist_read_data(KEY_SESSION, &g_session, n);
    if (g_session.version != 1) memset(&g_session, 0, sizeof g_session);
  }

  // Reconcile with the wall clock: time passed while we were closed. A steep
  // that ended in our absence is an alarm now, whether we were relaunched by
  // the wakeup or by a hand reaching for the app (R12, U5). alarm_begin runs
  // before main pushes windows, so the hook fires into nothing — main pushes
  // win_brew itself when the session is live.
  if (g_session.phase == PH_STEEPING &&
      (int)time(NULL) >= g_session.end_epoch) {
    alarm_begin();
  } else if (g_session.phase == PH_ALARM) {
    // Persisted mid-alarm, then the app died: ring again, briefly.
    alarm_begin();
  }
  heartbeat_sync();
}
