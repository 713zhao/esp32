# Building UIs with LVGL 9 on the 1.9" board

Template layout (`templates/lcd19_touch_app/main/`):
- `board.c/.h` — LCD, touch, backlight, LVGL port init. `board_init()` returns the `lv_display_t*`.
- `wifi.c/.h` — STA connect + callbacks.
- `ui.c/.h` — **your UI**. `ui_create()` builds widgets; `ui_set_wifi_status()` etc. are called from other tasks.
- `main.c` — wiring.

## Threading rule (most important)
LVGL runs in the `esp_lvgl_port` task. Any LVGL call from elsewhere:
```c
if (lvgl_port_lock(0)) {          // 0 = wait forever
    lv_label_set_text(label, "hi");
    lvgl_port_unlock();
}
```
LVGL event callbacks (`lv_obj_add_event_cb`) and `lv_timer` callbacks already run inside the lock — don't lock again there.

## Screen geometry
Portrait 170 x 320 (default) / landscape 320 x 170 (`#define BOARD_LANDSCAPE 1` in board.h).
Use flex/grid layouts so content adapts: `lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN)`.

## Common snippets
```c
lv_obj_t *scr = lv_screen_active();
lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);

lv_obj_t *lbl = lv_label_create(scr);
lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);   // font must be enabled in sdkconfig
lv_label_set_text(lbl, "Hello");
lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 10);

lv_obj_t *btn = lv_button_create(scr);
lv_obj_add_event_cb(btn, on_click, LV_EVENT_CLICKED, NULL);
lv_obj_t *bl = lv_label_create(btn); lv_label_set_text(bl, LV_SYMBOL_POWER " Toggle");

lv_obj_t *sl = lv_slider_create(scr);
lv_obj_add_event_cb(sl, on_slide, LV_EVENT_VALUE_CHANGED, NULL);
int v = lv_slider_get_value(sl);

lv_timer_create(update_cb, 1000, NULL);   // periodic UI refresh in LVGL context
```
Multiple pages: `lv_tabview_create()` (swipe between tabs) or separate screens with
`lv_screen_load_anim(scr2, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false)`.
Arc gauge: `lv_arc_create`; chart: `lv_chart_create` + `lv_chart_set_next_value`.
Built-in symbols: `LV_SYMBOL_WIFI`, `LV_SYMBOL_BATTERY_FULL`, `LV_SYMBOL_SETTINGS`, ...

## Fonts & images
- Enable sizes in `sdkconfig.defaults`: `CONFIG_LV_FONT_MONTSERRAT_14/20/28=y` (template enables 14, 20, 28, 40).
  `sdkconfig.defaults` only seeds a *new* `sdkconfig`. For an existing project either append the
  line to `sdkconfig` directly / use menuconfig, or `rm sdkconfig && idf.py reconfigure` and then
  re-run `scripts/set-wifi.sh` (deleting sdkconfig drops the WiFi credentials).
- Custom font / CJK: https://lvgl.io/tools/fontconverter → C file into `main/`, `LV_FONT_DECLARE(my_font)`.
- Images: https://lvgl.io/tools/imageconverter (LVGL v9, RGB565) → C array, `LV_IMAGE_DECLARE(img)`, `lv_image_set_src`.
  Keep images small — 8 MB flash, 3 MB app partition.

## Visual designers (optional)
- **SquareLine Studio** or **EEZ Studio** → target LVGL 9.x, resolution 170x320 (or 320x170), 16-bit colour.
  Export UI files into `main/ui/`, add them to `main/CMakeLists.txt` SRCS (or `SRC_DIRS "." "ui"`),
  call the generated `ui_init()` inside `lvgl_port_lock` instead of the template's `ui_create()`.

## Performance tips
- SPI at 40 MHz (`BOARD_LCD_PCLK_HZ`); double DMA buffers (template default).
- Avoid full-screen gradients/shadows/opacity animations — no GPU, no PSRAM.
- `lv_obj_invalidate` only what changes; timers at 100–1000 ms are plenty for status UIs.
