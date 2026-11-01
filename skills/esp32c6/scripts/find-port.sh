#!/usr/bin/env bash
# List candidate serial ports and flag Espressif native USB (303a:1001).
found=0
for d in /dev/ttyACM* /dev/ttyUSB*; do
  [[ -e "$d" ]] || continue
  found=1
  info=$(udevadm info -q property -n "$d" 2>/dev/null)
  vid=$(sed -n 's/^ID_VENDOR_ID=//p' <<<"$info"); pid=$(sed -n 's/^ID_MODEL_ID=//p' <<<"$info")
  model=$(sed -n 's/^ID_MODEL=//p' <<<"$info"); serial=$(sed -n 's/^ID_SERIAL_SHORT=//p' <<<"$info")
  tag=""
  [[ "$vid:$pid" == "303a:1001" ]] && tag="  <-- ESP32 USB-Serial-JTAG (serial = MAC $serial)"
  perm=$( [[ -r "$d" && -w "$d" ]] && echo rw || echo "NO ACCESS (dialout group?)")
  echo "$d  $vid:$pid  $model  [$perm]$tag"
done
if (( ! found )); then
  echo "No serial ports. Check: board plugged in, data-capable USB-C cable, try BOOT+RESET download mode."
  echo "lsusb Espressif entries:"; lsusb | grep -i 303a || echo "  (none)"
  exit 1
fi
