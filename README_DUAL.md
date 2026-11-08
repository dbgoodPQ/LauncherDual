# Launcher — Cardputer ADV with an external screen

A build of [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) for the M5Stack Cardputer ADV that
drives a second SPI panel. Target panel: Waveshare 2inch LCD module
(ST7789V, 240x320).

Env: `m5stack-cardputer-dual`. Everything else is stock Launcher.

> **Status: tested on the M5Stack Cardputer ADV.** Builds come from the **Build Cardputer dual**
> GitHub Action in this repo.

## Screen modes

Settings → `Dual screen` toggles between the two modes. The change applies immediately and is saved
in NVS and `config.conf` (key `dualScreen`).

| Mode | Built-in screen | External screen |
| --- | --- | --- |
| Dual (`[x]`, default) | main menu, and the boot screen | the open menu item (scaled), boot screen, otherwise the static splash |
| Single (`[ ]`) | everything | no picture, backlight off |

In Dual mode a menu item runs on the external screen until it returns (Esc as usual); popups and
keyboard prompts it raises go there too. The menu itself does not take input meanwhile.

## How it works

The panel driver talks to one data bus. `include/ExtMirrorBus.h` replaces it with a bus that sends everything
to the built-in panel unchanged and re-sends the picture to the external panel, scaled up to fill it.
`lib/DisplayDrivers/src/backends/ardgfx.cpp` wraps the bus when `EXT_MIRROR` is defined (4 lines; the library
is vendored in this repo for that reason).

A one-byte route (`extRoute`, `include/ExtRoute.h`) picks which panel(s) receive pixels: built-in,
external or both. It is read at each pixel window, so no extra drawing code or layout is needed for
the external screen: whatever the launcher draws goes to the panel(s) currently routed. Commands and
window coordinates always reach both panels, so each stays initialised and rotated. `extScreen()` in
`src/display.cpp` sets the route around each main-menu action.

Scaling is nearest-neighbour, done row by row as the launcher draws, with no frame buffer (about 1 KB of RAM).
By default the 240x135 picture is stretched to the full 320x240 panel. That is a wider stretch vertically
(1.78x) than horizontally (1.33x), so circles come out slightly tall. For undistorted proportions use
`-DEXT_MIRROR_H=180` (see the env): the picture fills the width and leaves 30 px bars top and bottom.
`-DEXT_MIRROR_W=240 -DEXT_MIRROR_H=135` gives the old unscaled, centred mirror.

The mirror works in landscape (the launcher's rotation 1 and 3). In a portrait rotation the external screen
stops updating.

The external panel shares SCK and MOSI with the SD card (the card's own `sdcardSPI` object, so transactions
are serialised). Only CS, DC, RST and the backlight are its own.

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
| BL | G4 | backlight: follows the brightness setting and turns off with the device. Wired to 3V3 instead, it works but stays lit when the device is turned off |

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
| External screen stays lit after OFF / brightness 0 | Move the BL wire from 3V3 to G4 (`-DEXT_TFT_BL=4`). A backlight wired to 3V3 cannot be switched off by software. |
| Image mirrored or upside down | Uncomment `-DEXT_MADCTL_XOR=0x40` (left/right) or `0x80` (up/down) in the env. This flips the external screen only. |
| Picture looks stretched tall | Intended with the default 320x240. Use `-DEXT_MIRROR_H=180` for the built-in screen's proportions. |
| Colours inverted | The driver sends INVON, as the built-in IPS panel needs. If your module disagrees, tell me and I will add a flag. |
| Garbled picture or noise | Both screens share one SPI speed (default 40 MHz). Uncomment `-DEXT_SPI_SPEED=20000000` in the env. |
| SD card not found | The external screen must not be wired to G39 (SD MISO) or G12 (SD CS). |
| Built-in screen works, external blank | The external panel may not accept the init sequence. Send me the model of the module. |
