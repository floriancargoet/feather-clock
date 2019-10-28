#include "Clock.h"

static const uint8_t daysInMonth [] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

FlashStorage(flash_settings, Settings);

void Clock::init() {
  Serial.begin(9600);
  // while (!Serial);
  Serial.println("Boot");
  initFlashSettings();
  Serial.println("Init Flash OK");
  initInput();
  Serial.println("Init input OK");
  initDisplay();
  Serial.println("Init display OK");
  initRTC();
  Serial.println("Init RTC OK");
  initSD();
  Serial.println("Init SD OK");
  initSound();
  Serial.println("Init Sound OK");
  Serial.println("Full init OK");
}

void Clock::initDisplay() {
  display.begin(0x70); // Code hangs here after a reset. The Display is not resetted correctly
  display.setBrightness(0);
}

void Clock::initRTC() {
   if (!rtc.begin()) {
     Serial.println("Failed to init RTC");
     display.printErr(1);
     display.flush();
     while (1);
   }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void Clock::initFlashSettings() {
  settings = flash_settings.read();

  if (!settings.valid) {
    // settings have never been written to flash, initialize settings
    settings = Settings();
    flash_settings.write(settings);
  }
}

void Clock::initSound() {
  if (! player.begin()) {
     Serial.println("Failed to init player");
     display.printErr(2);
     display.flush();
     while (1);
  }

  // If DREQ is on an interrupt pin we can do background audio playing
  player.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

  applyVolume();
  player.startPlayingFile(TRACK_BOOT);
}

void Clock::initSD() {
  if (!SD.begin(CARDCS)) {
     Serial.println("Failed to init SD card");
    display.printErr(3);
    display.flush();
    while (1);
  }
  // Check files existence
  if (!SD.exists(TRACK_BUTTON_PRESS)) {
     Serial.println("Couldn't find " TRACK_BUTTON_PRESS);
  }
  // Check consecutive alarm track files
  uint8_t i = 0;
  while (checkAlarmFile(i) && i < 8) i++;
  alarmTrackCount = i;
  Serial.println(alarmTrackCount);
}

void Clock::initInput() {
  input.begin();
}

void Clock::applyVolume() {
  // convert [0-99] to [0x80 - 0x00]
  // 0xFE = theoretical min
  // 0x80 = actual usable min
  uint8_t volume = 0x80 - ((uint8_t) (settings.volume * 1.3));
  player.setVolume(volume, volume);
}

void Clock::playButtonBeep() {
  if (player.stopped()) {
    player.startPlayingFile(TRACK_BUTTON_PRESS);
  }
}

String Clock::getAlarmFileName(uint8_t track) {
  char filename[] = TRACK_ALARM_PATTERN;
  char *c = strchr(filename, '?');
  if (c) {
    *c = '1' + track;
  }
  return String(filename);
}

void Clock::playAlarm(uint8_t track) {
  String file = getAlarmFileName(track);
  player.stopPlaying();
  player.startPlayingFile(file.c_str());
}

bool Clock::checkAlarmFile(uint8_t track) {
  return SD.exists(getAlarmFileName(track));
}

void Clock::run() {
  input.update();
  Command c = input.getCommand();
  alarmTransition(); // pre-emptive state change
  state = transition(state, c);
  if (c != NONE) {
    playButtonBeep();
  }
  render();
}

void Clock::alarmTransition() {
  checkAlarm(settings.alarm1, ALARM_1, alarm1Stopped);
  checkAlarm(settings.alarm2, ALARM_2, alarm2Stopped);
}

void Clock::checkAlarm(Alarm a, State ALARM_X, bool &stoppedFlag) {
  // When reaching the time of an alarm, we emit the ALARM_X command. When the
  // alarm is stopped by the user, if it's still the same time, we mustn't play
  // it again. We set an alarmXStopped flag, which will be removed 1 min before
  // ringing the next day.
  DateTime now = rtc.now();
  uint8_t hour = now.hour();
  uint8_t minute = now.minute();
  // If the song stopped itself, set the flag
  if (state == ALARM_X && !stoppedFlag && player.stopped()) {
    Serial.println("Track ended, stopping alarm");
    stoppedFlag = true;
    state = DISPLAY_TIME;
  }
  if (
    stoppedFlag &&
    a.hour * 60 + a.minute == hour * 60 + minute + 1
  ) {
    stoppedFlag = false;
  }
  if (
    a.enabled && // TODO: week-end check
    !stoppedFlag &&
    state != ALARM_X &&
    a.hour == hour &&
    a.minute == minute
  ) {
    state = ALARM_X;
    Serial.println("Starting alarm");
    playAlarm(a.track);
  }
}

// copy time locally when editing it so that the RTC doesn't modify it too
void Clock::copyTime() {
  DateTime now = rtc.now();
  year = now.year() - 2000; // keep between [0-99], easier for modulo
  month = now.month() - 1; // keep between [0-11], easier for modulo
  day = now.day() - 1; // keep between [0-30], easier for modulo
  hour = now.hour();
  minute = now.minute();
  second = now.second();
}

void Clock::writeTime() {
// correct day/month offset, year is ok (supported by lib)
  rtc.adjust(DateTime(year, month + 1, day + 1, hour, minute, second));
}

// copy date locally when editing it so that the RTC doesn't modify it too
// we don't copy time so that it's not modified when editing only the date
void Clock::copyDate() {
  DateTime now = rtc.now();
  year = now.year() - 2000; // keep between [0-99], easier for modulo
  month = now.month() - 1; // keep between [0-11], easier for modulo
  day = now.day() - 1; // keep between [0-30], easier for modulo
}

void Clock::writeDate() {
  // we check if the date if possible (day of month + leap year)
  // if not, we change the day
  uint8_t daysInCurrentMonth = daysInMonth[month];
  if ((year + 2000) % 4 == 0 && month == 1) { // enough for [2000-2099] (2000 is divisible by 400)
    daysInCurrentMonth++;
  }
  day = min(day + 1, daysInCurrentMonth) - 1; // add & subtract 1 because we shifted it to [0, 30] before

  DateTime now = rtc.now();
  // correct day/month offset, year is ok (supported by lib)
  rtc.adjust(DateTime(year, month + 1, day + 1, now.hour(), now.minute(), now.second()));
}

void Clock::writeSettings() {
  flash_settings.write(settings);
}

void Clock::render() {
  DateTime now = rtc.now();
  // reset the display
  display.setBlinking(0);
  display.clear();

  switch (state) {
    // Display modes
    case DISPLAY_VOLUME:
      display.print(settings.volume, DEC);
      break;
    case DISPLAY_TIME:
      display.setDots(
        (settings.alarm1.enabled ? LEFT_COLON_UPPER : 0) |
        (settings.alarm2.enabled ? LEFT_COLON_LOWER : 0)
      );
      display.printTime(now.hour(), now.minute());
      break;
    case DISPLAY_DATE:
      display.printDate(now.day(), now.month());
      break;
    case DISPLAY_ALARM_1:
      display.setDots(LEFT_COLON_UPPER);
      display.printAlarmEnabled(1, settings.alarm1.enabled);
      break;
    case DISPLAY_ALARM_2:
      display.setDots(LEFT_COLON_LOWER);
      display.printAlarmEnabled(2, settings.alarm2.enabled);
      break;

    // Set time
    case SET_HOURS:
      display.setBlinking(BLINK_DIGIT_1 | BLINK_DIGIT_2);
      display.printTime(hour, minute);
      break;
    case SET_MINUTES:
      display.setBlinking(BLINK_DIGIT_3 | BLINK_DIGIT_4);
      display.printTime(hour, minute);
      break;

    // Set date
    case SET_DAY:
      display.setBlinking(BLINK_DIGIT_1 | BLINK_DIGIT_2);
      display.printDate((day + 1), (month + 1));
      break;
    case SET_MONTH:
      display.setBlinking(BLINK_DIGIT_3 | BLINK_DIGIT_4);
      display.printDate((day + 1), (month + 1));
      break;
    case SET_YEAR:
      display.setBlinking(BLINK_DIGIT_1 | BLINK_DIGIT_2 | BLINK_DIGIT_3 | BLINK_DIGIT_4);
      display.print(year + 2000);
      break;

    // Set alarm 1
    case SET_ENABLED_1:
      display.setDots(LEFT_COLON_UPPER);
      display.setBlinking(BLINK_DIGIT_3 | BLINK_DIGIT_4);
      display.printAlarmEnabled(1, settings.alarm1.enabled);
      break;
    case SET_HOURS_1:
      display.setDots(LEFT_COLON_UPPER);
      display.setBlinking(BLINK_DIGIT_1 | BLINK_DIGIT_2);
      display.printTime(settings.alarm1.hour, settings.alarm1.minute);
      break;
    case SET_MINUTES_1:
      display.setDots(LEFT_COLON_UPPER);
      display.setBlinking(BLINK_DIGIT_3 | BLINK_DIGIT_4);
      display.printTime(settings.alarm1.hour, settings.alarm1.minute);
      break;
    case SET_WEEKEND_1:
      display.setDots(LEFT_COLON_UPPER);
      display.setBlinking(BLINK_DIGIT_3 | BLINK_DIGIT_4);
      display.printAlarmWeekEnd(1, settings.alarm1.weekend);
      break;
    case SET_TRACK_1:
      display.setDots(LEFT_COLON_UPPER);
      display.setBlinking(BLINK_DIGIT_4);
      display.printAlarmTrack(1, settings.alarm1.track + 1);
      break;

    // Set alarm 2
    case SET_ENABLED_2:
      display.setDots(LEFT_COLON_UPPER);
      display.setBlinking(BLINK_DIGIT_3 | BLINK_DIGIT_4);
      display.printAlarmEnabled(2, settings.alarm2.enabled);
      break;
    case SET_HOURS_2:
      display.setDots(LEFT_COLON_UPPER);
      display.setBlinking(BLINK_DIGIT_1 | BLINK_DIGIT_2);
      display.printTime(settings.alarm2.hour, settings.alarm2.minute);
      break;
    case SET_MINUTES_2:
      display.setDots(LEFT_COLON_UPPER);
      display.setBlinking(BLINK_DIGIT_3 | BLINK_DIGIT_4);
      display.printTime(settings.alarm2.hour, settings.alarm2.minute);
      break;
    case SET_WEEKEND_2:
      display.setDots(LEFT_COLON_UPPER);
      display.setBlinking(BLINK_DIGIT_3 | BLINK_DIGIT_4);
      display.printAlarmWeekEnd(2, settings.alarm2.weekend);
      break;
    case SET_TRACK_2:
      display.setDots(LEFT_COLON_UPPER);
      display.setBlinking(BLINK_DIGIT_4);
      display.printAlarmTrack(2, settings.alarm2.track + 1);
      break;

    // Ringing alarms
    case ALARM_1:
      display.setBlinking(BLINK_DOTS);
      display.setDots(LEFT_COLON_UPPER);
      display.printTime(now.hour(), now.minute());
      break;
    case ALARM_2:
      display.setBlinking(BLINK_DOTS);
      display.setDots(LEFT_COLON_LOWER);
      display.printTime(now.hour(), now.minute());
      break;
  }

  display.flush();
}

bool Clock::noInputDuringMS(unsigned long delay) {
  return millis() - input.lastEventTime > delay;
}

State Clock::transition(State s, Command c) {
  State next = s;
  switch (s) {
    case DISPLAY_VOLUME:
      if (c == SET || c == MODE) {
        writeSettings();
        next = DISPLAY_TIME;
      }
      // auto exit after 3 sec without any button press
      if (noInputDuringMS(3000)) {
        writeSettings();
        next = DISPLAY_TIME;
      }
      if (c == UP) {
        settings.volume = (settings.volume + 1) % 100;
        applyVolume();
      }
      if (c == DOWN) {
        settings.volume = (settings.volume + 99) % 100; // add 99 to ensure positive result
        applyVolume();
      }
      break;
    case DISPLAY_TIME:
      if (c == MODE) {
        next = DISPLAY_DATE;
      }
      if (c == SET) {
        next = SET_HOURS;
        copyTime();
      }
      if (c == UP | c == DOWN) {
        next = DISPLAY_VOLUME;
      }
      break;
    case SET_HOURS:
      if (c == MODE) {
        next = DISPLAY_TIME;
        writeTime();
      }
      if (c == SET) {
        next = SET_MINUTES;
      }
      if (c == UP) {
        hour = (hour + 1) % 24;
      }
      if (c == DOWN) {
        hour = (hour + 23) % 24; // add 23 to ensure positive result
      }
      break;
    case SET_MINUTES:
      if (c == MODE) {
        next = DISPLAY_TIME;
        writeTime();
      }
      if (c == SET) {
        next = DISPLAY_TIME;
        writeTime();
      }
      if (c == UP) {
        minute = (minute + 1) % 60;
      }
      if (c == DOWN) {
        minute = (minute + 59) % 60; // add 59 to ensure positive result
      }
      break;
    case DISPLAY_DATE:
      if (c == MODE) {
        next = DISPLAY_ALARM_1;
      }
      if (c == SET) {
        next = SET_DAY;
        copyDate();
      }
      break;
    case SET_DAY: {
      if (c == MODE) {
        next = DISPLAY_DATE;
        writeDate();
      }
      if (c == SET) {
        next = SET_MONTH;
      }
      // here we need to handle number of days in a month + leap years
      uint8_t daysInCurrentMonth = daysInMonth[month];
      if ((year + 2000) % 4 == 0 && month == 1) { // enough for [2000-2099] (2000 is divisible by 400)
        daysInCurrentMonth++;
      }
      if (c == UP) {
        day = (day + 1) % daysInCurrentMonth;
      }
      if (c == DOWN) {
        day = (day + daysInCurrentMonth - 1) % daysInCurrentMonth;
      }
      break;
    }
    case SET_MONTH:
      if (c == MODE) {
        next = DISPLAY_DATE;
        writeDate();
      }
      if (c == SET) {
        next = SET_YEAR;
      }
      if (c == UP) {
        month = (month + 1) % 12;
      }
      if (c == DOWN) {
        month = (month + 11) % 12; // add 99 to ensure positive result
      }
      break;
    case SET_YEAR:
      if (c == MODE) {
        next = DISPLAY_DATE;
        writeDate();
      }
      if (c == SET) {
        next = DISPLAY_DATE;
        writeDate();
      }
      if (c == UP) {
        year = (year + 1) % 100;
      }
      if (c == DOWN) {
        year = (year + 99) % 100; // add 99 to ensure positive result
      }
      break;
    case DISPLAY_ALARM_1:
      if (c == MODE) {
        next = DISPLAY_ALARM_2;
      }
      if (c == SET) {
        next = SET_ENABLED_1;
      }
      break;
    case SET_ENABLED_1:
      if (c == MODE) {
        writeSettings();
        next = DISPLAY_ALARM_1;
      }
      if (c == SET) {
        next = SET_HOURS_1;
      }
      if (c == UP || c == DOWN) {
        settings.alarm1.enabled = !settings.alarm1.enabled;
      }
      break;
    case SET_HOURS_1:
      if (c == MODE) {
        writeSettings();
        next = DISPLAY_ALARM_1;
      }
      if (c == SET) {
        next = SET_MINUTES_1;
      }
      if (c == UP) {
        settings.alarm1.hour = (settings.alarm1.hour + 1) % 24;
      }
      if (c == DOWN) {
        settings.alarm1.hour = (settings.alarm1.hour + 23) % 24; // add 23 to ensure positive result
      }
      break;
    case SET_MINUTES_1:
      if (c == MODE) {
        writeSettings();
        next = DISPLAY_ALARM_1;
      }
      if (c == SET) {
        next = SET_WEEKEND_1;
      }
      if (c == UP) {
        settings.alarm1.minute = (settings.alarm1.minute + 1) % 60;
      }
      if (c == DOWN) {
        settings.alarm1.minute = (settings.alarm1.minute + 59) % 60; // add 59 to ensure positive result
      }
      break;
    case SET_WEEKEND_1:
      if (c == MODE) {
        writeSettings();
        next = DISPLAY_ALARM_1;
      }
      if (c == SET) {
        next = SET_TRACK_1;
      }
      if (c == UP || c == DOWN) {
        settings.alarm1.weekend = !settings.alarm1.weekend;
      }
      break;
    case SET_TRACK_1:
      if (c == MODE) {
        player.stopPlaying(); // in case we were previewing the track
        writeSettings();
        next = DISPLAY_ALARM_1;
      }
      if (c == SET) {
        player.stopPlaying(); // in case we were previewing the track
        writeSettings();
        next = DISPLAY_ALARM_1;
      }
      if (c == UP) {
        settings.alarm1.track = (settings.alarm1.track + 1) % alarmTrackCount;
        // preview
        playAlarm(settings.alarm2.track);
      }
      if (c == DOWN) {
        settings.alarm1.track = (settings.alarm1.track + (alarmTrackCount - 1)) % alarmTrackCount; // add 8 to ensure positive result
        // preview
        playAlarm(settings.alarm2.track);
      }
      break;
    case DISPLAY_ALARM_2:
      if (c == MODE) {
        next = DISPLAY_TIME;
      }
      if (c == SET) {
        next = SET_ENABLED_2;
      }
      break;
    case SET_ENABLED_2:
      if (c == MODE) {
        writeSettings();
        next = DISPLAY_ALARM_2;
      }
      if (c == SET) {
        next = SET_HOURS_2;
      }
      if (c == UP || c == DOWN) {
        settings.alarm2.enabled = !settings.alarm2.enabled;
      }
      break;
    case SET_HOURS_2:
      if (c == MODE) {
        writeSettings();
        next = DISPLAY_ALARM_2;
      }
      if (c == SET) {
        next = SET_MINUTES_2;
      }
      if (c == UP) {
        settings.alarm2.hour = (settings.alarm2.hour + 1) % 24;
      }
      if (c == DOWN) {
        settings.alarm2.hour = (settings.alarm2.hour + 23) % 24; // add 23 to ensure positive result
      }
      break;
    case SET_MINUTES_2:
      if (c == MODE) {
        writeSettings();
        next = DISPLAY_ALARM_2;
      }
      if (c == SET) {
        next = SET_WEEKEND_2;
      }
      if (c == UP) {
        settings.alarm2.minute = (settings.alarm2.minute + 1) % 60;
      }
      if (c == DOWN) {
        settings.alarm2.minute = (settings.alarm2.minute + 59) % 60; // add 59 to ensure positive result
      }
      break;
    case SET_WEEKEND_2:
      if (c == MODE) {
        writeSettings();
        next = DISPLAY_ALARM_2;
      }
      if (c == SET) {
        next = SET_TRACK_2;
      }
      if (c == UP || c == DOWN) {
        settings.alarm2.weekend = !settings.alarm2.weekend;
      }
      break;
    case SET_TRACK_2:
      if (c == MODE) {
        player.stopPlaying(); // in case we were previewing the track
        writeSettings();
        next = DISPLAY_ALARM_2;
      }
      if (c == SET) {
        player.stopPlaying(); // in case we were previewing the track
        writeSettings();
        next = DISPLAY_ALARM_2;
      }
      if (c == UP) {
        settings.alarm2.track = (settings.alarm2.track + 1) % alarmTrackCount;
        // preview
        playAlarm(settings.alarm2.track);
      }
      if (c == DOWN) {
        settings.alarm2.track = (settings.alarm2.track + (alarmTrackCount - 1)) % alarmTrackCount; // add 8 to ensure positive result
        // preview
        playAlarm(settings.alarm2.track);
      }
      break;
    case ALARM_1:
      if (c == STOP) {
        player.stopPlaying();
        alarm1Stopped = true;
        next = DISPLAY_TIME;
      }
      break;
    case ALARM_2:
      if (c == STOP) {
        player.stopPlaying();
        alarm2Stopped = true;
        next = DISPLAY_TIME;
      }
      break;
    default:
      next = s;
  }
  return next;
}
