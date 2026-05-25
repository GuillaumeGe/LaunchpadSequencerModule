#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #ifndef PROGMEM
    #define PROGMEM
  #endif
  #include <stdint.h>
#endif

#define ICON_WIDTH  16
#define ICON_HEIGHT 16
#define ICON_PADDING 4

// Usage: display.drawBitmap(x, y, ICON_PLAY, ICON_WIDTH, ICON_HEIGHT, SSD1306_WHITE);

// Play — right-pointing filled triangle (1px padding, tip at col 13 rows 7-8)
// ................
// .X..............
// .XXX............
// .XXXXX..........
// .XXXXXXX........
// .XXXXXXXXX......
// .XXXXXXXXXXX....
// .XXXXXXXXXXXXX..
// .XXXXXXXXXXXXX..
// .XXXXXXXXXXX....
// .XXXXXXXXX......
// .XXXXXXX........
// .XXXXX..........
// .XXX............
// .X..............
// ................
const uint8_t ICON_PLAY[] PROGMEM = {
    0x00, 0x00,
    0x40, 0x00,
    0x70, 0x00,
    0x7C, 0x00,
    0x7F, 0x00,
    0x7F, 0xC0,
    0x7F, 0xF0,
    0x7F, 0xFC,
    0x7F, 0xFC,
    0x7F, 0xF0,
    0x7F, 0xC0,
    0x7F, 0x00,
    0x7C, 0x00,
    0x70, 0x00,
    0x40, 0x00,
    0x00, 0x00
};

// Pause — two 4px-wide vertical bars (cols 2-5 and cols 10-13, rows 2-13)
// ................
// ................
// ..XXXX....XXXX..
// ..XXXX....XXXX..
// ..XXXX....XXXX..
// ..XXXX....XXXX..
// ..XXXX....XXXX..
// ..XXXX....XXXX..
// ..XXXX....XXXX..
// ..XXXX....XXXX..
// ..XXXX....XXXX..
// ..XXXX....XXXX..
// ..XXXX....XXXX..
// ..XXXX....XXXX..
// ................
// ................
const uint8_t ICON_PAUSE[] PROGMEM = {
    0x00, 0x00,
    0x00, 0x00,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0x00, 0x00,
    0x00, 0x00
};

// Stop — filled 12x12 square centered in 16x16 (cols 2-13, rows 2-13)
// ................
// ................
// ..XXXXXXXXXXXX..
// ..XXXXXXXXXXXX..
// ..XXXXXXXXXXXX..
// ..XXXXXXXXXXXX..
// ..XXXXXXXXXXXX..
// ..XXXXXXXXXXXX..
// ..XXXXXXXXXXXX..
// ..XXXXXXXXXXXX..
// ..XXXXXXXXXXXX..
// ..XXXXXXXXXXXX..
// ..XXXXXXXXXXXX..
// ..XXXXXXXXXXXX..
// ................
// ................
const uint8_t ICON_STOP[] PROGMEM = {
    0x00, 0x00,
    0x00, 0x00,
    0x3F, 0xFC,
    0x3F, 0xFC,
    0x3F, 0xFC,
    0x3F, 0xFC,
    0x3F, 0xFC,
    0x3F, 0xFC,
    0x3F, 0xFC,
    0x3F, 0xFC,
    0x3F, 0xFC,
    0x3F, 0xFC,
    0x3F, 0xFC,
    0x3F, 0xFC,
    0x00, 0x00,
    0x00, 0x00
};

// Arrow Right — filled right-pointing triangle (chevron), tip at col 14 rows 7-8, base at col 2
// ................
// ..X.............
// ...XX...........
// ....XXX.........
// .....XXXX.......
// ......XXXXX.....
// .......XXXXXX...
// ........XXXXXXX.
// ........XXXXXXX.
// .......XXXXXX...
// ......XXXXX.....
// .....XXXX.......
// ....XXX.........
// ...XX...........
// ..X.............
// ................
const uint8_t ICON_ARROW_RIGHT[] PROGMEM = {
    0x00, 0x00,
    0x20, 0x00,
    0x18, 0x00,
    0x0E, 0x00,
    0x07, 0x80,
    0x03, 0xE0,
    0x01, 0xF8,
    0x00, 0xFE,
    0x00, 0xFE,
    0x01, 0xF8,
    0x03, 0xE0,
    0x07, 0x80,
    0x0E, 0x00,
    0x18, 0x00,
    0x20, 0x00,
    0x00, 0x00
};

// Arrow Left — filled left-pointing triangle (chevron), tip at col 1 rows 7-8, base at col 13
// ................
// .............X..
// ...........XX...
// .........XXX....
// .......XXXX.....
// .....XXXXX......
// ...XXXXXX.......
// .XXXXXXX........
// .XXXXXXX........
// ...XXXXXX.......
// .....XXXXX......
// .......XXXX.....
// .........XXX....
// ...........XX...
// .............X..
// ................
const uint8_t ICON_ARROW_LEFT[] PROGMEM = {
    0x00, 0x00,
    0x00, 0x04,
    0x00, 0x18,
    0x00, 0x70,
    0x01, 0xE0,
    0x07, 0xC0,
    0x1F, 0x80,
    0x7F, 0x00,
    0x7F, 0x00,
    0x1F, 0x80,
    0x07, 0xC0,
    0x01, 0xE0,
    0x00, 0x70,
    0x00, 0x18,
    0x00, 0x04,
    0x00, 0x00
};

// Letter P — 2px stroke, bump cols 2-10, right wall cols 10-11, 4-row open counter
// ................
// ..XXXXXXXXX.....
// ..XXXXXXXXX.....
// ..XX......XX....
// ..XX......XX....
// ..XX......XX....
// ..XX......XX....
// ..XXXXXXXXX.....
// ..XXXXXXXXX.....
// ..XX............
// ..XX............
// ..XX............
// ..XX............
// ..XX............
// ..XX............
// ................
const uint8_t ICON_LETTER_P[] PROGMEM = {
    0x00, 0x00,
    0x3F, 0xE0,
    0x3F, 0xE0,
    0x30, 0x30,
    0x30, 0x30,
    0x30, 0x30,
    0x30, 0x30,
    0x3F, 0xE0,
    0x3F, 0xE0,
    0x30, 0x00,
    0x30, 0x00,
    0x30, 0x00,
    0x30, 0x00,
    0x30, 0x00,
    0x30, 0x00,
    0x00, 0x00
};
