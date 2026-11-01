#!/usr/bin/env bash
# Install ESP-IDF for ESP32-C6 into ~/esp/esp-idf (tools in ~/.espressif).
# Usage: setup-idf.sh [version]   (default v5.5.6 — stable, well supported by esp_lvgl_port/LVGL 9)
set -euo pipefail
VER="${1:-v5.5.6}"
IDF_DIR="$HOME/esp/esp-idf"

need_gb=5
free_gb=$(df -BG --output=avail "$HOME" | tail -1 | tr -dc '0-9')
if (( free_gb < need_gb )); then
  echo "ERROR: only ${free_gb}G free in \$HOME; ESP-IDF needs ~${need_gb}G (repo+toolchain+builds)." >&2
  exit 1
fi

pkg_ok() { dpkg -s "$1" >/dev/null 2>&1 || dpkg -s "$1t64" >/dev/null 2>&1; }
missing=(); optional=()
# Required for CMake builds of esp32c6
for p in git wget python3 python3-venv cmake ninja-build libusb-1.0-0; do
  pkg_ok "$p" || missing+=("$p")
done
# Listed in Espressif docs but not needed for esp32c6 CMake builds (legacy make / ccache speedup / DFU chips)
for p in flex bison gperf ccache dfu-util libffi-dev libssl-dev; do
  pkg_ok "$p" || optional+=("$p")
done
if ((${#missing[@]})); then
  echo "Missing required apt packages: ${missing[*]}"
  echo "Run:  sudo apt-get install -y ${missing[*]}"
  exit 2
fi
((${#optional[@]})) && echo "note: optional packages not installed (fine for esp32c6): ${optional[*]}"

mkdir -p "$HOME/esp"
if [[ -d "$IDF_DIR/.git" ]]; then
  echo "ESP-IDF already at $IDF_DIR ($(git -C "$IDF_DIR" describe --tags 2>/dev/null)); switching to $VER"
  git -C "$IDF_DIR" fetch --depth 1 origin tag "$VER"
  git -C "$IDF_DIR" checkout "$VER"
  git -C "$IDF_DIR" submodule update --init --recursive --depth 1
else
  git clone -b "$VER" --depth 1 --recursive --shallow-submodules \
    https://github.com/espressif/esp-idf.git "$IDF_DIR"
fi

# esp32c6 only keeps download small (~1.5 GB of tools)
"$IDF_DIR/install.sh" esp32c6
# Drop downloaded archives after extraction to save ~400 MB
rm -rf "$HOME/.espressif/dist"

if ! id -nG "$USER" | grep -qw dialout; then
  echo
  echo "NOTE: add yourself to the dialout group for serial access, then log out/in:"
  echo "  sudo usermod -aG dialout \$USER"
fi
echo
echo "Done. Activate with:  source ~/.claude/skills/esp32c6/scripts/idf-env.sh"
