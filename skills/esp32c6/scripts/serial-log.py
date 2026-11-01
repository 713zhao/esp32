#!/usr/bin/env python3
"""Non-interactive serial capture for ESP32 (use instead of `idf.py monitor` from scripts/agents).

Usage: serial-log.py [--port /dev/ttyACM0] [--seconds 15] [--no-reset] [--until REGEX]
Resets the chip (via USB-Serial-JTAG RTS/DTR) unless --no-reset, then prints output for N seconds
or until REGEX matches. Requires pyserial (present in the ESP-IDF python env).
"""
import argparse, glob, re, sys, time

try:
    import serial
except ImportError:
    sys.exit("pyserial missing: source ~/.claude/skills/esp32c6/scripts/idf-env.sh first")


def default_port():
    for pat in ("/dev/serial/by-id/*Espressif*", "/dev/ttyACM*", "/dev/ttyUSB*"):
        hits = sorted(glob.glob(pat))
        if hits:
            return hits[0]
    return None


ap = argparse.ArgumentParser()
ap.add_argument("--port", default=None)
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--seconds", type=float, default=15)
ap.add_argument("--no-reset", action="store_true")
ap.add_argument("--until", default=None, help="stop early when a line matches this regex")
a = ap.parse_args()

port = a.port or default_port()
if not port:
    sys.exit("No serial port found")

# Line-order matters on ESP32 USB-Serial-JTAG: RTS high while DTR drops = chip reset.
# pyserial's open() drops DTR before RTS, so use dsrdtr=True to skip its DTR write, then
# lower RTS first and DTR second -> opening the port no longer resets the board.
s = serial.Serial()
s.port, s.baudrate, s.timeout = port, a.baud, 0.2
s.dsrdtr = True
s.open()
s.rts = False
s.dtr = False
if not a.no_reset:
    # EN low pulse via RTS, as esptool's hard reset does for USB-Serial-JTAG
    s.rts = True
    time.sleep(0.1)
    s.rts = False
    time.sleep(0.05)

pat = re.compile(a.until) if a.until else None
end = time.time() + a.seconds
buf = b""
while time.time() < end:
    try:
        chunk = s.read(1024)
    except serial.SerialException:
        # USB-JTAG re-enumerates on reset; reopen
        s.close(); time.sleep(0.5)
        try:
            s.open()
        except serial.SerialException:
            continue
        continue
    if not chunk:
        continue
    buf += chunk
    while b"\n" in buf:
        line, buf = buf.split(b"\n", 1)
        txt = re.sub(r"\x1b\[[0-9;]*m", "", line.decode(errors="replace").rstrip("\r"))
        print(txt, flush=True)
        if pat and pat.search(txt):
            end = 0
            break
if buf:
    print(buf.decode(errors="replace"))
s.close()
