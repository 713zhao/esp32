// Chat UI for the 320x170 landscape screen:
//   [header: name ........ wifi/ip]
//   [scrollable message bubbles   ]
//   [quick reply x N ...  | ⌨ ]
// Keyboard opens as a full-screen overlay on the top layer.
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "esp_lvgl_port.h"
#include "sdkconfig.h"

#define HEADER_H      20
#define BAR_H         36
#define MAX_BUBBLES   30
#define BUBBLE_TEXT_MAX_W  (BOARD_LCD_W * 3 / 4)   // max text width inside a bubble, px
#define COL_BG        0x0F1720
#define COL_HEADER    0x1B2530
#define COL_IN        0x243243     // message received from LAN
#define COL_OUT       0x1F6FEB     // reply typed on the board
#define COL_MUTED     0x7D8590

static const char *k_default_replies[] = {"OK", "Yes", "No"};

static lv_obj_t *s_status, *s_list, *s_bar;
static lv_obj_t *s_kb_panel, *s_kb_ta;
static uint32_t s_reply_to;           // IN message the quick replies belong to

static void send_reply(const char *text);

// ---------- bubbles ----------
static void add_bubble(chat_dir_t dir, const char *from, const char *text)
{
    while (lv_obj_get_child_count(s_list) >= MAX_BUBBLES) lv_obj_delete(lv_obj_get_child(s_list, 0));

    // full-width transparent row so the bubble can sit left (in) or right (out)
    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, dir == CHAT_OUT ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *b = lv_obj_create(row);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(dir == CHAT_OUT ? COL_OUT : COL_IN), 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_pad_hor(b, 8, 0);
    lv_obj_set_style_pad_ver(b, 4, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);

    // Size labels in pixels: percentage widths inside content-sized parents resolve to 0
    // (that collapsed every bubble to one word per line).
    const lv_font_t *font = lv_obj_get_style_text_font(s_list, 0);
    if (dir == CHAT_IN && from && from[0]) {
        lv_obj_t *who = lv_label_create(b);
        lv_obj_set_style_text_color(who, lv_color_hex(0x9CC4FF), 0);
        lv_label_set_long_mode(who, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(who, from);
        lv_point_t sz;
        lv_text_get_size(&sz, from, font, 0, 0, BUBBLE_TEXT_MAX_W, LV_TEXT_FLAG_NONE);
        lv_obj_set_width(who, LV_MIN(sz.x + 1, BUBBLE_TEXT_MAX_W));
    }
    lv_obj_t *lbl = lv_label_create(b);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(lbl, text);
    lv_point_t sz;
    lv_text_get_size(&sz, text, font, 0, 0, BUBBLE_TEXT_MAX_W, LV_TEXT_FLAG_NONE);
    lv_obj_set_width(lbl, LV_MIN(sz.x + 1, BUBBLE_TEXT_MAX_W));

    lv_obj_update_layout(s_list);
    lv_obj_scroll_to_view(row, LV_ANIM_ON);
}

// ---------- quick replies ----------
static void on_quick(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    send_reply(lv_label_get_text(lv_obj_get_child(btn, 0)));
}

static void kb_open(lv_event_t *e);

static void set_quick_replies(const char *const *items, int n)
{
    lv_obj_clean(s_bar);
    for (int i = 0; i < n; i++) {
        lv_obj_t *btn = lv_button_create(s_bar);
        lv_obj_set_height(btn, lv_pct(100));
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_style_pad_hor(btn, 4, 0);
        lv_obj_add_event_cb(btn, on_quick, LV_EVENT_CLICKED, NULL);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(l, lv_pct(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(l, items[i]);
        lv_obj_center(l);
    }
    lv_obj_t *kb = lv_button_create(s_bar);
    lv_obj_set_size(kb, 44, lv_pct(100));
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x30363D), 0);
    lv_obj_add_event_cb(kb, kb_open, LV_EVENT_CLICKED, NULL);
    lv_obj_t *kl = lv_label_create(kb);
    lv_label_set_text(kl, LV_SYMBOL_KEYBOARD);
    lv_obj_center(kl);
}

static void reset_quick_replies(void)
{
    set_quick_replies(k_default_replies, sizeof k_default_replies / sizeof k_default_replies[0]);
}
static void reset_quick_replies_cb(void *arg) { reset_quick_replies(); }

// ---------- keyboard overlay ----------
static void kb_close(void)
{
    // async: we are usually inside the keyboard's own event callback
    if (s_kb_panel) { lv_obj_delete_async(s_kb_panel); s_kb_panel = NULL; s_kb_ta = NULL; }
}

static void on_kb_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {                    // ✓ key
        const char *t = lv_textarea_get_text(s_kb_ta);
        if (t && t[0]) {
            char buf[CHAT_MAX_TEXT];
            strlcpy(buf, t, sizeof buf);
            kb_close();
            send_reply(buf);
            return;
        }
        kb_close();
    } else if (code == LV_EVENT_CANCEL) {            // keyboard-hide key
        kb_close();
    }
}

static void kb_open(lv_event_t *e)
{
    if (s_kb_panel) return;
    s_kb_panel = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_kb_panel);
    lv_obj_set_size(s_kb_panel, BOARD_LCD_W, BOARD_LCD_H);
    lv_obj_set_style_bg_color(s_kb_panel, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_kb_panel, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_kb_panel, LV_OBJ_FLAG_CLICKABLE);   // swallow touches meant for the chat below

    s_kb_ta = lv_textarea_create(s_kb_panel);
    lv_obj_set_size(s_kb_ta, BOARD_LCD_W - 8, 34);
    lv_obj_align(s_kb_ta, LV_ALIGN_TOP_MID, 0, 3);
    lv_textarea_set_one_line(s_kb_ta, true);
    lv_textarea_set_max_length(s_kb_ta, CHAT_MAX_TEXT - 1);
    lv_textarea_set_placeholder_text(s_kb_ta, "Type a reply...");

    lv_obj_t *kb = lv_keyboard_create(s_kb_panel);
    lv_obj_set_size(kb, BOARD_LCD_W, BOARD_LCD_H - 40);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(kb, 2, 0);
    lv_obj_set_style_pad_gap(kb, 2, 0);
    lv_keyboard_set_textarea(kb, s_kb_ta);
    lv_obj_add_event_cb(kb, on_kb_event, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, on_kb_event, LV_EVENT_CANCEL, NULL);
}

// ---------- send ----------
static void send_reply(const char *text)
{
    chat_msg_t m = { .dir = CHAT_OUT, .reply_to = s_reply_to };
    strlcpy(m.from, CONFIG_APP_CHAT_NAME, sizeof m.from);
    strlcpy(m.text, text, sizeof m.text);
    chat_add(&m);                       // network clients pick it up via GET /api/messages
    add_bubble(CHAT_OUT, NULL, m.text);
    s_reply_to = 0;
    // async: the tapped quick-reply button lives in s_bar and must not be deleted inside its own event
    lv_async_call(reset_quick_replies_cb, NULL);
}

// ---------- LED flash on new message ----------
static void led_off_cb(lv_timer_t *t) { board_led_set(0, 0, 0); }

// ---------- public ----------
void ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0xE6EDF3), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hdr = lv_obj_create(scr);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, BOARD_LCD_W, HEADER_H);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(COL_HEADER), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(hdr, 6, 0);
    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, CONFIG_APP_CHAT_NAME);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);
    s_status = lv_label_create(hdr);
    lv_obj_set_style_text_color(s_status, lv_color_hex(COL_MUTED), 0);
    lv_label_set_text(s_status, LV_SYMBOL_WIFI " ...");
    lv_obj_align(s_status, LV_ALIGN_RIGHT_MID, 0, 0);

    s_list = lv_obj_create(scr);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, BOARD_LCD_W, BOARD_LCD_H - HEADER_H - BAR_H);
    lv_obj_set_pos(s_list, 0, HEADER_H);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_list, 4, 0);
    lv_obj_set_style_pad_row(s_list, 4, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_ACTIVE);

    s_bar = lv_obj_create(scr);
    lv_obj_remove_style_all(s_bar);
    lv_obj_set_size(s_bar, BOARD_LCD_W, BAR_H);
    lv_obj_align(s_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(s_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(s_bar, 3, 0);
    lv_obj_set_style_pad_column(s_bar, 3, 0);
    lv_obj_remove_flag(s_bar, LV_OBJ_FLAG_SCROLLABLE);

    reset_quick_replies();
    add_bubble(CHAT_IN, "System", board_has_touch() ? "Waiting for messages..." : "Touch not detected!");
}

void ui_show_message(const chat_msg_t *m)
{
    if (!lvgl_port_lock(0)) return;
    add_bubble(m->dir, m->from, m->text);
    if (m->dir == CHAT_IN) {
        s_reply_to = m->id;
        if (m->n_replies) {
            const char *items[CHAT_MAX_REPLIES];
            for (int i = 0; i < m->n_replies; i++) items[i] = m->replies[i];
            set_quick_replies(items, m->n_replies);
        } else {
            reset_quick_replies();
        }
        board_led_set(0, 40, 0);
        lv_timer_t *t = lv_timer_create(led_off_cb, 1500, NULL);
        lv_timer_set_repeat_count(t, 1);
    }
    lvgl_port_unlock();
}

void ui_set_wifi(app_wifi_state_t state, const char *info)
{
    if (!lvgl_port_lock(0)) return;
    switch (state) {
    case APP_WIFI_NO_CONFIG:
        lv_label_set_text(s_status, LV_SYMBOL_WARNING " no WiFi config");
        lv_obj_set_style_text_color(s_status, lv_color_hex(0xF0B72F), 0);
        break;
    case APP_WIFI_CONNECTING:
        lv_label_set_text(s_status, LV_SYMBOL_REFRESH " connecting");
        lv_obj_set_style_text_color(s_status, lv_color_hex(COL_MUTED), 0);
        break;
    case APP_WIFI_CONNECTED:
        lv_label_set_text_fmt(s_status, LV_SYMBOL_WIFI " %s", info);
        lv_obj_set_style_text_color(s_status, lv_color_hex(0x3FB950), 0);
        break;
    case APP_WIFI_DISCONNECTED:
        lv_label_set_text(s_status, LV_SYMBOL_CLOSE " offline");
        lv_obj_set_style_text_color(s_status, lv_color_hex(0xF85149), 0);
        break;
    }
    lvgl_port_unlock();
}
