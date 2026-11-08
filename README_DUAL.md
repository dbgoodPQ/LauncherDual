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
| Dual (`[x]`, default) | main menu, and the boot screen | the open menu item, natively at 320x240; otherwise the static splash |
| Single (`[ ]`) | everything | no picture, backlight off |

In Dual mode a menu item runs on the external screen until it returns (Esc as usual); popups and
keyboard prompts it raises go there too. The menu itself does not take input meanwhile.

## How it works

The external panel has its own Arduino_GFX ST7789 driver (240x320, no offsets), built next to the built-in
one in `lib/DisplayDrivers/src/backends/ardgfx.cpp` when `EXT_MIRROR` is defined (the library is vendored in
this repo for that reason). `tft->useExt()` swaps which driver the launcher draws on, and `extScreen()` in
`src/display.cpp` calls it around each main-menu action and updates `tftWidth`/`tftHeight`. Every screen
then lays itself out for the panel it is on, so the external screen is drawn at its real resolution, with
no scaling and no frame buffer (no extra RAM beyond the driver objects).

The external screen uses the same rotation as the built-in one (Config > Orientation).

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
| Colours inverted | The driver sends INVON, as the built-in IPS panel needs. If your module disagrees, tell me and I will add a flag. |
| Garbled picture or noise on the external screen | Uncomment `-DEXT_SPI_SPEED=20000000` in the env (default 40 MHz). |
| SD card not found | The external screen must not be wired to G39 (SD MISO) or G12 (SD CS). |
| Built-in screen works, external blank | The external panel may not accept the init sequence. Send me the model of the module. |
