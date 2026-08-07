#include <pebble.h>
#include "ui.h"
#include "opts.h"
#include "session.h"

// Launch shape (§5): the title menu is always the floor of the stack, so
// Back always finds it. A live session lands you straight back in the brew
// — whether you left on purpose, launched by hand, or the wakeup pulled the
// app up because a steep finished while it was closed (R21, U5). Auto-open
// skips the menu the way a kettle skips small talk (R3).

int main(void) {
  opts_init();
  session_init();
  session_set_alarm_hook(win_brew_push);

  win_title_push();
  if (session_live()) {
    win_brew_push();
  } else if (g_opts.auto_open) {
    win_session_push();
  }

  app_event_loop();
  session_save();
}
