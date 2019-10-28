#include "Display.h"

// Print without the first leading zero 01:23 => 1:23, 00:00 => 0:00
void Display::printTime(uint8_t hour, uint8_t minute) {
  uint8_t digits[] = {};
  digits[0] = hour / 10;
  digits[1] = hour - 10 * digits[0];
  digits[2] = minute / 10;
  digits[3] = minute - 10 * digits[2];
  // don't print first digit if zeros
  if (digits[0] > 0) {
    writeDigitNum(0, digits[0]);
  }
  else {
    writeDigitRaw(0, 0); // empty char
  }
  writeDigitNum(1, digits[1]);
  writeDigitNum(3, digits[2]);
  writeDigitNum(4, digits[3]);
}

// print 4 digits
void Display::printDate(uint8_t day, uint8_t month) {
  uint8_t digits[] = {};
  digits[0] = day / 10;
  digits[1] = day - 10 * digits[0];
  digits[2] = month / 10;
  digits[3] = month - 10 * digits[2];
  writeDigitNum(0, digits[0]);
  writeDigitNum(1, digits[1]);
  writeDigitNum(3, digits[2]);
  writeDigitNum(4, digits[3]);
}

void Display::printAlarmEnabled(uint8_t number, boolean enabled) {
  writeDigitRaw(0, LETTER_A);
  writeDigitNum(1, number);
  writeDigitRaw(3, LETTER_o);
  writeDigitRaw(4, enabled ? LETTER_n : LETTER_f);
}

// toggle between SA(turday) and SU(nday)
void Display::printAlarmWeekEnd(uint8_t number, boolean weekend) {
  writeDigitRaw(0, LETTER_S);
  if (millis() % (2 * BLINK_DELAY) < BLINK_DELAY) {
    writeDigitRaw(1, LETTER_A);
  }
  else {
    writeDigitRaw(1, LETTER_U);
  }
  writeDigitRaw(3, LETTER_o);
  writeDigitRaw(4, weekend ? LETTER_n : LETTER_f);
}

void Display::printAlarmTrack(uint8_t number, uint8_t track) {
  writeDigitRaw(0, LETTER_A);
  writeDigitNum(1, number);
  writeDigitRaw(3, 0);
  writeDigitNum(4, track);
}

void Display::printErr(uint8_t errorCode) {
  writeDigitRaw(0, LETTER_E);
  writeDigitRaw(1, LETTER_r);
  writeDigitRaw(3, LETTER_r);
  writeDigitNum(4, errorCode);
}

void Display::flush() {
  // blinking
  if (millis() % (2 * BLINK_DELAY) < BLINK_DELAY) {
    for (uint8_t i = 0; i < 4; i++) {
      if ((blinking >> i) & 1 == 1) {
        writeDigitRaw(i, 0);
      }
    }
  }

  if (changed()) {
    writeDisplay();
    memcpy(lastDisplayBuffer, displaybuffer, sizeof(lastDisplayBuffer));
  }
}

bool Display::changed() {
  return memcmp(lastDisplayBuffer, displaybuffer, sizeof(lastDisplayBuffer)) != 0;
}

void Display::setDots(uint8_t dots) {
  writeDigitRaw(2, dots);
}

void Display::setBlinking(uint8_t digits) {
  blinking = digits;
}
