#ifndef constants_h
#define constants_h

// Sound files
#define TRACK_BOOT          "/sounds/boot.mp3"
#define TRACK_BUTTON_PRESS  "/sounds/button.mp3"
#define TRACK_ALARM_PATTERN "/alarms/?.mp3"

// Display constants
#define CENTER_COLON     0x02
#define LEFT_COLON_LOWER 0x08
#define LEFT_COLON_UPPER 0x04
#define DECIMAL_POINT    0x10
#define DIGIT_1        0b0001
#define DIGIT_2        0b0010
#define DIGIT_3        0b0100
#define DIGIT_4        0b1000

#define BLINK_DELAY 300

#define LETTER_A 0b1110111
#define LETTER_E 0b1111001
#define LETTER_L 0b0111000
#define LETTER_S 0b1101101
#define LETTER_U 0b0111110
#define LETTER_o 0b1011100
#define LETTER_n 0b1010100
#define LETTER_f 0b1110001
#define LETTER_r 0b1010000

// VS1053
#define VS1053_RESET   -1     // VS1053 reset pin (not used!)
#define VS1053_CS       6     // VS1053 chip select pin (output)
#define VS1053_DCS     10     // VS1053 Data/command select pin (output)
#define CARDCS          5     // Card chip select pin
// DREQ should be an Int pin *if possible* (not possible on 32u4)
#define VS1053_DREQ     9     // VS1053 Data request, ideally an Interrupt pin

#endif
