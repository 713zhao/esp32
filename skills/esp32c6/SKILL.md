---
name: esp32c6
description: Operate the Waveshare ESP32-C6-Touch-LCD-1.9 board (and ESP32-C6 boards generally) with ESP-IDF — install/activate the toolchain, find the serial port, create a project from the LCD+touch+WiFi LVGL template, build, flash, serial monitor, connect to WiFi, build custom LVGL UIs, debug (panic backtraces, core dumps, GDB over built-in USB-JTAG), erase, back up, and restore factory firmware. Use whenever the user mentions ESP32, ESP32-C6, idf.py, esptool, flashing, the touch LCD board, LVGL on the board, or a serial/monitor/JTAG problem with it.
---

# ESP32-C6 Touch LCD 1.9" — operating guide

Board facts, pins and quirks: `references/board.md` (read before touching any GPIO/display code).
Debugging deep-dive: `references/debugging.md`. LVGL UI recipes: `references/lvgl-ui.md`.

Skill dir below = `~/.claude/skills/esp32c6`.

## 0. Environment (every shell)

Interactive tools (`menuconfig`, `monitor`, `gdbtui`) need a real terminal: never suggest the `!` prefix for them —
tell the user to open a separate terminal window.


ESP-IDF lives in `~/esp/esp-idf` (installed by `scripts/setup-idf.sh`). Each Bash call is a fresh
shell, so **prefix every idf.py command** with the env script:

```bash
source ~/.claude/skills/esp32c6/scripts/idf-env.sh && idf.py build
```

`idf-env.sh` sources `export.sh` quietly and exports `ESPPORT` (auto-detected board port) if found.
If IDF is not installed → run `scripts/setup-idf.sh` (needs ~5 GB free disk; check `df -h ~` first
and tell the user if space is short). If the user is not in the `dialout` group, they must run
`! sudo usermod -aG dialout $USER` and log out/in (can't be done non-interactively).

## 1. Find / identify the board

```bash
~/.claude/skills/esp32c6/scripts/find-port.sh     # lists ports, marks Espressif USB-JTAG (303a:1001)
source ~/.claude/skills/esp32c6/scripts/idf-env.sh && esptool.py --chip esp32c6 -p $ESPPORT flash_id
```
The board uses the C6's native USB-Serial-JTAG → shows as `/dev/ttyACM0`, VID:PID `303a:1001`.
`flash_id` prints chip revision, MAC and flash size (expect 8MB; if different, fix
`CONFIG_ESPTOOLPY_FLASHSIZE_*` in the project's sdkconfig.defaults).

## 2. New project from template

```bash
cp -r ~/.claude/skills/esp32c6/templates/lcd19_touch_app <dest>/<name>
cd <dest>/<name> && source ~/.claude/skills/esp32c6/scripts/idf-env.sh && idf.py set-target esp32c6
```
The template gives: ST7789V2 display (170x320) + CST816 touch via `esp_lvgl_port` + LVGL 9,
backlight PWM, WiFi station with auto-reconnect, SNTP clock, and a demo screen (status, IP,
clock, button, slider controlling brightness). Put UI code in `main/ui.c`; keep `main/board.c`
for hardware. First `idf.py build` downloads managed components (needs internet).

Second template **`templates/lcd19_chat_app`** (landscape): LAN chat device — HTTP/JSON protocol
`espchat/1` on port 80 + mDNS `esp32-chat.local` (spec in its `PROTOCOL.md`), built-in web chat page at `/`,
chat bubbles, server-chosen quick-reply buttons, on-screen keyboard overlay, RGB LED flash on new message,
and `tools/espchat.py` (stdlib CLI: send / listen / chat / `send --wait` to get the answer back).
LVGL rule learned here: never delete the object whose event is running (button bar rebuild,
keyboard close) — use `lv_obj_delete_async` / `lv_async_call`. Also never size a child with `lv_pct()`
inside an `LV_SIZE_CONTENT` parent (resolves to 0 → bubbles collapse); measure with `lv_text_get_size` and set px.

## 3. WiFi credentials

Never hard-code credentials in source or commit them. They live in the project's `sdkconfig`
(git-ignored by the template's `.gitignore`), under Kconfig menu **App WiFi**:
- User, interactively: in a **separate terminal window** (not `!` — it has no TTY, curses UIs don't show):
  `cd <project> && source ~/.claude/skills/esp32c6/scripts/idf-env.sh && idf.py menuconfig` → App WiFi → save (S), quit (Q).
- Claude, non-interactively: after `idf.py set-target`/first build created `sdkconfig`, run
  ```bash
  ~/.claude/skills/esp32c6/scripts/set-wifi.sh <project_dir> "<ssid>" "<password>"
  ```
  then rebuild + flash. (It edits `CONFIG_APP_WIFI_SSID/PASSWORD` in `sdkconfig` in place.)

Ask the user for SSID/password rather than guessing; don't echo the password back in summaries.
ESP32-C6 is **2.4 GHz only** (WiFi 6 / 802.11ax at 2.4 GHz). 5 GHz-only SSIDs will never connect.
WPA3-only networks need `CONFIG_ESP_WIFI_ENABLE_WPA3_SAE=y` (default on).

## 4. Build / flash / monitor

```bash
S=~/.claude/skills/esp32c6/scripts/idf-env.sh
source $S && idf.py build                       # compile
source $S && idf.py -p $ESPPORT flash           # flash (auto-reset into bootloader)
source $S && idf.py size                        # check app vs 3MB partition
```
`idf.py monitor` is interactive (needs a TTY) — **don't run it via Bash tool**; instead either
tell the user to run `idf.py -p /dev/ttyACM0 monitor` in a **separate terminal window** (Ctrl+] exits; `!` has no TTY) or capture logs
non-interactively with `scripts/serial-log.py` (resets board, prints N seconds of output):

```bash
source $S && python3 ~/.claude/skills/esp32c6/scripts/serial-log.py --seconds 15
```
serial-log.py output can be decoded for backtraces: pipe `Backtrace:` addresses through
`riscv32-esp-elf-addr2line -pfiaC -e build/<app>.elf <addrs>` (see debugging.md).

## 5. When flashing fails

1. `Failed to connect` / port busy → close any other monitor (`fuser /dev/ttyACM0`), retry.
2. Port vanished → the running app crashed USB or deep-sleeps. Manual download mode:
   **hold BOOT, tap RESET, release BOOT**, then flash. After flashing press RESET (in manual
   download mode the chip doesn't auto-reboot; "waiting for download" in log = still in ROM).
3. `Permission denied` → dialout group (section 0).
4. Wrong flash size warnings → see section 1.

## 6. Erase / back up / restore

```bash
source $S && esptool.py --chip esp32c6 -p $ESPPORT read_flash 0 0x800000 backup-$(date +%F).bin  # full 8MB backup (~2 min)
source $S && idf.py -p $ESPPORT erase-flash                                                     # wipe all (incl. WiFi creds in NVS)
source $S && esptool.py --chip esp32c6 -p $ESPPORT write_flash 0x0 ~/.claude/skills/esp32c6/assets/ESP32-C6-LCD-1.9-factory.bin  # Waveshare factory demo
```
Offer a backup before the user's **first** flash — the shipped firmware is otherwise only
recoverable from the bundled factory bin. Never erase/overwrite without the user's go-ahead.

## 7. Debugging (summary — full detail in references/debugging.md)

- Logs: `ESP_LOGI/W/E(TAG, ...)`; raise verbosity per tag with `esp_log_level_set("tag", ESP_LOG_DEBUG)`.
- Crash (`Guru Meditation Error`) → monitor auto-decodes backtrace; else addr2line on the ELF.
- Core dump to flash: enable in menuconfig, then `idf.py coredump-info` / `coredump-debug`.
- Live GDB over the built-in USB-JTAG (no extra hardware):
  `idf.py openocd` (background) + `idf.py gdb` — or `idf.py openocd gdbtui` interactively for the user.
  Linux needs the udev rule from `$IDF_TOOLS_PATH/tools/openocd-esp32/*/openocd-esp32/share/openocd/contrib/60-openocd.rules`.
- Heap/stack: `heap_caps_get_free_size(MALLOC_CAP_DEFAULT)`, `uxTaskGetStackHighWaterMark(NULL)`; stack-overflow panics name the task.
- LVGL perf: `CONFIG_LV_USE_SYSMON=y` + `CONFIG_LV_USE_PERF_MONITOR=y` shows FPS/CPU on screen.

## 8. Building UIs

Read `references/lvgl-ui.md`. Rules that matter on this board:
- All LVGL calls from non-LVGL tasks (WiFi events, timers, sensors) **must** be wrapped in
  `lvgl_port_lock(0)` / `lvgl_port_unlock()`. Unlocked calls = random crashes.
- Screen is 170 px wide portrait; design for that or switch template to landscape (`BOARD_LANDSCAPE` in board.h).
- Only fonts enabled in sdkconfig (`CONFIG_LV_FONT_MONTSERRAT_xx`) exist; add more there.
- For visual design, SquareLine Studio or EEZ Studio can export LVGL 9 C code into `main/ui/`.

## 9. Verify changes

After flashing, run `serial-log.py --seconds 10` and check for `app_main` progress, `wifi: got ip`,
no `E (` errors and no panic. Ask the user to confirm what the screen shows — Claude can't see it.
