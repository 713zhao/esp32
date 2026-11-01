# Debugging ESP32-C6 with ESP-IDF

Prefix commands with `source ~/.claude/skills/esp32c6/scripts/idf-env.sh &&`.

## 1. Serial logs
- Non-interactive capture (Claude): `python3 ~/.claude/skills/esp32c6/scripts/serial-log.py --seconds 20 [--no-reset] [--port /dev/ttyACM0]`
- Interactive (user, separate terminal window — not `!`): `idf.py -p /dev/ttyACM0 monitor` — Ctrl+] quit, Ctrl+T Ctrl+R reset, Ctrl+T Ctrl+F build+flash.
- Log level: menuconfig → Component config → Log → Default log verbosity; at runtime
  `esp_log_level_set("*", ESP_LOG_WARN); esp_log_level_set("app", ESP_LOG_DEBUG);`
- Boot-loop? Look for `rst:0x..` reason: `0xc (SW_CPU)` = panic/abort reboot, `0xf (TG1WDT)`/`RTCWDT` = watchdog,
  `brownout` = power (bad cable/USB hub).

## 2. Panics / Guru Meditation
Typical: `Guru Meditation Error: Core 0 panic'ed (Load access fault)` + register dump + `Backtrace:` / `Stack memory:`.
- `idf.py monitor` decodes automatically.
- Manual: `riscv32-esp-elf-addr2line -pfiaC -e build/<project>.elf 0x4200abcd 0x4200ef01 ...`
- RISC-V backtraces need frame pointers for accuracy: menuconfig → Component config → ESP System Settings →
  "Enable backtracing using frame pointer" (`CONFIG_ESP_SYSTEM_USE_FRAME_POINTER=y`).
- Common causes: NULL deref (`MTVAL` = 0x0..), stack overflow (`***ERROR*** A stack overflow in task X`
  → increase that task's stack), LVGL call without lock, using freed memory.
- Task watchdog (`task_wdt: Task watchdog got triggered`) → a task loops without `vTaskDelay`;
  the log names the starving task.

## 3. Core dumps (post-mortem)
menuconfig → Component config → Core dump → Data destination = Flash, format ELF. Needs a
`coredump` partition (add `coredump, data, coredump, , 64K` to partitions.csv). After a crash:
`idf.py coredump-info` (backtrace + tasks) or `idf.py coredump-debug` (GDB on the dump).

## 4. Live JTAG debugging (built-in USB-JTAG, no probe needed)
One-time Linux setup (user runs, sudo):
```
! sudo cp ~/.espressif/tools/openocd-esp32/*/openocd-esp32/share/openocd/contrib/60-openocd.rules /etc/udev/rules.d/ && sudo udevadm control --reload && sudo udevadm trigger
```
Then:
- Claude: start `idf.py openocd` with `run_in_background`, then batch GDB:
  `riscv32-esp-elf-gdb -batch -ex "target extended-remote :3333" -ex "mon reset halt" -ex "b app_main" -ex "c" -ex "bt" build/<project>.elf`
- User interactive (separate terminal): `idf.py openocd gdbtui`
- Build with `-Og` for debuggable code: menuconfig → Compiler options → Optimization = Debug.
- Only 2 HW breakpoints / 2 watchpoints on C6. Flash breakpoints use HW slots.
- Monitor and OpenOCD share the USB device fine (separate interfaces), but flashing while OpenOCD runs fails — stop OpenOCD first.

## 5. Memory & performance
- `heap_caps_print_heap_info(MALLOC_CAP_DEFAULT)`, `heap_caps_get_minimum_free_size(...)`.
- Heap corruption: menuconfig → Heap memory debugging → Comprehensive; then `heap_caps_check_integrity_all(true)`.
- Task stats: enable `CONFIG_FREERTOS_USE_TRACE_FACILITY` + `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`, call `vTaskList()` / `vTaskGetRunTimeStats()`.
- `idf.py size`, `idf.py size-components` — binary bloat (LVGL fonts/demos are the usual culprits).
- LVGL: `CONFIG_LV_USE_SYSMON=y`, `CONFIG_LV_USE_PERF_MONITOR=y`, `CONFIG_LV_USE_MEM_MONITOR=y`.

## 6. Display / touch specific
| Symptom | Fix |
|---|---|
| Black screen, logs fine | backlight polarity (GPIO15 active LOW), or `esp_lcd_panel_disp_on_off(panel,true)` missing |
| Garbage stripe / image shifted 35 px | gap wrong: portrait `(35,0)`, landscape `(0,35)` |
| Colours inverted (white↔black) | toggle `esp_lcd_panel_invert_color` |
| Red/blue swapped | `rgb_ele_order` RGB↔BGR |
| Colours noisy/pastel | byte swap: `flags.swap_bytes` in lvgl_port display cfg |
| Touch dead / `i2c ... NACK` | addr 0x15, SDA18/SCL8; CST816 asleep → write 0xFE=1; touch only on Touch SKU |
| Touch axes wrong after rotation | adjust `esp_lcd_touch` flags swap_xy / mirror_x / mirror_y |
| I2C scan | `i2c_master_probe(bus, addr, 50)` loop 0x08..0x77 — expect 0x15 (touch), 0x6B (IMU) |

## 7. WiFi
- `wifi: ... reason=201` NO_AP_FOUND → wrong SSID or 5 GHz network. `reason=15/204` 4-way handshake → wrong password.
  `reason=2/202` auth expire/fail → WPA mode mismatch; lower `threshold.authmode` in the template's `main/wifi.c`
  (default `WIFI_AUTH_WPA2_PSK`; use `WIFI_AUTH_WPA_PSK` for old routers, `WIFI_AUTH_OPEN` for open networks).
- Print RSSI: `esp_wifi_sta_get_ap_info(&ap); ap.rssi`.
- Erase saved WiFi state: `idf.py erase-flash` or `nvs_flash_erase()`.
