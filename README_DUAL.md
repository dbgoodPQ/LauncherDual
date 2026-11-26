# LauncherDual — technical notes

Details behind [README.md](README.md): how the second screen is driven, the exact wiring, and what to try
when something looks wrong. Envs: `m5stack-cardputer-dual` (Waveshare 2inch ST7789 on SPI) and
`m5stack-cardputer-screendriver` (ScreenDriver ILI9341 behind a XIAO ESP32-C6, see
[below](#screendriver-build)). Both have the same Dual/Single modes.

## Screen modes

| Mode | Built-in screen | External screen |
| --- | --- | --- |
| **Dual** `[x]` (default) | boot screen, main menu, popups raised by the menu itself | the open menu item (and everything it draws), at 320x240; the static splash otherwise |
| **Single** `[ ]` | everything | no picture, backlight off |

Toggled by Settings → `Dual screen`. Saved in NVS and in `config.conf` as `dualScreen` (bool). Switching
takes effect at once: Settings itself moves to the screen that now shows open items.

Each menu item is a blocking loop, so while one is open on the external screen the main menu does not
run. Closing the item (Esc) returns control to the menu.

## How it works

- **Two drivers, one API.** `lib/DisplayDrivers/src/backends/ardgfx.cpp` builds, next to the built-in
  panel's driver, a second Arduino_GFX `Arduino_ST7789` (240x320, IPS, no offsets) for the external panel.
  The library is vendored in this repo for that reason. All of it is under `#if defined(EXT_MIRROR)`, so
  other boards are unchanged.
- **Switching.** `tft->useExt(bool)` swaps which driver the global `tft` draws on, and hands over the
  rotation, text size and text colours. `extScreen(bool item)` in `src/display.cpp` calls it around each
  main-menu action (`RUN_ITEM` in `src/main.cpp`) and updates `tftWidth`/`tftHeight`, so every screen
  lays itself out for the panel it is on. It also draws the splash (`initDisplay(true)`) on the external
  panel after boot and each time an item closes.
- **Memory.** No frame buffer and no scaling: the external panel costs only the driver and bus objects.
- **Backlight.** `EXT_TFT_BL` follows the brightness setting in Dual mode and is off in Single mode
  (`_setBrightness()` in `src/settings.cpp`). It is held low through deep sleep when the device is
  turned off.
- **Rotation.** The external panel takes the same rotation as the built-in one (Settings → Orientation).
- **Shared SPI.** The external panel shares SCK and MOSI with the SD card on the card's own `sdcardSPI`
  object, so transactions are serialised. Only CS, DC, RST and the backlight are its own.

## Text editor (implementation)

`src/text_editor.cpp`, compiled only when `ARDUINO_M5STACK_CARDPUTER` is defined, and hooked into the SD
file menu in `src/sd_functions.cpp`. Usage is in [README.md](README.md#text-editor).

- The file is read into one heap buffer (no `String` per line), capped at 32 KB; the buffer grows in
  1 KB steps while typing. Positions are byte offsets; `\n` and `\r\n` endings are kept as found.
- Only the changed rows are redrawn; scrolling or a new/joined line redraws the visible page.
- Saving writes `<file>.tmp`, removes the original and renames. `.json` and `config.conf` are first
  parsed by ArduinoJson with an empty filter, which validates the syntax without storing anything.
- Keyboard: the ADV (TCA8418) driver in `boards/m5stack-cardputer/interface.cpp` now attaches the held
  Fn and Ctrl keys to each key event, as the original Cardputer keyboard library already did, and marks
  the real Tab key in `hid_keys` (it shares its code with `+`). Holding an Fn-arrow repeats using the
  driver's navigation pulses.

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
| BL | G4 | backlight: follows the brightness setting and turns off with the device. Wired to 3V3 instead, it works but stays lit in Single mode and when the device is off |

The GPIOs match the three other Cardputer ADV external-display projects I found
(`zeromem0/cardputer_adv`, `moes-bobandy/botcorps-adv`, `purplxhazee/Purplx-`). Verify the header pinout
against M5Stack's schematic before powering up. The pin numbers are `EXT_TFT_*` in
`boards/m5stack-cardputer/platformio.ini`.

## ScreenDriver build

`m5stack-cardputer-screendriver` drives the [ScreenDriver](https://github.com/dbgoodPQ/ScreenDriver)
screen instead: a 2.8" ILI9341 (320x240) rendered by a XIAO ESP32-C6, which the Cardputer feeds with
drawing commands over a 5 Mbaud UART. The library is the `lib_ext/ScreenDriver` submodule.

- **Wiring:** G13 (TX) → C6 D7, G15 (RX) ← C6 D6, G4 (RTS) → C6 D5, G6 (CTS) ← C6 D9, GND, and power
  as described in ScreenDriver's README. It uses UART1 and no SPI pins, so it does not share the SD bus.
- **Driver:** `Arduino_RemoteGFX` in `lib/DisplayDrivers/src/backends/ardgfx.cpp` (under
  `EXT_SCREENDRIVER`) is the external Arduino_GFX driver that `useExt()` swaps in. Shapes become C6
  commands, and text is drawn by the C6 with the same 6x8 font. The C6 shows nothing until a frame ends,
  so a task ends the open frame every 25 ms.
- **Without the screen:** the launcher boots normally (startup waits up to 1.5 s for the C6), and the
  library connects by itself when the C6 appears.
- **Backlight:** follows the brightness setting in Dual mode, off in Single mode and when the device
  turns off.
- **Rotation:** the C6 drives the panel in landscape only. Rotations 1 and 3 work; changing between
  them restarts the C6.
- **XIAO reset while an item is open:** the external screen stays blank until the item redraws (usually
  the next key press). There is no RAM for a copy of the screen.
- **Partition table:** `support_files/custom_8Mb2.csv`, launcher partition 0x170000 instead of 0x150000
  (RemoteDisplay does not fit otherwise). Installed apps get 6.44 MB instead of 6.56 MB. Moving a
  Cardputer between this build and the Waveshare one needs a full esptool flash (partitions included),
  not an OTA or SD update.

## Build and flash

```
git clone --recursive https://github.com/dbgoodPQ/LauncherDual
cd LauncherDual
pio run -e m5stack-cardputer-dual -e m5stack-cardputer-screendriver
```

The merged images are `Launcher-m5stack-cardputer-dual.bin` and
`Launcher-m5stack-cardputer-screendriver.bin` in the project root; the **Build Cardputer dual** GitHub
Action builds the same files (Actions tab, download the artifacts).

Download mode: hold G0 and press RST. Then (`erase_flash` first when switching between the two builds):

```
esptool.py --chip esp32s3 write_flash 0x0 Launcher-m5stack-cardputer-dual.bin
esptool.py --chip esp32s3 write_flash 0x0 Launcher-m5stack-cardputer-screendriver.bin
```

(or flash the file at offset 0x0 from any web flasher that accepts a custom merged binary.)

## If something looks wrong

| Symptom | Try |
| --- | --- |
| External screen stays white or black | Check CS/DC/RST wiring and that the module is at 3.3V. Check `Dual screen` is `[x]` in Settings. |
| External screen stays lit in Single mode or when off | Move the BL wire from 3V3 to G4 (`-DEXT_TFT_BL=4`). A backlight wired to 3V3 cannot be switched off by software. |
| Colours inverted | The driver sends INVON for an IPS panel. If your module disagrees, open an issue with its model. |
| Garbled picture or noise on the external screen | Uncomment `-DEXT_SPI_SPEED=20000000` in the env (default 40 MHz). |
| SD card not found | The external screen must not be wired to G39 (SD MISO) or G12 (SD CS). |
| A screen is laid out wrongly at 320x240 | Open an issue naming the menu item; it probably assumes the built-in screen's size. |
