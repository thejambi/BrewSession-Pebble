#pragma once
#include <pebble.h>

// In-app options, watch-resident, no phone round-trip (Solfarer pattern).
// Deliberately tiny — everything else in the app is a decision, not a
// setting (REQUIREMENTS §5 R24).

#define OPTS_VERSION 1
typedef struct {
  uint8_t version;
  uint8_t auto_open;   // launch straight into Brew Session; Back finds the menu
  uint8_t pour_s;      // pour countdown length: 0 (off), 3, 5, 10
} BrewOpts;
extern BrewOpts g_opts;

void opts_init(void);
void opts_save(void);
