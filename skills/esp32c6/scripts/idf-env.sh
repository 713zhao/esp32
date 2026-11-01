# Source me:  source ~/.claude/skills/esp32c6/scripts/idf-env.sh
# Activates ESP-IDF quietly and sets ESPPORT to the first Espressif USB device found.
export IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf}"
if [[ ! -f "$IDF_PATH/export.sh" ]]; then
  echo "ESP-IDF not found at $IDF_PATH — run ~/.claude/skills/esp32c6/scripts/setup-idf.sh" >&2
  return 1 2>/dev/null || exit 1
fi
# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" >/dev/null 2>&1 || { echo "export.sh failed; re-run: $IDF_PATH/install.sh esp32c6" >&2; return 1; }

if [[ -z "${ESPPORT:-}" ]]; then
  for d in /dev/serial/by-id/*Espressif* /dev/ttyACM* /dev/ttyUSB*; do
    [[ -e "$d" ]] && { ESPPORT="$(readlink -f "$d")"; break; }
  done
fi
export ESPPORT="${ESPPORT:-}"
[[ -z "$ESPPORT" ]] && echo "warning: no ESP32 serial port found (board unplugged? cable charge-only?)" >&2
true
