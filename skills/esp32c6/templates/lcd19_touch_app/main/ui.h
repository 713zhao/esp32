#pragma once
#include "wifi.h"

// Build the UI. Must be called with the LVGL lock held.
void ui_create(void);
// Thread-safe: takes the LVGL lock internally. Safe to call from WiFi callback.
void ui_set_wifi(app_wifi_state_t state, const char *info);
