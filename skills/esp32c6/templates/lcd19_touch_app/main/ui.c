// Demo UI — replace freely. Layout adapts to portrait (170x320) or landscape via flex.
#include "ui.h"
#include <stdio.h>
#include <time.h>
#include "board.h"
#include "esp_lvgl_port.h"

static lv_obj_t *s_wifi_lbl, *s_ip_lbl, *s_clock_lbl, *s_count_lbl;
static int s_taps;
static char s_ip[16];

static void on_tap(lv_event_t *e)
{
    lv_label_set_text_fmt(s_count_lbl, "Taps: %d", ++s_taps);
}

static void on_brightness(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    board_set_backlight((uint8_t)lv_slider_get_value(slider));
}

static void clock_tick(lv_timer_t *t)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year + 1900 < 2024) {
        lv_label_set_text(s_clock_lbl, "--:--:--");
    } else {
        lv_label_set_text_fmt(s_clock_lbl, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
    int rssi = app_wifi_rssi();
    if (rssi && s_ip[0]) lv_label_set_text_fmt(s_ip_lbl, "%s  %d dBm", s_ip, rssi);
}

void ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F1720), 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0xE6EDF3), 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(scr, 8, 0);
    lv_obj_set_style_pad_row(scr, 8, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "ESP32-C6");

    s_wifi_lbl = lv_label_create(scr);
    lv_obj_set_width(s_wifi_lbl, lv_pct(100));
    lv_label_set_long_mode(s_wifi_lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(s_wifi_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_wifi_lbl, LV_SYMBOL_WIFI " starting...");

    s_ip_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(s_ip_lbl, lv_color_hex(0x7D8590), 0);
    lv_label_set_text(s_ip_lbl, "");

    s_clock_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(s_clock_lbl, &lv_font_montserrat_28, 0);
    lv_label_set_text(s_clock_lbl, "--:--:--");

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_width(btn, lv_pct(90));
    lv_obj_add_event_cb(btn, on_tap, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, LV_SYMBOL_OK " Tap me");
    lv_obj_center(bl);

    s_count_lbl = lv_label_create(scr);
    lv_label_set_text(s_count_lbl, board_has_touch() ? "Taps: 0" : "(no touch detected)");

    lv_obj_t *bright = lv_label_create(scr);
    lv_label_set_text(bright, LV_SYMBOL_IMAGE " Brightness");

    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, lv_pct(85));
    lv_slider_set_range(slider, 5, 100);
    lv_slider_set_value(slider, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_VALUE_CHANGED, NULL);

    lv_timer_create(clock_tick, 1000, NULL);
}

void ui_set_wifi(app_wifi_state_t state, const char *info)
{
    if (!lvgl_port_lock(0)) return;
    switch (state) {
    case APP_WIFI_NO_CONFIG:
        lv_label_set_text(s_wifi_lbl, LV_SYMBOL_WARNING " WiFi not configured");
        lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0xF0B72F), 0);
        break;
    case APP_WIFI_CONNECTING:
        lv_label_set_text_fmt(s_wifi_lbl, LV_SYMBOL_REFRESH " %s", info);
        lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0x7D8590), 0);
        break;
    case APP_WIFI_CONNECTED:
        lv_label_set_text(s_wifi_lbl, LV_SYMBOL_WIFI " Connected");
        lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0x3FB950), 0);
        snprintf(s_ip, sizeof s_ip, "%s", info);
        lv_label_set_text(s_ip_lbl, s_ip);
        break;
    case APP_WIFI_DISCONNECTED:
        lv_label_set_text_fmt(s_wifi_lbl, LV_SYMBOL_CLOSE " %s", info);
        lv_obj_set_style_text_color(s_wifi_lbl, lv_color_hex(0xF85149), 0);
        s_ip[0] = '\0';
        lv_label_set_text(s_ip_lbl, "");
        break;
    }
    lvgl_port_unlock();
}
