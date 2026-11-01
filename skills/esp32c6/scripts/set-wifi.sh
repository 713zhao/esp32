#!/usr/bin/env bash
# Set WiFi credentials in a template project's sdkconfig (not committed to git).
# Usage: set-wifi.sh <project_dir> <ssid> <password>
set -euo pipefail
dir="${1:?project dir}"; ssid="${2:?ssid}"; pass="${3-}"
cfg="$dir/sdkconfig"
[[ -f "$cfg" ]] || { echo "No $cfg yet — run 'idf.py set-target esp32c6' (or build) first." >&2; exit 1; }
python3 -I - "$cfg" "$ssid" "$pass" <<'PY'
import sys, re
path, ssid, pw = sys.argv[1:4]
esc = lambda s: s.replace('\\', '\\\\').replace('"', '\\"')
text = open(path).read()
for key, val in (("CONFIG_APP_WIFI_SSID", ssid), ("CONFIG_APP_WIFI_PASSWORD", pw)):
    line = f'{key}="{esc(val)}"'
    text, n = re.subn(rf'^{key}=.*$', lambda m: line, text, flags=re.M)
    if n == 0:
        text += line + "\n"
open(path, "w").write(text)
print(f"WiFi SSID set to '{ssid}' in {path} (password hidden). Rebuild + flash to apply.")
PY
