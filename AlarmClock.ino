#include "Clock.h"

/*
 * TODO stop button
 * TODO nap mode
 * TODO brightness control?
 * TODO handle DST?
 */
static Clock clock;

void setup() {
  clock.init();
}

void loop() {
  clock.run();
}
