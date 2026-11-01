#pragma once
#include "chat.h"
#include "wifi.h"

// Build the UI. Must be called with the LVGL lock held.
void ui_create(void);
// Thread-safe (take the LVGL lock internally). Safe from WiFi / HTTP tasks.
void ui_set_wifi(app_wifi_state_t state, const char *info);
void ui_show_message(const chat_msg_t *m);
