#pragma once
#include <pebble.h>

// The session state machine. This file owns truth; windows render it.
// (REQUIREMENTS §8.) Pouring is deliberately absent: a pour countdown you
// walked away from is a pour that didn't happen, so only READY, STEEPING
// and ALARM are worth persisting.

typedef enum { PH_NONE = 0, PH_READY, PH_STEEPING, PH_ALARM } BrewPhase;

#define INCR_UNSET   INT16_MIN   // increment not asked for yet (R13)
#define STEEP_MIN_S  10          // computed steep times floor here (R13a)
#define STEEP_MAX_S  (20 * 60)
#define INCR_MIN_S   (-60)       // negative increments are first-class (R13a)
#define INCR_MAX_S   120
#define ALARM_CAP_S  45          // then the buzz gives up quietly (R18)

typedef struct {
  uint8_t version;
  uint8_t phase;         // BrewPhase
  uint16_t infusion;     // 1-based
  int16_t base_s;
  int16_t increment_s;   // INCR_UNSET until the brewer commits to round two
  int16_t override_s;    // one-off time for this infusion only (R16); 0 = none
  int32_t end_epoch;     // when the current steep ends; 0 outside STEEPING
} BrewSession;
extern BrewSession g_session;

void session_init(void);   // load, then reconcile with the wall clock
void session_save(void);

bool session_live(void);
void session_new(int base_s, int increment_s);   // INCR_UNSET for a custom
bool session_needs_increment(void);              // "+ ?" on the ready screen
void session_set_increment(int s);
int  session_steep_s(void);        // this infusion's time, clamped; -1 = "+ ?"
void session_adjust_once(int s);   // R16: this infusion only
void session_skip(void);           // R15: N+1, another increment applied
void session_start_steep(void);    // READY -> STEEPING, wakeup scheduled
void session_adjust_running(int ds);   // ±5s mid-steep, wakeup rescheduled
int  session_remaining_s(void);
void session_dismiss(void);        // ALARM -> next infusion READY, one gesture
void session_abandon(void);

// The per-second heartbeat lives here, not in a window, so a steep that ends
// while the brewer is on the title screen still alarms. The visible window
// registers a listener for redraws; main.c hooks alarm onto win_brew_push.
void session_set_listener(void (*cb)(void));
void session_set_alarm_hook(void (*hook)(void));

// Recents are the presets (§6): last sessions, most-recently-used first.
#define RECENTS_MAX 6
typedef struct { int16_t base_s; int16_t increment_s; } Recent;
int    recents_count(void);
Recent recents_get(int i);
