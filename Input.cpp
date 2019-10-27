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
  bool wasPressed = !debouncedState[pin];// pullup
  bool isPressed = !debouncedRead(pin); // pullup

  if (wasPressed && !isPressed) {
    return CLICKED;
  }
  if (isPressed && (millis() - lastDebounceTime[pin] > 2000)) {
    Serial.println("long press");
  // TODO: finish long press, don't trigger click at the end
    // needs longPressing[] bools
    // trigger LONG_PRESS events with duration
  }
  return NOTHING;
}

void Input::update() {
  event.type = NOTHING;
  event.pin = -1;

  for (uint8_t pin = firstPin; pin <= lastPin; pin++) {
    EventType t = getEventTypeForPin(pin);
    if (t != NOTHING) {
      event.type = t;
      event.pin = pin;
    }
    // dismiss multiple inputs ??
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
          return STOP;
      }
    default:
      return NONE;
  }
}
