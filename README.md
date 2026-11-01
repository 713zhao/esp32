# ESP32 projects

## esp32c6/my_ui — LAN chat device (Waveshare ESP32-C6-Touch-LCD-1.9)
ESP-IDF 5.5 + LVGL 9 app for the 1.9" 320x170 touch screen:
chat bubbles, quick-reply buttons, on-screen keyboard, RGB LED notification,
and an HTTP/JSON LAN protocol (`espchat/1`, mDNS `esp32-chat.local`). See
[`esp32c6/my_ui/PROTOCOL.md`](esp32c6/my_ui/PROTOCOL.md).

```bash
cd esp32c6/my_ui
. ~/esp/esp-idf/export.sh
idf.py set-target esp32c6
idf.py menuconfig            # App WiFi -> SSID / password (stored in sdkconfig, git-ignored)
idf.py -p /dev/ttyACM0 build flash monitor
python3 tools/espchat.py send "Hello" --replies Hi Bye
```

## skills/esp32c6 — Claude Code skill
Teaches Claude Code how to operate this board: toolchain setup, port detection, build/flash,
non-interactive serial logs, WiFi config, LVGL UI recipes, debugging (backtraces, core dumps,
GDB over USB-JTAG), backup/restore, plus two project templates (basic touch UI, LAN chat).

Install (symlink so `git pull` updates it):
```bash
ln -s "$PWD/skills/esp32c6" ~/.claude/skills/esp32c6
~/.claude/skills/esp32c6/scripts/setup-idf.sh      # installs ESP-IDF v5.5.6 for esp32c6
```
