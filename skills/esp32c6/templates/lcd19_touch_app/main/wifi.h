#pragma once

typedef enum {
    APP_WIFI_NO_CONFIG,     // SSID empty in menuconfig
    APP_WIFI_CONNECTING,
    APP_WIFI_CONNECTED,     // info = IP address
    APP_WIFI_DISCONNECTED,  // info = reason text
} app_wifi_state_t;

// Called from the WiFi/event task — NOT the LVGL task. Lock LVGL before touching UI.
typedef void (*app_wifi_cb_t)(app_wifi_state_t state, const char *info);

// Requires nvs_flash_init() done. Connects to CONFIG_APP_WIFI_SSID, auto-reconnects, starts SNTP.
void app_wifi_start(app_wifi_cb_t cb);
// RSSI in dBm, or 0 if not connected.
int app_wifi_rssi(void);
