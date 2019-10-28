#include "Clock.h"

/*
 * TODO v1:
 * - nap mode
 * - week-end when triggering alarms
 * - BUG: blinking
 *
 * TODO v2:
 * - brightness control? photoresistor?
 * - handle DST?
 *
 * Edge cases:
 * - simultaneous alarms
 * - alarm track overlaps over alarm
 * - alarm track > 24h
 * - nap + alarms
 */
static Clock clock;

void setup() {
  clock.init();
}

void loop() {
  clock.run();
}
