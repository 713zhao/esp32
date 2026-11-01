#pragma once
// In-memory chat log shared by the HTTP server (network side) and the UI (board side).
#include <stdint.h>

#define CHAT_MAX_MSGS       40     // ring buffer size
#define CHAT_MAX_TEXT       256    // bytes incl. NUL
#define CHAT_MAX_FROM       24
#define CHAT_MAX_REPLIES    4
#define CHAT_MAX_REPLY_LEN  24

typedef enum { CHAT_IN = 0, CHAT_OUT = 1 } chat_dir_t;   // IN = from network to board, OUT = typed on board

typedef struct {
    uint32_t id;                    // assigned by chat_add, starts at 1, monotonically increasing
    chat_dir_t dir;
    int64_t ts;                     // unix seconds (0 if clock not synced yet)
    uint32_t reply_to;              // OUT: id of the IN message being answered (0 = none)
    char from[CHAT_MAX_FROM];
    char text[CHAT_MAX_TEXT];
    uint8_t n_replies;              // IN: suggested quick replies
    char replies[CHAT_MAX_REPLIES][CHAT_MAX_REPLY_LEN];
} chat_msg_t;

void chat_init(void);
// Copies *m into the log, assigns id + ts. Returns new id. Thread-safe. Does NOT touch the UI.
uint32_t chat_add(chat_msg_t *m);
uint32_t chat_last_in_id(void);
// JSON {"last":N,"messages":[...]} with all messages id > since. Caller frees. Thread-safe.
char *chat_json_since(uint32_t since);
