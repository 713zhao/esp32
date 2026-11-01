#include "chat.h"
#include <string.h>
#include <time.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static chat_msg_t s_log[CHAT_MAX_MSGS];
static uint32_t s_next_id = 1;
static uint32_t s_last_in;
static SemaphoreHandle_t s_mux;

void chat_init(void) { s_mux = xSemaphoreCreateMutex(); }

uint32_t chat_add(chat_msg_t *m)
{
    xSemaphoreTake(s_mux, portMAX_DELAY);
    m->id = s_next_id++;
    time_t now = time(NULL);
    m->ts = now > 1700000000 ? (int64_t)now : 0;
    s_log[m->id % CHAT_MAX_MSGS] = *m;
    if (m->dir == CHAT_IN) s_last_in = m->id;
    xSemaphoreGive(s_mux);
    return m->id;
}

uint32_t chat_last_in_id(void) { return s_last_in; }

char *chat_json_since(uint32_t since)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(root, "messages");
    xSemaphoreTake(s_mux, portMAX_DELAY);
    uint32_t last = s_next_id - 1;
    uint32_t first = last >= CHAT_MAX_MSGS ? last - CHAT_MAX_MSGS + 1 : 1;
    if (since + 1 > first) first = since + 1;
    for (uint32_t id = first; id <= last; id++) {
        const chat_msg_t *m = &s_log[id % CHAT_MAX_MSGS];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", m->id);
        cJSON_AddStringToObject(o, "dir", m->dir == CHAT_IN ? "in" : "out");
        cJSON_AddStringToObject(o, "from", m->from);
        cJSON_AddStringToObject(o, "text", m->text);
        cJSON_AddNumberToObject(o, "ts", (double)m->ts);
        if (m->reply_to) cJSON_AddNumberToObject(o, "reply_to", m->reply_to);
        if (m->n_replies) {
            cJSON *r = cJSON_AddArrayToObject(o, "replies");
            for (int i = 0; i < m->n_replies; i++) cJSON_AddItemToArray(r, cJSON_CreateString(m->replies[i]));
        }
        cJSON_AddItemToArray(arr, o);
    }
    xSemaphoreGive(s_mux);
    cJSON_AddNumberToObject(root, "last", last);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}
