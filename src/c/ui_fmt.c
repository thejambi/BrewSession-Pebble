#include "ui.h"

void fmt_mmss(char *buf, size_t cap, int s) {
  if (s < 0) s = 0;
  snprintf(buf, cap, "%d:%02d", s / 60, s % 60);
}

// Increments read the way a brewer says them: "+25s each", "-15s each".
// Past a minute the seconds form stops scanning, so it switches shape.
void fmt_incr(char *buf, size_t cap, int s) {
  char sign = s < 0 ? '-' : '+';
  int a = s < 0 ? -s : s;
  if (a < 60) snprintf(buf, cap, "%c%ds", sign, a);
  else snprintf(buf, cap, "%c%d:%02d", sign, a / 60, a % 60);
}
