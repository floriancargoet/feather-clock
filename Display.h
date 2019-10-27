#ifndef Display_h
#define Display_h

#include "Adafruit_LEDBackpack.h"
#include "constants.h"

class Display : public Adafruit_7segment {
  public:
    void init();
    void printTime(uint8_t hour, uint8_t minutes);
    void printDate(uint8_t day, uint8_t month);
    void printAlarmEnabled(uint8_t number, boolean enabled);
    void printAlarmWeekEnd(uint8_t number, boolean weekend);
    void printAlarmTrack(uint8_t number, uint8_t track);
    void printErr(uint8_t errorCode);
    void setDots(uint8_t dots);
    void setBlinking(uint8_t digits);
    void flush();
  private:
    uint16_t lastDisplayBuffer[8];
    bool changed();
    uint8_t blinking = 0;
};

#endif
