#include "Clock.h"

/*
 * TODO v1:
 * - brightness based on hour of the day
 *
 * TODO v2:
 * - brightness control? photoresistor?
 * - handle DST?
 * - > 99 min naps?
 *
 * Edge cases:
 * - simultaneous alarms
 * - alarm track overlaps next alarm
 * - alarm track > 24h
 * - nap + alarms
 * - multiple buttons at once
 */
static Clock clock;

void setup() {
  clock.init();
}

void loop() {
  clock.run();
}
