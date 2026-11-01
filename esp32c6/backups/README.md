# Factory flash backup — Waveshare ESP32-C6-Touch-LCD-1.9 (MAC a0:85:e3:af:35:bc)
Taken 2026-11-01, full 8 MB, verified against the chip (digest matched). SHA-256 in the .sha256 file.

Restore (puts the board back exactly as shipped):
    ~/.local/share/esptool-venv/bin/esptool --port /dev/ttyACM0 -b 921600 write-flash 0 factory-full-a085e3af35bc-2026-11-01.bin
If the port is missing: hold BOOT, tap RESET, release BOOT, run the command, then press RESET.
