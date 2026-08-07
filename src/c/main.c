#include <pebble.h>
#include "ui.h"
#include "opts.h"
#include "session.h"

// Launch shape (§5): the title menu is always the floor of the stack, so
// Back always finds it. Only a time-sensitive session — steeping, or
// alarming — jumps straight into the brew, whether launched by hand or by
// the wakeup (R21, U5): tea that's brewing can't wait on a menu. A session
// merely resting between infusions waits on the title's Resume row; jumping
// for it too would make the auto-open setting a lie whenever a session
// exists, which is always. Auto-open skips the menu the way a kettle skips
// small talk (R3).

int main(void) {
  opts_init();
  session_init();
  session_set_alarm_hook(win_brew_push);

  win_title_push();
  if (g_session.phase == PH_STEEPING || g_session.phase == PH_ALARM) {
    win_brew_push();
  } else if (g_opts.auto_open) {
    if (session_live()) win_brew_push();
    else win_session_push();
  }

  app_event_loop();
  session_save();
}
