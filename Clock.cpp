#include "Clock.h"

static const uint8_t daysInMonth [] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

FlashStorage(flash_settings, Settings);

uint8_t incr(uint8_t n, uint8_t modulo) {
  return (n + 1) % modulo;
}

uint8_t decr(uint8_t n, uint8_t modulo) {
  return (n + modulo - 1) % modulo;
}

void Clock::die(char* msg, uint8_t errCode) {
   Serial.println(msg);
   display.printErr(errCode);
   display.flush();
   while (1);
}

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
  // disable the power led if everything went well
  pinMode(POWER_LED, OUTPUT);
  digitalWrite(POWER_LED, LOW);
  Serial.println("Full init OK");
}

void Clock::initDisplay() {
  display.begin(0x70); // Sometimes code hangs here after a reset. The Display is not resetted correctly
  display.setBrightness(0);
  display.printBoot();
  display.flush();
}

void Clock::initRTC() {
   if (!rtc.begin()) {
    die("Failed to init RTC", 1);
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
    settings.alarm1.enabled = true;
    settings.alarm1.hour = 18;
    settings.alarm1.minute = 28;
    flash_settings.write(settings);
  }
}

void Clock::initSound() {
  player = Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, sdPin);
  if (! player.begin()) {
    die("Failed to init player", 2);
  }

  // If DREQ is on an interrupt pin we can do background audio playing
  player.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

  applyVolume();
  player.startPlayingFile(TRACK_BOOT);
}

void Clock::initSD() {
  sdPin = CARD_CS;
  pinMode(ALT_CARD_DETECT, INPUT_PULLUP);
  if (digitalRead(ALT_CARD_DETECT)) {
    sdPin = ALT_CARD_CS;
  }
  if (!SD.begin(sdPin)) {
    die("Failed to init SD card", 3);
  }
  // Check files existence
  if (!SD.exists(TRACK_BUTTON_PRESS)) {
    Serial.println("Couldn't find " TRACK_BUTTON_PRESS);
  }
  if (!SD.exists(TRACK_NAP)) {
    Serial.println("Couldn't find " TRACK_NAP);
    die("Missing " TRACK_NAP, 4);
  }
  // Check consecutive alarm track files
  uint8_t i = 0;
  while (checkAlarmFile(i) && i < 8) i++;
  alarmTrackCount = i;
  Serial.print("Alarm tracks found: ");
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

void Clock::playNap() {
  player.stopPlaying();
  player.startPlayingFile(TRACK_NAP);
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
  checkAlarm(settings.alarm1, RINGING_ALARM_1, alarm1Stopped);
  checkAlarm(settings.alarm2, RINGING_ALARM_2, alarm2Stopped);
  checkNap();
}

void Clock::checkNap() {
  if (state == RINGING_NAP && player.stopped()) {
    // track stopped, auto exit
    state = DISPLAY_TIME;
  }
  // only check nap in nap mode
  if (state == DISPLAY_NAP) {
    int32_t remaining = (napTime - rtc.now()).totalseconds();
    if (remaining <= 0) {
      state = RINGING_NAP;
      playNap();
    }
  }
}

void Clock::checkAlarm(Alarm a, State ALARM_X, bool &stoppedFlag) {
  // When reaching the time of an alarm, we emit the ALARM_X command. When the
  // alarm is stopped by the user, if it's still the same time, we mustn't play
  // it again. We set an alarmXStopped flag, which will be removed 1 min before
  // ringing the next day.
  DateTime now = rtc.now();
  uint8_t hour = now.hour();
  uint8_t minute = now.minute();
  uint8_t dow = now.dayOfTheWeek();
  bool isWeekEnd = dow == 0 || dow == 6;

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
    a.enabled &&                 // is the alarm enabled
    (a.weekend || !isWeekEnd) && // is it a week day or is the alarm enabled on week-ends
    !stoppedFlag &&              // has the alarm been stopped today
    state != ALARM_X &&          // is it not already ringing
    a.hour == hour &&            // does it matches the current time
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
        CENTER_COLON |
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
      display.setDots(CENTER_COLON);
      display.setBlinking(BLINK_DIGIT_1 | BLINK_DIGIT_2);
      display.printTime(hour, minute);
      break;
    case SET_MINUTES:
      display.setDots(CENTER_COLON);
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
      display.setDots(LEFT_COLON_UPPER | CENTER_COLON);
      display.setBlinking(BLINK_DIGIT_1 | BLINK_DIGIT_2);
      display.printTime(settings.alarm1.hour, settings.alarm1.minute);
      break;
    case SET_MINUTES_1:
      display.setDots(LEFT_COLON_UPPER | CENTER_COLON);
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
      display.setDots(LEFT_COLON_UPPER | CENTER_COLON);
      display.setBlinking(BLINK_DIGIT_1 | BLINK_DIGIT_2);
      display.printTime(settings.alarm2.hour, settings.alarm2.minute);
      break;
    case SET_MINUTES_2:
      display.setDots(LEFT_COLON_UPPER | CENTER_COLON);
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
    case RINGING_ALARM_1:
      display.setBlinking(BLINK_DOTS);
      display.setDots(LEFT_COLON_UPPER | CENTER_COLON);
      display.printTime(now.hour(), now.minute());
      break;
    case RINGING_ALARM_2:
      display.setBlinking(BLINK_DOTS);
      display.setDots(LEFT_COLON_LOWER | CENTER_COLON);
      display.printTime(now.hour(), now.minute());
      break;

    // Nap
    case DISPLAY_NAP_INTRO:
      display.printNapIntro();
      break;
    case SET_NAP: {
      uint8_t minutes = napTS.totalseconds() / 60; // ts.minutes() would not go over 59
      uint8_t seconds = napTS.seconds();
      display.setDots(CENTER_COLON);
      display.setBlinking(BLINK_DIGIT_1 | BLINK_DIGIT_2 | BLINK_DIGIT_3 | BLINK_DIGIT_4);
      display.printTime(minutes, seconds);
      break;
    }
    case DISPLAY_NAP: {
      TimeSpan ts = napTime - now;
      uint8_t minutes = ts.totalseconds() / 60; // ts.minutes() would not go over 59
      uint8_t seconds = ts.seconds();
      display.setDots(CENTER_COLON);
      display.printTime(minutes, seconds);
      break;
    }
    case RINGING_NAP:
      display.setDots(CENTER_COLON);
      display.setBlinking(BLINK_DOTS | BLINK_DIGIT_1 | BLINK_DIGIT_2 | BLINK_DIGIT_3 | BLINK_DIGIT_4);
      display.printTime(0, 0);
      break;
    case DARK_MODE:
      // print nothing
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
      // auto exit after delay without any button press
      if (noInputDuringMS(EXIT_VOLUME_DELAY)) {
        writeSettings();
        next = DISPLAY_TIME;
      }
      if (c == UP) {
        settings.volume = incr(settings.volume, 100);
        applyVolume();
      }
      if (c == DOWN) {
        settings.volume = decr(settings.volume, 100);
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
      if (c == NAP) {
        next = DISPLAY_NAP_INTRO;
        napTS = TimeSpan(NAP_INCREMENT);
      }
      if (noInputDuringMS(DARK_MODE_DELAY)) {
        next = DARK_MODE;
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
        hour = incr(hour, 24);
      }
      if (c == DOWN) {
        hour = decr(hour, 24);
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
        minute = incr(minute, 60);
      }
      if (c == DOWN) {
        minute = decr(minute, 60);
      }
      break;
    case DISPLAY_DATE:
      // auto exit after delay without any button press
      if (noInputDuringMS(EXIT_MENU_DELAY)) {
        next = DISPLAY_TIME;
      }
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
        day = incr(day, daysInCurrentMonth);
      }
      if (c == DOWN) {
        day = decr(day, daysInCurrentMonth);
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
        month = incr(month, 12);
      }
      if (c == DOWN) {
        month = decr(month, 12);
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
        year = incr(year, 100);
      }
      if (c == DOWN) {
        year = decr(year, 100);
      }
      break;
    case DISPLAY_ALARM_1:
      // auto exit after delay without any button press
      if (noInputDuringMS(EXIT_MENU_DELAY)) {
        next = DISPLAY_TIME;
      }
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
        settings.alarm1.hour = incr(settings.alarm1.hour, 24);
      }
      if (c == DOWN) {
        settings.alarm1.hour = decr(settings.alarm1.hour, 24);
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
        settings.alarm1.minute = incr(settings.alarm1.minute, 60);
      }
      if (c == DOWN) {
        settings.alarm1.minute = decr(settings.alarm1.minute, 60);
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
        settings.alarm1.track = incr(settings.alarm1.track, alarmTrackCount);
        // preview
        playAlarm(settings.alarm1.track);
      }
      if (c == DOWN) {
        settings.alarm1.track = decr(settings.alarm1.track, alarmTrackCount);
        // preview
        playAlarm(settings.alarm1.track);
      }
      break;
    case DISPLAY_ALARM_2:
      // auto exit after delay without any button press
      if (noInputDuringMS(EXIT_MENU_DELAY)) {
        next = DISPLAY_TIME;
      }
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
        settings.alarm2.hour = incr(settings.alarm2.hour, 24);
      }
      if (c == DOWN) {
        settings.alarm2.hour = decr(settings.alarm2.hour, 24);
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
        settings.alarm2.minute = incr(settings.alarm2.minute, 60);
      }
      if (c == DOWN) {
        settings.alarm2.minute = decr(settings.alarm2.minute, 60);
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
        settings.alarm2.track = incr(settings.alarm2.track, alarmTrackCount);
        // preview
        playAlarm(settings.alarm2.track);
      }
      if (c == DOWN) {
        settings.alarm2.track = decr(settings.alarm2.track, alarmTrackCount);
        // preview
        playAlarm(settings.alarm2.track);
      }
      break;
    case RINGING_ALARM_1:
      if (c == STOP_ADD_5) {
        player.stopPlaying();
        alarm1Stopped = true;
        next = DISPLAY_TIME;
      }
      break;
    case RINGING_ALARM_2:
      if (c == STOP_ADD_5) {
        player.stopPlaying();
        alarm2Stopped = true;
        next = DISPLAY_TIME;
      }
      break;
    case RINGING_NAP:
      if (c == STOP_ADD_5) {
        player.stopPlaying();
        next = DISPLAY_TIME;
      }
      break;
    case DISPLAY_NAP_INTRO:
      if (noInputDuringMS(NAP_INTRO_DELAY)) {
        next = SET_NAP;
      }
      break;
    case SET_NAP:
      if (c == NAP) {
        next = DISPLAY_TIME;
      }
      if (c == STOP_ADD_5) {
        napTS = napTS + TimeSpan(NAP_INCREMENT);
        if (napTS.totalseconds() >= 100 * 60) {
          napTS = TimeSpan(99 * 60 + 59);
        }
      }
      if (noInputDuringMS(NAP_SET_DELAY)) {
        next = DISPLAY_NAP;
        napTime = rtc.now() + napTS;
      }
      break;
    case DISPLAY_NAP:
      if (c == NAP) {
        next = DISPLAY_TIME;
      }
      if (c == STOP_ADD_5) {
        DateTime now = rtc.now();
        napTime = napTime + TimeSpan(NAP_INCREMENT);
        if ((napTime - now).totalseconds() >= 100 * 60) {
          napTime = now + TimeSpan(99 * 60 + 59);
        }
      }
      break;
    case DARK_MODE:
      if (c != NONE) {
        next = DISPLAY_TIME;
      }
      break;
    default:
      next = s;
  }
  return next;
}
