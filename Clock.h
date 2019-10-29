#ifndef Clock_h
#define Clock_h

#include "constants.h"
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FlashStorage.h>
#include <Adafruit_VS1053.h>
#include <RTClib.h>
#include "Display.h"
#include "Input.h"
#include "State.h";

class Alarm {
  public:
    bool enabled = false;
    uint8_t hour = 0;
    uint8_t minute = 0;
    bool weekend = true;
    uint8_t track = 0; // [0-8], displayed as [1-9]
};

class Settings {
  public:
    bool valid = true; // will be false if read from empty flash
    Alarm alarm1;
    Alarm alarm2;
    int volume = 70; // [0-99]
};

class Clock {
  public:
    void init();
    void run();

  private:
    // Input/output
    Display display;
    RTC_DS3231 rtc;
    Adafruit_VS1053_FilePlayer player = Adafruit_VS1053_FilePlayer(0, 0, 0, 0, 0); // reinstantiated after SD init
    Input input;

    // Time settings
    // we work on local copies when settings the time or date
    int year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    void copyTime();
    void writeTime();
    void copyDate();
    void writeDate();

    // Alarms/Settings
    Settings settings;
    uint8_t alarmTrackCount;
    bool alarm1Stopped = false;
    bool alarm2Stopped = false;
    void writeSettings();
    void applyVolume();
    void playButtonBeep();
    String getAlarmFileName(uint8_t track);
    bool checkAlarmFile(uint8_t track);
    void playAlarm(uint8_t track);
    void checkAlarm(Alarm a, State ALARM_X, bool &stoppedFlag);
    bool noInputDuringMS(unsigned long delay);

    // Nap
    TimeSpan napTS;
    DateTime napTime;
    void playNap();
    void checkNap();

    // Init
    uint8_t sdPin;
    void die(char* msg, uint8_t errCode);
    void initDisplay();
    void initRTC();
    void initSound();
    void initSD();
    void initInput();
    void initFlashSettings();

    // State management
    State state = DISPLAY_TIME;

    void render();
    State transition(State s, Command c);
    void alarmTransition();
};

#endif
