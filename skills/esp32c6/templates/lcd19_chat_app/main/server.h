#pragma once
// LAN chat protocol: HTTP/JSON on port 80 + mDNS (<hostname>.local, service _espchat._tcp).
// See PROTOCOL.md in the project root. Call after app_wifi_start() created the network stack.
void chat_server_start(void);
