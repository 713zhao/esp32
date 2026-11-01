#include "wifi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "sdkconfig.h"

static const char *TAG = "wifi";
static app_wifi_cb_t s_cb;
static TimerHandle_t s_retry_timer;
static int s_retry_delay_ms = 1000;
static bool s_connected;

static void notify(app_wifi_state_t st, const char *info)
{
    if (s_cb) s_cb(st, info);
}

static void retry_cb(TimerHandle_t t)
{
    notify(APP_WIFI_CONNECTING, CONFIG_APP_WIFI_SSID);
    esp_wifi_connect();
}

static const char *reason_str(uint8_t r)
{
    switch (r) {
    case WIFI_REASON_NO_AP_FOUND: return "AP not found (SSID? 5GHz?)";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "auth failed (password?)";
    case WIFI_REASON_ASSOC_LEAVE: return "left";
    case WIFI_REASON_BEACON_TIMEOUT: return "signal lost";
    default: return "disconnected";
    }
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        notify(APP_WIFI_CONNECTING, CONFIG_APP_WIFI_SSID);
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = data;
        s_connected = false;
        char msg[64];
        snprintf(msg, sizeof msg, "%s (%d)", reason_str(d->reason), d->reason);
        ESP_LOGW(TAG, "disconnected: %s, retry in %d ms", msg, s_retry_delay_ms);
        notify(APP_WIFI_DISCONNECTED, msg);
        xTimerChangePeriod(s_retry_timer, pdMS_TO_TICKS(s_retry_delay_ms), 0);
        xTimerStart(s_retry_timer, 0);
        if (s_retry_delay_ms < 30000) s_retry_delay_ms *= 2;   // exponential backoff
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        char ip[16];
        snprintf(ip, sizeof ip, IPSTR, IP2STR(&e->ip_info.ip));
        ESP_LOGI(TAG, "got ip: %s", ip);
        s_connected = true;
        s_retry_delay_ms = 1000;
        static bool sntp_started;
        if (!sntp_started) {
            esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            esp_netif_sntp_init(&cfg);
            sntp_started = true;
        }
        notify(APP_WIFI_CONNECTED, ip);
    }
}

int app_wifi_rssi(void)
{
    wifi_ap_record_t ap;
    if (!s_connected || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return 0;
    return ap.rssi;
}

void app_wifi_start(app_wifi_cb_t cb)
{
    s_cb = cb;
    setenv("TZ", CONFIG_APP_TIMEZONE, 1);
    tzset();

    if (strlen(CONFIG_APP_WIFI_SSID) == 0) {
        ESP_LOGW(TAG, "No SSID configured: idf.py menuconfig -> App WiFi");
        notify(APP_WIFI_NO_CONFIG, "set SSID in menuconfig");
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));

    s_retry_timer = xTimerCreate("wifi_retry", pdMS_TO_TICKS(1000), pdFALSE, NULL, retry_cb);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));

    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, CONFIG_APP_WIFI_SSID, sizeof wc.sta.ssid);
    strlcpy((char *)wc.sta.password, CONFIG_APP_WIFI_PASSWORD, sizeof wc.sta.password);
    // Minimum security accepted. Lower to WIFI_AUTH_WPA_PSK for old routers, WIFI_AUTH_OPEN for open networks.
    wc.sta.threshold.authmode = strlen(CONFIG_APP_WIFI_PASSWORD) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wc.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "connecting to '%s'", CONFIG_APP_WIFI_SSID);
}
