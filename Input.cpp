#include "Input.h"

const uint8_t firstPin = 15;
const uint8_t lastPin = 19;

void Input::begin() {

  for (uint8_t pin = firstPin; pin <= lastPin; pin++) {
    pinMode(pin, INPUT_PULLUP);
    lastRealState[pin] = debouncedState[pin] = HIGH;
  }
}

uint8_t Input::debouncedRead(uint8_t pin) {
  uint8_t realState = digitalRead(pin);
  // If the switch changed, due to noise or pressing:
  if (realState != lastRealState[pin]) {
    // reset the debouncing timer
    lastDebounceTime[pin] = millis(); // FIXME rollover ?
  }

  if ((millis() - lastDebounceTime[pin]) > 50) {
    // whatever the realState is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:
    debouncedState[pin] = realState;
  }

  // save the realState for next turn
  lastRealState[pin] = realState;
  return debouncedState[pin];
}

EventType Input::getEventTypeForPin(uint8_t pin) {
  bool wasLongPressed = longPressed[pin];
  bool wasPressed = !debouncedState[pin];// pullup
  bool isPressed = !debouncedRead(pin); // pullup

  if (wasPressed && !isPressed && !wasLongPressed) {
    return CLICKED;
  }
  if (wasPressed && !isPressed && wasLongPressed) {
    longPressed[pin] = false;
    return LONG_PRESS_STOP;
  }

  if (isPressed && (millis() - lastDebounceTime[pin] > LONG_PRESS_DELAY)) {
    longPressed[pin] = true;
    if (!wasLongPressed) {
      return LONG_PRESS_START;
    }
    return LONG_PRESS_HOLD;
  }
  return NOTHING;
}

void Input::update() {
  event.type = NOTHING;
  event.pin = -1;
  event.duration = 0;

  for (uint8_t pin = firstPin; pin <= lastPin; pin++) {
    EventType t = getEventTypeForPin(pin);
    if (t != NOTHING) {
      event.type = t;
      event.pin = pin;
    }
    if (t == LONG_PRESS_HOLD || t == LONG_PRESS_STOP) {
      event.duration = millis() - lastDebounceTime[pin];
    }
    // TODO: how to handle multiple inputs at the same time?
  }

  if (event.type != NOTHING) {
    lastEventTime = millis();
  }
}


Command Input::getCommand() {
  switch (event.type) {
    case CLICKED:
      switch (event.pin) {
        case 15:
          return MODE;
        case 16:
          return SET;
        case 17:
          return UP;
        case 18:
          return DOWN;
        case 19:
          return STOP_ADD_5;
      }
    case LONG_PRESS_START:
      switch (event.pin) {
        case 19:
          return NAP;
      }
    default:
      return NONE;
  }
}
