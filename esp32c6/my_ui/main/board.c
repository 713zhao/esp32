#include "board.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "led_strip.h"

static const char *TAG = "board";

#define LCD_HOST       SPI2_HOST
#define BL_LEDC_TIMER  LEDC_TIMER_0
#define BL_LEDC_CH     LEDC_CHANNEL_0
#define BL_LEDC_RES    LEDC_TIMER_10_BIT

static i2c_master_bus_handle_t s_i2c_bus;
static bool s_has_touch;
static led_strip_handle_t s_led;

// Waveshare panel tuning (porch, voltages, gamma) from their demo, sent after the stock ST7789 init.
typedef struct { uint8_t cmd; uint8_t data[14]; uint8_t len; } lcd_cmd_t;
static const lcd_cmd_t k_vendor_init[] = {
    {0xB2, {0x0C, 0x0C, 0x00, 0x33, 0x33}, 5},
    {0xB7, {0x35}, 1},
    {0xBB, {0x13}, 1},
    {0xC0, {0x2C}, 1},
    {0xC2, {0x01}, 1},
    {0xC3, {0x0B}, 1},
    {0xC4, {0x20}, 1},
    {0xC6, {0x0F}, 1},
    {0xD0, {0xA4, 0xA1}, 2},
    {0xD6, {0xA1}, 1},
    {0xE0, {0x00, 0x03, 0x07, 0x08, 0x07, 0x15, 0x2A, 0x44, 0x42, 0x0A, 0x17, 0x18, 0x25, 0x27}, 14},
    {0xE1, {0x00, 0x03, 0x08, 0x07, 0x07, 0x23, 0x2A, 0x43, 0x42, 0x09, 0x18, 0x17, 0x25, 0x27}, 14},
};

static void backlight_init(void)
{
    const ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BL_LEDC_RES,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    const ledc_channel_config_t c = {
        .gpio_num = BOARD_PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_LEDC_CH,
        .timer_sel = BL_LEDC_TIMER,
        .duty = 0,                      // off until panel is ready
        .hpoint = 0,
        .flags.output_invert = 1,       // backlight is active LOW
    };
    ESP_ERROR_CHECK(ledc_channel_config(&c));
}

void board_set_backlight(uint8_t percent)
{
    if (percent > 100) percent = 100;
    const uint32_t max = (1u << BL_LEDC_RES) - 1;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CH, max * percent / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CH);
}

bool board_boot_button_pressed(void) { return gpio_get_level(BOARD_PIN_BOOT_BTN) == 0; }
bool board_has_touch(void) { return s_has_touch; }

static void led_init(void)
{
    const led_strip_config_t cfg = {
        .strip_gpio_num = BOARD_PIN_RGB_LED,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    const led_strip_rmt_config_t rmt = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };
    if (led_strip_new_rmt_device(&cfg, &rmt, &s_led) != ESP_OK) {
        ESP_LOGW(TAG, "RGB LED init failed");
        s_led = NULL;
        return;
    }
    led_strip_clear(s_led);
}

void board_led_set(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_led) return;
    led_strip_set_pixel(s_led, 0, r, g, b);
    led_strip_refresh(s_led);
}

static void lcd_init(esp_lcd_panel_io_handle_t *out_io, esp_lcd_panel_handle_t *out_panel)
{
    const spi_bus_config_t bus = {
        .mosi_io_num = BOARD_PIN_LCD_MOSI,
        .sclk_io_num = BOARD_PIN_LCD_SCLK,
        .miso_io_num = -1,               // set BOARD_PIN_SD_MISO here if you add the SD card
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_W * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = BOARD_PIN_LCD_DC,
        .cs_gpio_num = BOARD_PIN_LCD_CS,
        .pclk_hz = BOARD_LCD_PCLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));

    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    for (size_t i = 0; i < sizeof(k_vendor_init) / sizeof(k_vendor_init[0]); i++) {
        esp_lcd_panel_io_tx_param(io, k_vendor_init[i].cmd, k_vendor_init[i].data, k_vendor_init[i].len);
    }
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
#if BOARD_LANDSCAPE
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 0, BOARD_LCD_X_GAP));
#else
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, BOARD_LCD_X_GAP, 0));
#endif
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    *out_io = io;
    *out_panel = panel;
}

static esp_lcd_touch_handle_t touch_init(void)
{
    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BOARD_PIN_I2C_SDA,
        .scl_io_num = BOARD_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_i2c_bus));

    if (i2c_master_probe(s_i2c_bus, BOARD_TOUCH_ADDR, 100) != ESP_OK) {
        ESP_LOGW(TAG, "CST816 not found at 0x%02x (non-touch SKU?) - touch disabled", BOARD_TOUCH_ADDR);
        return NULL;
    }

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_cfg.scl_speed_hz = 400 * 1000;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_cfg, &tp_io));

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BOARD_LCD_NATIVE_W,
        .y_max = BOARD_LCD_NATIVE_H,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .flags = {
            // Landscape mapping derived from Waveshare demo (x=ty, y=170-tx).
            // If touches land in the wrong place, toggle these.
            .swap_xy = BOARD_LANDSCAPE,
            .mirror_x = BOARD_LANDSCAPE,
            .mirror_y = 0,
        },
    };
    esp_lcd_touch_handle_t tp = NULL;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io, &tp_cfg, &tp));

    // Disable CST816 auto-sleep (reg 0xFE) so polling without INT keeps working.
    const uint8_t one = 0x01;
    if (esp_lcd_panel_io_tx_param(tp_io, 0xFE, &one, 1) != ESP_OK) {
        ESP_LOGW(TAG, "could not disable touch auto-sleep");
    }
    return tp;
}

lv_display_t *board_init(void)
{
    const gpio_config_t btn = {
        .pin_bit_mask = 1ULL << BOARD_PIN_BOOT_BTN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn);

    backlight_init();
    led_init();
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    lcd_init(&io, &panel);
    esp_lcd_touch_handle_t tp = touch_init();
    s_has_touch = tp != NULL;

    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_stack = 6144;
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = BOARD_LCD_W * 40,     // pixels per buffer (~13 KB), x2 double-buffered
        .double_buffer = true,
        .hres = BOARD_LCD_W,
        .vres = BOARD_LCD_H,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = BOARD_LANDSCAPE,
            .mirror_x = BOARD_LANDSCAPE,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);

    if (tp) {
        const lvgl_port_touch_cfg_t touch_cfg = { .disp = disp, .handle = tp };
        lvgl_port_add_touch(&touch_cfg);
    }

    board_set_backlight(80);
    ESP_LOGI(TAG, "display %dx%d ready, touch=%s", BOARD_LCD_W, BOARD_LCD_H, tp ? "yes" : "no");
    return disp;
}
