# Waveshare ESP32-C6-Touch-LCD-1.9 — board reference

Source: Waveshare wiki `ESP32-C6-LCD-1.9`, demo package `ESP32-C6-LCD-1.9-Demo-V2.zip`, and
`ESP32-C6-Touch-LCD-1.9-Schematic.pdf` (pins verified against demo code + schematic net names).

## Core
| Item | Value |
|---|---|
| SoC | ESP32-C6 (RISC-V 160 MHz HP core + 20 MHz LP core), 512 KB HP SRAM |
| Flash | 8 MB (verify with `esptool.py flash_id`) — no PSRAM |
| Radio | WiFi 6 2.4 GHz, BLE 5, 802.15.4 (Thread/Zigbee) |
| USB | Type-C → native USB-Serial-JTAG (`/dev/ttyACM*`, 303a:1001). Flash + console + JTAG over one cable |
| Buttons | BOOT = GPIO9 (active low, usable as user button after boot), RESET = EN |

## Display — ST7789V2, 1.9", 170x320 IPS, SPI
| Signal | GPIO |
|---|---|
| MOSI (DIN) | 4 |
| SCLK | 5 |
| DC | 6 |
| CS | 7 |
| RST | 14 |
| Backlight | 15 — **active LOW** (P-MOSFET). PWM with inverted output |

- SPI host SPI2, 20–40 MHz pclk (Waveshare uses 20 MHz; 40 MHz usually works).
- Colour inversion **on** (`esp_lcd_panel_invert_color(panel, true)`), RGB order, 16-bit, bytes swapped for LVGL.
- Panel RAM is 240 wide; visible 170 columns start at **x offset 35** in portrait
  (`esp_lcd_panel_set_gap(panel, 35, 0)`); in landscape (swap_xy) the gap becomes `(0, 35)`.
- Waveshare's vendor init (gamma/voltage) is replayed after the stock ST7789 init in the template.

## Touch — CST816 (CST816D/S), I2C
| Signal | GPIO |
|---|---|
| SDA | 18 (shared I2C bus with IMU) |
| SCL | 8 |
| INT | not connected by default (R33 → IO21, NC) |
| RST | not connected by default (R28 → IO22, NC) |

- I2C address **0x15**, polled (no INT). Coordinates native portrait 170x320.
- Chip auto-sleeps after idle and then NACKs → write reg `0xFE = 0x01` (disable auto-sleep) at init.
- Only the "Touch" SKU has the CST816; the plain ESP32-C6-LCD-1.9 has none.

## Other peripherals
| Peripheral | Pins / notes |
|---|---|
| QMI8658 6-axis IMU | I2C SDA 18 / SCL 8, addr 0x6B (0x6A alt) |
| TF (microSD) card | SPI shared with LCD: MOSI 4, SCLK 5, **MISO 19**, CS 20 |
| WS2812 RGB LED | GPIO3 (2 LEDs in Waveshare code), use `espressif/led_strip` |
| Battery voltage | GPIO0 = ADC1_CH0 (net BAT_ADC) via resistor divider — check ratio in schematic; wiki shows ~1900 raw on USB power |
| Li-ion header | onboard charger |
| Header GPIOs | 40-pin Pico-style header; GPIOs not used above (e.g. 1, 2, 21, 22, 23, UART0 16/17) — confirm on schematic before use. Avoid 12/13 (USB) and 9 (strap) |

## Not on this board / chip limits
- **No mic, speaker, audio codec** (schematic checked). Add I2S parts on header GPIOs: INMP441 mic, MAX98357A amp.
- **No Bluetooth Classic** → no A2DP → cannot stream to normal Bluetooth speakers/headphones. BLE only
  (BLE HID keyboards possible via NimBLE HID host). LE Audio not supported on C6 in ESP-IDF.
- **No USB host** — the USB-C port is device-only (USB-Serial-JTAG). No USB keyboards/drives.

## Strapping / gotchas
- GPIO8, GPIO9, GPIO15 are strapping pins. 15 is the backlight; 9 is BOOT — don't drive 9 low at reset.
- GPIO12/13 are USB D-/D+; reconfiguring them kills the USB console/flashing (recover with BOOT+RESET).
- No PSRAM: keep LVGL draw buffers ≤ ~1/4 screen (170*80*2 B ≈ 27 KB each, DMA-capable internal RAM).
- WiFi + LVGL + BLE together are tight on RAM; disable BT (`CONFIG_BT_ENABLED=n`) unless needed.
- Waveshare note: their *factory demo's* WiFi scan was written for IDF 5.1; normal STA code works on 5.3+.

## Waveshare resources
- Wiki: https://www.waveshare.com/wiki/ESP32-C6-LCD-1.9
- Demos: https://files.waveshare.com/wiki/ESP32-C6-LCD-1.9/ESP32-C6-LCD-1.9-Demo-V2.zip
- Schematic: https://files.waveshare.com/wiki/ESP32-C6-LCD-1.9/ESP32-C6-Touch-LCD-1.9-Schematic.pdf
- Factory firmware (bundled): `assets/ESP32-C6-LCD-1.9-factory.bin` → flash at 0x0
