# espchat/1 — LAN chat protocol for the ESP32-C6 board

Transport: plain HTTP/1.1 + JSON on port 80. Discovery: mDNS hostname `esp32-chat.local`,
services `_espchat._tcp` and `_http._tcp` (TXT: `protocol=espchat/1`, `path=/api`).
Auth: none by default (LAN only). If `CONFIG_APP_CHAT_TOKEN` is set, send `X-Token: <token>`
header (or `?token=<token>` query) on `/api/message(s)`; otherwise `401`.

## Endpoints

| Method | Path | Purpose |
|---|---|---|
| GET  | `/` | Built-in web chat page (phone/PC browser) |
| GET  | `/api/info` | `{"name","protocol":"espchat/1","auth":bool,"max_text":255,"max_replies":4}` |
| POST | `/api/message` | Show a message on the board |
| GET  | `/api/messages?since=<id>` | Messages with id > since (both directions) |

### POST /api/message
Request (max 2048 B):
```json
{"from": "Kitchen PC", "text": "Dinner is ready!", "replies": ["Coming", "5 min", "Not hungry"]}
```
- `text` required (truncated to 255 bytes). `from` optional (≤23 B, default "LAN").
- `replies` optional, ≤4 strings (≤23 B each) → shown as quick-reply buttons; absent → defaults OK/Yes/No.
Response: `{"id": 7}`. Errors: `400` (no text), `413` (too big), `401` (token).

### GET /api/messages?since=N
```json
{"last": 8, "messages": [
  {"id":7,"dir":"in","from":"Kitchen PC","text":"Dinner is ready!","ts":1793532094,"replies":["Coming","5 min","Not hungry"]},
  {"id":8,"dir":"out","from":"ESP32 Chat","text":"Coming","ts":1793532101,"reply_to":7}
]}
```
- `dir`: `in` = sent to the board, `out` = typed/tapped on the board.
- `reply_to` (out only): id of the incoming message that was on screen when the user replied.
- `ts`: unix seconds, 0 if the board clock isn't synced yet.
- Board keeps the last 40 messages in RAM (lost on reboot). Poll with the returned `last` (1 s is fine).
- ids restart at 1 after a reboot: if `last` < your cursor, reset your cursor to 0.

## Examples
```bash
curl -X POST http://esp32-chat.local/api/message -H 'Content-Type: application/json' \
     -d '{"from":"curl","text":"Hello","replies":["Hi","Bye"]}'
curl 'http://esp32-chat.local/api/messages?since=0'
python3 tools/espchat.py send "Deploy finished. Restart?" --replies Yes No --wait 60   # prints the answer
python3 tools/espchat.py chat
```
Note: board fonts cover ASCII only (no emoji/CJK) unless a custom LVGL font is added.
