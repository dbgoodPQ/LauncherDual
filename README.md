# LauncherDual

A fork of [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) for the **M5Stack Cardputer ADV**
(env `m5stack-cardputer-dual`) that adds the option to use an external
**Waveshare 2inch LCD module (ST7789V, 240x320)** as the main screen.

This is **not a dual-screen port**: the launcher uses one screen at a time, and you switch between the
built-in screen and the external one. Apart from that, everything is stock Launcher. For features, usage
and other supported devices, see the [upstream project](https://github.com/bmorcelli/Launcher).

> **Status:** internal mode (with mirror) tested on the M5Stack Cardputer ADV. External mode builds, but has
> not been tested on hardware yet. Builds come from the **Build Cardputer dual** GitHub Action.

## Screens

| Mode | Main screen | External panel |
| --- | --- | --- |
| **Internal** (default) | Built-in 240x135 screen | Shows a scaled copy of the built-in screen (mirror). Usable, but text is soft. |
| **External** | External panel, drawn natively at 320x240 (sharp text and icons) | Is the screen. The built-in one stays dark. |

**Switching between internal and external:**

- hold **G0 for 3 s**, wait for the stripe "Release: external screen" (or internal), then release: the device
  reboots on the other screen, or
- use **Config > `[Internal] Screen` / `[External] Screen`**.

The choice is remembered across reboots and power-off. If you switch to external with no panel plugged in,
hold G0 for 3 s again and release to come back. A short G0 press is still Select (now sent on release).
Details in [README_DUAL.md](README_DUAL.md#two-screen-modes).

Other notes:

- The external backlight follows the brightness setting and turns off with the device.
- The external panel shares SCK/MOSI with the SD card; only CS, DC, RST and BL are its own.
- The mirror in internal mode is nearest-neighbour, row by row, with no frame buffer (about 1 KB of RAM).

Implementation: `include/ExtDisplay.h` and `boards/m5stack-cardputer/interface.cpp` (screen choice),
`include/ExtMirrorBus.h` (mirror), both hooked in `lib/DisplayDrivers/src/backends/ardgfx.cpp`.

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

The `EXT_MIRROR_*` and `EXT_MADCTL_XOR` flags only affect the mirror (internal mode), which works in
landscape rotations only; `EXT_SPI_SPEED` applies to both modes. More details and troubleshooting are in
[README_DUAL.md](README_DUAL.md).

## Credits

All credit for Launcher goes to [bmorcelli](https://github.com/bmorcelli) and the upstream contributors.
Licensed under the MIT License — see [LICENSE](LICENSE).
