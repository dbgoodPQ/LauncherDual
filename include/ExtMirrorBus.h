// Mirror of the built-in screen onto a second SPI panel (m5stack-cardputer-dual).
//
// The panel driver talks to one Arduino_DataBus. This bus sends everything to
// the built-in panel unchanged, and re-sends the picture to the external panel
// scaled up to fill it (nearest neighbour, 240x135 -> EXT_MIRROR_W x EXT_MIRROR_H).
//
// How the scaling works without a frame buffer: the driver sets a drawing
// window (CASET/RASET), sends RAMWR and then streams the window's pixels row by
// row. This class tracks that window, collects one source row at a time (240
// pixels at most) and, when the row is complete, writes it to the external
// panel stretched horizontally and repeated as many times as vertical scaling
// needs. The external window is the scaled copy of the source window, so any
// rectangle the launcher draws lands in the right place.
//
// Every other command (init, MADCTL, inversion, sleep...) goes to both panels.
//
// The external panel shares SCK/MOSI with the SD card on the card's own SPI
// object (sdcardSPI), so Arduino's transaction lock keeps the two apart. Only
// its CS, DC and RST are its own.
//
// Landscape only (the launcher's rotation 1 and 3). In a portrait rotation the
// external panel simply stops updating.
//
// extRoute picks which panel(s) receive pixels (EXT_INT, EXT_EXT or both). It is
// sampled at each RAMWR, so a window always lands on one consistent set of panels.
// Commands and CASET/RASET always reach both, so each controller stays
// initialised, rotated and in step with the driver's address-window cache.
#ifndef EXT_MIRROR_BUS_H
#define EXT_MIRROR_BUS_H

#include <Arduino_GFX_Library.h>
#include <SPI.h>
#ifdef EXT_TFT_BL
#include <driver/gpio.h>
#endif

extern SPIClass sdcardSPI; // src/sd_functions.cpp
#include "ExtRoute.h"

#ifndef EXT_MIRROR_W
#define EXT_MIRROR_W 320 // size of the mirrored picture on the external panel
#endif
#ifndef EXT_MIRROR_H
#define EXT_MIRROR_H 240 // 180 keeps the built-in screen's proportions (bars top and bottom)
#endif
#ifndef EXT_MADCTL_XOR
// Flips the external panel only: 0x40 mirrors left/right, 0x80 up/down.
#define EXT_MADCTL_XOR 0x00
#endif

static_assert(EXT_MIRROR_W >= 240 && EXT_MIRROR_W <= 320, "EXT_MIRROR_W must be 240..320");
static_assert(EXT_MIRROR_H >= 135 && EXT_MIRROR_H <= 240, "EXT_MIRROR_H must be 135..240");

class ExtMirrorBus : public Arduino_DataBus {
  public:
    explicit ExtMirrorBus(Arduino_HWSPI *main)
        : _a(main),
          _b(new Arduino_HWSPI(EXT_TFT_DC, EXT_TFT_CS, SDCARD_SCK, SDCARD_MOSI, SDCARD_MISO, &sdcardSPI)) {}

    bool begin(int32_t speed = GFX_NOT_DEFINED, int8_t dataMode = GFX_NOT_DEFINED) override {
        // Bring the shared bus up exactly as sd_functions.cpp does, so the SD
        // card's own begin() later finds it running and leaves it alone.
        sdcardSPI.begin(SDCARD_SCK, SDCARD_MISO, SDCARD_MOSI, SDCARD_CS);
#ifdef EXT_TFT_BL
        // powerOff() holds the backlight pin low through deep sleep; let go of it after the wake-up reset.
        gpio_hold_dis((gpio_num_t)EXT_TFT_BL);
        gpio_deep_sleep_hold_dis();
#endif
        pinMode(EXT_TFT_RST, OUTPUT);
        digitalWrite(EXT_TFT_RST, HIGH);
        delay(10);
        digitalWrite(EXT_TFT_RST, LOW);
        delay(20);
        digitalWrite(EXT_TFT_RST, HIGH);
        delay(120);
#ifdef EXT_SPI_SPEED
        speed = EXT_SPI_SPEED; // both panels share one SPISettings, so this applies to both
#endif
        bool okA = _a->begin(speed, dataMode);
        bool okB = _b->begin(speed, dataMode);
        return okA && okB;
    }
    void beginWrite() override {
        _a->beginWrite();
        _b->beginWrite();
    }
    void endWrite() override {
        _b->endWrite();
        _a->endWrite();
    }

    // ---- commands: to both panels, except RAMWR which starts a scaled window ----
    void writeCommand(uint8_t c) override {
        if (c == 0x2C) { // RAMWR: pixel data follows
            _pix = true;
            _half = false;
            _ra = extRoute & EXT_INT;
            if (_ra) _a->writeCommand(c);
            startWindow();
        } else {
            _pix = false;
            _a->writeCommand(c);
            _b->writeCommand(c);
        }
    }
    void writeCommand16(uint16_t c) override {
        _pix = false;
        _a->writeCommand16(c);
        _b->writeCommand16(c);
    }
    void writeCommandBytes(uint8_t *data, uint32_t len) override {
        _pix = false;
        _a->writeCommandBytes(data, len);
        _b->writeCommandBytes(data, len);
    }
    void writeC8D8(uint8_t c, uint8_t d) override {
        _pix = false;
        _a->writeC8D8(c, d);
        if (c == 0x36) { // MADCTL: scan direction. Landscape rotations 1/3 differ by one row offset.
            _landscape = d & 0x20;
            _yo = (d & 0x40) ? 53 : 52;
            d ^= EXT_MADCTL_XOR;
        }
        _b->writeC8D8(c, d);
    }
    void writeC8D16D16(uint8_t c, uint16_t d1, uint16_t d2) override {
        _pix = false;
        _a->writeC8D16D16(c, d1, d2);
        if (c == 0x2A) { // CASET: remembered, re-sent scaled at the next RAMWR
            _x0 = d1;
            _x1 = d2;
        } else if (c == 0x2B) { // RASET
            _y0 = d1;
            _y1 = d2;
        } else {
            _b->writeC8D16D16(c, d1, d2);
        }
    }

    // ---- data: pixel stream while RAMWR is active, command parameters otherwise ----
    void write(uint8_t d) override {
        if (A()) _a->write(d);
        if (_pix) byteIn(d);
        else _b->write(d);
    }
    void write16(uint16_t d) override {
        if (A()) _a->write16(d);
        if (_pix) px(d);
        else _b->write16(d);
    }
    void writeRepeat(uint16_t p, uint32_t len) override {
        if (A()) _a->writeRepeat(p, len);
        if (_pix) {
            while (len--) px(p);
        } else {
            _b->writeRepeat(p, len);
        }
    }
    void writeBytes(uint8_t *data, uint32_t len) override {
        if (A()) _a->writeBytes(data, len);
        if (_pix) {
            for (uint32_t i = 0; i < len; i++) byteIn(data[i]);
        } else {
            _b->writeBytes(data, len);
        }
    }
    void writePixels(uint16_t *data, uint32_t len) override {
        if (A()) _a->writePixels(data, len);
        if (_pix) {
            for (uint32_t i = 0; i < len; i++) px(data[i]);
        } else {
            _b->writePixels(data, len);
        }
    }
#if !defined(LITTLE_FOOT_PRINT)
    void writePattern(uint8_t *data, uint8_t len, uint32_t repeat) override {
        if (A()) _a->writePattern(data, len, repeat);
        if (_pix) {
            while (repeat--)
                for (uint8_t i = 0; i < len; i++) byteIn(data[i]);
        } else {
            _b->writePattern(data, len, repeat);
        }
    }
#endif

  private:
    static int ceilDiv(int a, int b) { return (a + b - 1) / b; }
    bool A() const { return !_pix || _ra; } // built-in panel takes this byte

    // Source picture in the built-in panel's memory: 240 columns from x 40, 135 rows from _yo.
    static constexpr int SRC_X0 = 40, SRC_W = 240, SRC_H = 135;
    static constexpr int XOFF = (320 - EXT_MIRROR_W) / 2, YOFF = (240 - EXT_MIRROR_H) / 2;

    // RAMWR arrived: map the remembered source window to the external panel and open it.
    void startWindow() {
        _on = false;
        if (!_landscape || !(extRoute & EXT_EXT)) return;
        int xr0 = _x0 - SRC_X0, xr1 = _x1 - SRC_X0, yr0 = _y0 - _yo, yr1 = _y1 - _yo;
        if (xr0 < 0 || xr1 >= SRC_W || yr0 < 0 || yr1 >= SRC_H || xr1 < xr0 || yr1 < yr0) return;
        if (!_cleared) {
            // The external memory starts as noise and the scaled picture may not cover it all.
            _cleared = true;
            _b->writeC8D16D16(0x2A, 0, 319);
            _b->writeC8D16D16(0x2B, 0, 239);
            _b->writeCommand(0x2C);
            _b->writeRepeat(0, 320UL * 240UL);
        }
        _dx0 = ceilDiv(xr0 * EXT_MIRROR_W, SRC_W);
        _dx1 = ceilDiv((xr1 + 1) * EXT_MIRROR_W, SRC_W) - 1;
        int dy0 = ceilDiv(yr0 * EXT_MIRROR_H, SRC_H);
        int dy1 = ceilDiv((yr1 + 1) * EXT_MIRROR_H, SRC_H) - 1;
        _b->writeC8D16D16(0x2A, _dx0 + XOFF, _dx1 + XOFF);
        _b->writeC8D16D16(0x2B, dy0 + YOFF, dy1 + YOFF);
        _b->writeCommand(0x2C);
        _cx = _x0;
        _cy = _y0;
        _on = true;
    }

    // Bytes of a pixel stream arrive high byte first.
    void byteIn(uint8_t d) {
        if (!_half) {
            _hi = d;
            _half = true;
        } else {
            _half = false;
            px((uint16_t)(_hi << 8 | d));
        }
    }

    // One pixel of the current window, as it goes on the wire.
    void px(uint16_t v) {
        if (!_on) return;
        _row[_cx - _x0] = v;
        if (++_cx > _x1) {
            _cx = _x0;
            sendRow();
            if (++_cy > _y1) _cy = _y0; // the controller wraps inside the window too
        }
    }

    // A source row is complete: write it to the external panel, stretched, once per dest row it covers.
    void sendRow() {
        int ry = _cy - _yo;
        int n = ceilDiv((ry + 1) * EXT_MIRROR_H, SRC_H) - ceilDiv(ry * EXT_MIRROR_H, SRC_H);
        int w = _dx1 - _dx0 + 1;
        for (int i = 0; i < w; i++) _line[i] = _row[((_dx0 + i) * SRC_W) / EXT_MIRROR_W + SRC_X0 - _x0];
        while (n-- > 0) _b->writePixels(_line, w);
    }

    Arduino_HWSPI *_a; // built-in screen
    Arduino_HWSPI *_b; // external screen
    uint16_t _row[SRC_W];
    uint16_t _line[EXT_MIRROR_W];
    uint16_t _x0 = 0, _x1 = 0, _y0 = 0, _y1 = 0, _cx = 0, _cy = 0;
    int _dx0 = 0, _dx1 = 0, _yo = 53;
    uint8_t _hi = 0;
    bool _pix = false, _ra = true, _half = false, _on = false, _landscape = false, _cleared = false;
};

#endif // EXT_MIRROR_BUS_H
