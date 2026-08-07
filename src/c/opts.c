#include "opts.h"

BrewOpts g_opts;

#define KEY_OPTS 2

void opts_init(void) {
  memset(&g_opts, 0, sizeof g_opts);
  g_opts.version = OPTS_VERSION;
  g_opts.auto_open = 0;
  g_opts.pour_s = 5;
  // Append-only: an older, shorter save reads over the defaults and stops
  // where it ends.
  int n = persist_exists(KEY_OPTS) ? persist_get_size(KEY_OPTS) : 0;
  if (n > 0 && n <= (int)sizeof g_opts) {
    BrewOpts tmp = g_opts;
    persist_read_data(KEY_OPTS, &tmp, n);
    if (tmp.version == OPTS_VERSION) g_opts = tmp;
  }
}

void opts_save(void) { persist_write_data(KEY_OPTS, &g_opts, sizeof g_opts); }
