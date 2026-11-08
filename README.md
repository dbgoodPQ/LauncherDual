# LauncherDual

A fork of [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) for the **M5Stack Cardputer ADV**
that adds a second screen: an external **Waveshare 2inch LCD module (ST7789V, 240x320)**.

The menu stays on the Cardputer's built-in screen, and whatever you open from it (SD browser, OTA, WebUI,
Partition Manager, Settings, apps) is shown on the external screen at its full 320x240 resolution.
Everything else is stock Launcher; for features, usage and other devices, see the
[upstream project](https://github.com/bmorcelli/Launcher).

Env: `m5stack-cardputer-dual`. Builds come from the **Build Cardputer dual** GitHub Action.

## Screen modes

Choose in **Settings → `Dual screen`**. The change applies immediately, with no reboot, and is saved
with the other settings.

| Mode | Built-in screen | External screen |
| --- | --- | --- |
| **Dual** `[x]` (default) | boot screen and main menu | the menu item you open; the Launcher splash when nothing is open |
| **Single** `[ ]` | everything | off (no picture, backlight off) |

How Dual mode works in use:

- Select an item on the main menu: it opens on the external screen and takes the keyboard until you
  leave it. Its popups and text prompts appear there too.
- Press **Esc** inside the item as usual to go back. When it closes, the keyboard returns to the menu
  and the external screen shows the splash again.
- On the main menu, Esc does nothing: the built-in screen always shows the menu.

The external screen is drawn natively, so text and icons are as sharp as on the built-in screen and
lists use the bigger panel. It follows the brightness setting and the orientation you set in Settings.

## Text editor

In the SD browser, open the menu on a `.json`, `.conf`, `.txt`, `.ini`, `.cfg`, `.csv`, `.md` or `.log`
file and choose **Edit file**. Files up to 32 KB can be edited. In Dual mode the editor opens on the
external screen, in Single mode on the built-in one. Text is shown at size 2 on the external screen
(26 columns × 14 lines) and size 1 on the built-in one (40 × 15); Ctrl + `=` switches between the two
sizes (53 × 28 at size 1 on the external screen), and the choice is kept for each screen until reboot.

The top line shows the file name (`*` when modified) and the cursor's line:column. Long lines scroll
sideways.

| Key | Action |
| --- | --- |
| any character | insert, including `;` `.` `,` `/` |
| Fn + `;` `.` `,` `/` | cursor up / down / left / right (hold to repeat) |
| Ctrl + `;` `.` | page up / page down |
| Ctrl + `,` `/` | line start / line end |
| Enter | new line, keeping the indentation |
| Backspace | delete before the cursor |
| Tab | insert 2 spaces |
| Fn + S | save |
| Fn + Q or `` ` `` | quit; if modified: Save / Don't save / Cancel |
| Ctrl + `=` | zoom: text size 1 / 2 |
| Ctrl + H | key help |

Saving writes `<file>.tmp` first and then replaces the file, so a failed write leaves the original intact.
`.json` files and `config.conf` are checked first: invalid JSON asks before saving. After saving
`config.conf`, the launcher reloads its settings so your edits are kept. Line endings (LF or CRLF) are
kept as found. Not included, to stay small: undo, copy/paste, search. A backtick cannot be typed
(it quits). Cardputer builds only.

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

Pins are set as `EXT_TFT_*` in `boards/m5stack-cardputer/platformio.ini`.

## Build

```
git clone --recursive https://github.com/dbgoodPQ/LauncherDual
cd LauncherDual
pio run -e m5stack-cardputer-dual
```

Output: `Launcher-m5stack-cardputer-dual.bin` in the project root. Or run the **Build Cardputer dual**
Action (Actions tab) and download the artifact.

## Flash

Enter download mode (hold G0, press RST), then:

```
esptool.py --chip esp32s3 write_flash 0x0 Launcher-m5stack-cardputer-dual.bin
```

## Upgrading from the external-only build

The earlier "external screen" mode (switched by holding G0 for 3 s or `[Internal]/[External] Screen` in
Config) is replaced by the Dual/Single modes above. G0 is a normal Select button again. No settings need
to be reset.

## Options

| Flag | Effect |
| --- | --- |
| `-DEXT_SPI_SPEED=20000000` | Lower the external screen's SPI speed if its picture is garbled (default 40 MHz) |

How it works and troubleshooting: [README_DUAL.md](README_DUAL.md).

## Credits

All credit for Launcher goes to [bmorcelli](https://github.com/bmorcelli) and the upstream contributors.
Licensed under the MIT License — see [LICENSE](LICENSE).
