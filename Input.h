#ifndef Input_h
#define Input_h

#include <Arduino.h>
#include "Command.h";

typedef enum {
  NOTHING,
  CLICKED,
  LONG_PRESSED
} EventType;

class Event {
  public:
    EventType type;
    uint8_t pin;
};

class Input {
  public:
    void begin(void);
    uint8_t debouncedRead(uint8_t pin);
    void update(void);
    Command getCommand();

    Event event;
    unsigned long lastEventTime = 0;

  private:
    // Debounced state
    // Use debounceRead(pin) instead of digitalRead(pin)
    // If you set a pin as an INPUT_PULLUP, you need to set the corresponding
    // lastReadState[pin] to HIGH.
    uint8_t debouncedState[20] = {};
    uint8_t lastRealState[20] = {};
    unsigned long lastDebounceTime[20] = {};
    EventType getEventTypeForPin(uint8_t pin);
};

#endif
