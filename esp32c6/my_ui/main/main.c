#include "board.h"
#include "chat.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "server.h"
#include "ui.h"
#include "wifi.h"

static const char *TAG = "app";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    chat_init();
    board_init();

    if (lvgl_port_lock(0)) {
        ui_create();
        lvgl_port_unlock();
    }

    app_wifi_start(ui_set_wifi);
    if (app_wifi_configured()) chat_server_start();
    ESP_LOGI(TAG, "started; free heap %u", (unsigned)esp_get_free_heap_size());
}
