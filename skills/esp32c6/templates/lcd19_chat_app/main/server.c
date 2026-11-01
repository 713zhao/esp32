#include "server.h"
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "chat.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mdns.h"
#include "sdkconfig.h"
#include "ui.h"

static const char *TAG = "chat_srv";
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

#define MAX_BODY 2048

static bool authorized(httpd_req_t *req)
{
    if (strlen(CONFIG_APP_CHAT_TOKEN) == 0) return true;
    char tok[64] = "";
    if (httpd_req_get_hdr_value_str(req, "X-Token", tok, sizeof tok) == ESP_OK &&
        strcmp(tok, CONFIG_APP_CHAT_TOKEN) == 0) return true;
    char q[96];
    if (httpd_req_get_url_query_str(req, q, sizeof q) == ESP_OK &&
        httpd_query_key_value(q, "token", tok, sizeof tok) == ESP_OK &&
        strcmp(tok, CONFIG_APP_CHAT_TOKEN) == 0) return true;
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_sendstr(req, "{\"error\":\"bad token\"}");
    return false;
}

static esp_err_t send_json(httpd_req_t *req, const char *status, const char *json)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t h_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start - 1);
}

static esp_err_t h_info(httpd_req_t *req)
{
    char buf[200];
    snprintf(buf, sizeof buf,
             "{\"name\":\"%s\",\"protocol\":\"espchat/1\",\"auth\":%s,\"max_text\":%d,\"max_replies\":%d}",
             CONFIG_APP_CHAT_NAME, strlen(CONFIG_APP_CHAT_TOKEN) ? "true" : "false",
             CHAT_MAX_TEXT - 1, CHAT_MAX_REPLIES);
    return send_json(req, "200 OK", buf);
}

static esp_err_t h_get_messages(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    uint32_t since = 0;
    char q[64], v[16];
    if (httpd_req_get_url_query_str(req, q, sizeof q) == ESP_OK &&
        httpd_query_key_value(q, "since", v, sizeof v) == ESP_OK) {
        since = strtoul(v, NULL, 10);
    }
    char *json = chat_json_since(since);
    esp_err_t r = send_json(req, "200 OK", json ? json : "{}");
    free(json);
    return r;
}

static esp_err_t h_post_message(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    if (req->content_len <= 0 || req->content_len > MAX_BODY) {
        return send_json(req, "413 Payload Too Large", "{\"error\":\"body must be 1..2048 bytes\"}");
    }
    char *body = malloc(req->content_len + 1);
    if (!body) return send_json(req, "500 Internal Server Error", "{\"error\":\"oom\"}");
    int got = 0;
    while (got < req->content_len) {
        int r = httpd_req_recv(req, body + got, req->content_len - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) { free(body); return ESP_FAIL; }
        got += r;
    }
    body[got] = '\0';

    cJSON *j = cJSON_Parse(body);
    free(body);
    const cJSON *text = cJSON_GetObjectItem(j, "text");
    if (!cJSON_IsString(text) || !text->valuestring[0]) {
        cJSON_Delete(j);
        return send_json(req, "400 Bad Request", "{\"error\":\"'text' (string) required\"}");
    }
    chat_msg_t m = { .dir = CHAT_IN };
    strlcpy(m.text, text->valuestring, sizeof m.text);
    const cJSON *from = cJSON_GetObjectItem(j, "from");
    strlcpy(m.from, cJSON_IsString(from) && from->valuestring[0] ? from->valuestring : "LAN", sizeof m.from);
    const cJSON *replies = cJSON_GetObjectItem(j, "replies");
    const cJSON *r;
    cJSON_ArrayForEach(r, replies) {
        if (m.n_replies >= CHAT_MAX_REPLIES) break;
        if (cJSON_IsString(r) && r->valuestring[0]) {
            strlcpy(m.replies[m.n_replies++], r->valuestring, CHAT_MAX_REPLY_LEN);
        }
    }
    cJSON_Delete(j);

    uint32_t id = chat_add(&m);
    ui_show_message(&m);
    ESP_LOGI(TAG, "msg #%lu from %s: %s", (unsigned long)id, m.from, m.text);
    char resp[40];
    snprintf(resp, sizeof resp, "{\"id\":%lu}", (unsigned long)id);
    return send_json(req, "200 OK", resp);
}

static void mdns_start(void)
{
    if (mdns_init() != ESP_OK) { ESP_LOGW(TAG, "mdns init failed"); return; }
    mdns_hostname_set(CONFIG_APP_CHAT_HOSTNAME);
    mdns_instance_name_set(CONFIG_APP_CHAT_NAME);
    mdns_txt_item_t txt[] = { {"protocol", "espchat/1"}, {"path", "/api"} };
    mdns_service_add(CONFIG_APP_CHAT_NAME, "_espchat", "_tcp", 80, txt, 2);
    mdns_service_add(CONFIG_APP_CHAT_NAME, "_http", "_tcp", 80, txt, 2);
    ESP_LOGI(TAG, "mDNS: http://%s.local/", CONFIG_APP_CHAT_HOSTNAME);
}

void chat_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 6144;
    cfg.lru_purge_enable = true;
    httpd_handle_t srv = NULL;
    if (httpd_start(&srv, &cfg) != ESP_OK) { ESP_LOGE(TAG, "httpd start failed"); return; }
    const httpd_uri_t uris[] = {
        { .uri = "/",             .method = HTTP_GET,  .handler = h_index },
        { .uri = "/api/info",     .method = HTTP_GET,  .handler = h_info },
        { .uri = "/api/messages", .method = HTTP_GET,  .handler = h_get_messages },
        { .uri = "/api/message",  .method = HTTP_POST, .handler = h_post_message },
    };
    for (size_t i = 0; i < sizeof uris / sizeof uris[0]; i++) httpd_register_uri_handler(srv, &uris[i]);
    mdns_start();
    ESP_LOGI(TAG, "chat server listening on :80");
}
