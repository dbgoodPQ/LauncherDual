// Mirror of the built-in screen onto a second SPI panel (m5stack-cardputer-dual).
//
// The panel driver talks to one Arduino_DataBus. This bus hands every byte to
// the built-in panel's HWSPI bus and to a second HWSPI bus wired to the
// external panel, so whatever the launcher draws shows up on both screens.
//
// Why this works without any scaling or window maths: the built-in 135x240
// ST7789 is addressed through column/row offsets (x 40, y 53 in landscape).
// Those same offsets centre the 240x135 picture on a 320x240 panel, so the
// address-window commands can go to both panels unchanged.
//
// The external panel shares SCK/MOSI with the SD card on the card's own SPI
// object (sdcardSPI), so Arduino's transaction lock keeps the two apart. Only
// its CS, DC and RST are its own.
#ifndef EXT_MIRROR_BUS_H
#define EXT_MIRROR_BUS_H

#include <Arduino_GFX_Library.h>
#include <SPI.h>

extern SPIClass sdcardSPI; // src/sd_functions.cpp

#ifndef EXT_MADCTL_XOR
// Flips the external panel only: 0x40 mirrors left/right, 0x80 up/down.
#define EXT_MADCTL_XOR 0x00
#endif

class ExtMirrorBus : public Arduino_DataBus {
  public:
    explicit ExtMirrorBus(Arduino_HWSPI *main)
        : _a(main),
          _b(new Arduino_HWSPI(EXT_TFT_DC, EXT_TFT_CS, SDCARD_SCK, SDCARD_MOSI, SDCARD_MISO, &sdcardSPI)) {}

    bool begin(int32_t speed = GFX_NOT_DEFINED, int8_t dataMode = GFX_NOT_DEFINED) override {
        // Bring the shared bus up exactly as sd_functions.cpp does, so the SD
        // card's own begin() later finds it running and leaves it alone.
        sdcardSPI.begin(SDCARD_SCK, SDCARD_MISO, SDCARD_MOSI, SDCARD_CS);
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
    void writeCommand(uint8_t c) override {
        _a->writeCommand(c);
        _b->writeCommand(c);
    }
    void writeCommand16(uint16_t c) override {
        _a->writeCommand16(c);
        _b->writeCommand16(c);
    }
    void writeCommandBytes(uint8_t *data, uint32_t len) override {
        _a->writeCommandBytes(data, len);
        _b->writeCommandBytes(data, len);
    }
    void write(uint8_t d) override {
        _a->write(d);
        _b->write(d);
    }
    void write16(uint16_t d) override {
        _a->write16(d);
        _b->write16(d);
    }
    void writeRepeat(uint16_t p, uint32_t len) override {
        _a->writeRepeat(p, len);
        _b->writeRepeat(p, len);
    }
    void writeBytes(uint8_t *data, uint32_t len) override {
        _a->writeBytes(data, len);
        _b->writeBytes(data, len);
    }
    void writePixels(uint16_t *data, uint32_t len) override {
        _a->writePixels(data, len);
        _b->writePixels(data, len);
    }
#if !defined(LITTLE_FOOT_PRINT)
    void writePattern(uint8_t *data, uint8_t len, uint32_t repeat) override {
        _a->writePattern(data, len, repeat);
        _b->writePattern(data, len, repeat);
    }
#endif
    // The one command the two panels may disagree on: MADCTL (0x36) sets the
    // scan direction, and a module can be mounted mirrored relative to the
    // built-in one. Everything else in the base class (writeC8D16D16, the init
    // batch, indexed pixels...) is built from the primitives above.
    void writeC8D8(uint8_t c, uint8_t d) override {
        _a->writeC8D8(c, d);
        _b->writeC8D8(c, c == 0x36 ? (d ^ EXT_MADCTL_XOR) : d);
    }

  private:
    Arduino_HWSPI *_a; // built-in screen
    Arduino_HWSPI *_b; // external screen
};

#endif // EXT_MIRROR_BUS_H
