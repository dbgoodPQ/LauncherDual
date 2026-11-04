# LauncherDual

A fork of [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) that adds support for an
external **Waveshare 2inch LCD module (ST7789V, 240x320)** which mirrors the device's built-in screen.

The target device is the **M5Stack Cardputer ADV** (env `m5stack-cardputer-dual`). Apart from the
mirror, everything else is stock Launcher — for features, usage and other supported devices, see the
[upstream project](https://github.com/bmorcelli/Launcher).

> **Status: tested on the M5Stack Cardputer ADV.** Builds come from the **Build Cardputer dual** GitHub Action.

## What it adds

- Everything drawn on the built-in screen is re-sent to the external panel, scaled to fill it.
- Scaling is nearest-neighbour, row by row, with no frame buffer (about 1 KB of RAM).
- The external backlight follows the brightness setting and turns off with the device.
- The external panel shares SCK/MOSI with the SD card; only CS, DC, RST and BL are its own.

Implementation: `include/ExtMirrorBus.h` wraps the display bus when `EXT_MIRROR` is defined
(hooked in `lib/DisplayDrivers/src/backends/ardgfx.cpp`).

## Wiring (Cardputer ADV → Waveshare 2inch)

| Waveshare pin | Cardputer ADV GPIO | Notes |
| --- | --- | --- |
| DIN (MOSI) | G14 | shared with the SD card |
| CLK (SCK) | G40 | shared with the SD card |
| CS | G5 | |
| DC | G6 | |
| RST | G3 | |
| BL | G4 | backlight, software-controlled |
| VCC | 3V3 | set the module's voltage selector to 3.3V |
| GND | GND | |

This is the wiring used for testing. Pins are set as `EXT_TFT_*` in
`boards/m5stack-cardputer/platformio.ini`.

## Build

```
git clone --recursive https://github.com/dbgoodPQ/LauncherDual
cd LauncherDual
pio run -e m5stack-cardputer-dual
```

Output: `Launcher-m5stack-cardputer-dual.bin` in the project root.

## Flash

Enter download mode (hold G0, press RST), then:

```
esptool.py --chip esp32s3 write_flash 0x0 Launcher-m5stack-cardputer-dual.bin
```

## Options

| Flag | Effect |
| --- | --- |
| `-DEXT_MIRROR_H=180` | Keep the built-in screen's proportions (30 px bars top and bottom) instead of stretching to 320x240 |
| `-DEXT_MIRROR_W=240 -DEXT_MIRROR_H=135` | Unscaled, centred mirror |
| `-DEXT_MADCTL_XOR=0x40` / `0x80` | Flip the external screen left/right / up/down |
| `-DEXT_SPI_SPEED=20000000` | Lower SPI speed if the picture is garbled |

The mirror works in landscape rotations only. More details and troubleshooting are in
[README_DUAL.md](README_DUAL.md).

## Credits

All credit for Launcher goes to [bmorcelli](https://github.com/bmorcelli) and the upstream contributors.
Licensed under the MIT License — see [LICENSE](LICENSE).
