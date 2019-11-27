#ifndef constants_h
#define constants_h

// Sound files
#define TRACK_BOOT          "/sounds/boot.mp3"
#define TRACK_BUTTON_PRESS  "/sounds/button.mp3"
#define TRACK_NAP           "/alarms/nap.mp3"
#define TRACK_ALARM_PATTERN "/alarms/?.mp3"

// Display constants
#define CENTER_COLON        0x02
#define LEFT_COLON_LOWER    0x08
#define LEFT_COLON_UPPER    0x04
#define DECIMAL_POINT       0x10
#define BLINK_DIGIT_1    0b00001
#define BLINK_DIGIT_2    0b00010
#define BLINK_DOTS       0b00100
#define BLINK_DIGIT_3    0b01000
#define BLINK_DIGIT_4    0b10000

#define LETTER_A       0b1110111
#define LETTER_E       0b1111001
#define LETTER_L       0b0111000
#define LETTER_S       0b1101101
#define LETTER_U       0b0111110
#define LETTER_P       0b1110011
#define LETTER_b       0b1111100
#define LETTER_o       0b1011100
#define LETTER_n       0b1010100
#define LETTER_f       0b1110001
#define LETTER_r       0b1010000
#define LETTER_t       0b1111000

#define EXIT_VOLUME_DELAY   3000
#define EXIT_MENU_DELAY    10000
#define BLINK_DELAY          300
#define LONG_PRESS_DELAY    2000
#define NAP_INCREMENT        600
#define NAP_INTRO_DELAY     2000
#define NAP_SET_DELAY       5000
#define DARK_MODE_DELAY    60000

/********
 * PINS *
 ********/

// VS1053
// DREQ should be an Int pin *if possible* (not possible on 32u4)
#define VS1053_DREQ       9     // VS1053 Data request, ideally an Interrupt pin
#define VS1053_RESET     -1     // VS1053 reset pin (not used!)
#define VS1053_CS         6     // VS1053 chip select pin (output)
#define VS1053_DCS       10     // VS1053 Data/command select pin (output)

// SD
#define CARD_CS           5     // Card chip select pin
#define ALT_CARD_CS       4
#define ALT_CARD_DETECT   7

// Buttons
#define BUTTON_TOP_PIN   19
#define BUTTON_UP_PIN    16
#define BUTTON_DOWN_PIN  17
#define BUTTON_LEFT_PIN  15
#define BUTTON_RIGHT_PIN 18

// Other
#define POWER_LED        13

#endif
