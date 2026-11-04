# Launcher — Cardputer ADV with a mirrored external screen

A build of [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) for the M5Stack Cardputer ADV that
mirrors the built-in screen onto a second SPI panel. Target panel: Waveshare 2inch LCD module
(ST7789V, 240x320).

Env: `m5stack-cardputer-dual`. Everything else is stock Launcher.

> **Status: not yet tested on hardware.** The mirror class was syntax-checked against the real
> Arduino_GFX bus headers, but no full firmware build or flash was possible where it was written.
> The GitHub Action in this repo does the first real build.

## How it works

The panel driver talks to one data bus. `include/ExtMirrorBus.h` replaces it with a bus that sends every
byte to the built-in panel and to the external one. `lib/DisplayDrivers/src/backends/ardgfx.cpp` wraps the
bus when `EXT_MIRROR` is defined (4 lines; the library is vendored in this repo for that reason).

No scaling is done. The built-in panel is addressed with offsets (x 40, y 53 in landscape) that happen to
centre a 240x135 picture on a 320x240 panel, so the same address-window commands go to both screens. The
mirror appears 1:1, centred, with black borders.

The external panel shares SCK and MOSI with the SD card (the card's own `sdcardSPI` object, so transactions
are serialised). Only CS, DC and RST are its own.

## Wiring (Cardputer ADV header to Waveshare 2inch module)

| Waveshare pin | Cardputer ADV GPIO | Notes |
| --- | --- | --- |
| DIN (MOSI) | G14 | shared with the SD card |
| CLK (SCK) | G40 | shared with the SD card |
| CS | G5 | |
| DC | G6 | |
| RST | G3 | |
| VCC | 3V3 | set the module's voltage selector to 3.3V so supply and logic match |
| GND | GND | |
| BL | 3V3 | backlight always on |

The GPIOs match the three other Cardputer ADV external-display projects I found
(`zeromem0/cardputer_adv`, `moes-bobandy/botcorps-adv`, `purplxhazee/Purplx-`). I did not check them against
M5Stack's schematic, so verify the header pinout before powering up. The pin numbers are `EXT_TFT_*` in
`boards/m5stack-cardputer/platformio.ini`.

## Build

```
git clone --recursive https://github.com/dbgoodPQ/LauncherDual
cd LauncherDual
pio run -e m5stack-cardputer-dual
```

The merged image is written to `Launcher-m5stack-cardputer-dual.bin` in the project root. It can also be
built by the **Build Cardputer dual** GitHub Action (Actions tab, download the artifact).

## Flash

Download mode: hold G0 and press RST. Then:

```
esptool.py --chip esp32s3 write_flash 0x0 Launcher-m5stack-cardputer-dual.bin
```

(or flash the file at offset 0x0 from any web flasher that accepts a custom merged binary.)

## If something looks wrong

| Symptom | Try |
| --- | --- |
| External screen stays white or black | Check CS/DC/RST wiring and that the module is at 3.3V. Backlight pin must be powered. |
| Image mirrored or upside down | Uncomment `-DEXT_MADCTL_XOR=0x40` (left/right) or `0x80` (up/down) in the env. This flips the external screen only. |
| Colours inverted | The driver sends INVON, as the built-in IPS panel needs. If your module disagrees, tell me and I will add a flag. |
| Garbled picture or noise | Both screens share one SPI speed (default 40 MHz). Uncomment `-DEXT_SPI_SPEED=20000000` in the env. |
| SD card not found | The external screen must not be wired to G39 (SD MISO) or G12 (SD CS). |
| Built-in screen works, external blank | The external panel may not accept the init sequence. Send me the model of the module. |
