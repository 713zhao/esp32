#!/usr/bin/env python3
"""LAN client for the ESP32 chat board (protocol espchat/1). Standard library only.

  espchat.py send "Dinner at 7?" --replies Yes No Later      # show a message with quick replies
  espchat.py send "Build done" --from CI --wait 60             # ...and wait up to 60 s for the answer
  espchat.py listen                                            # print replies typed on the board
  espchat.py chat                                              # interactive: type lines, see replies

Host: --host or $ESPCHAT_HOST (default esp32-chat.local). Token: --token or $ESPCHAT_TOKEN.
"""
import argparse
import json
import os
import sys
import threading
import time
import urllib.error
import urllib.request


class Board:
    def __init__(self, host, token=""):
        self.base = f"http://{host}"
        self.token = token

    def _req(self, method, path, body=None):
        headers = {"Content-Type": "application/json"}
        if self.token:
            headers["X-Token"] = self.token
        data = json.dumps(body).encode() if body is not None else None
        req = urllib.request.Request(self.base + path, data=data, headers=headers, method=method)
        with urllib.request.urlopen(req, timeout=5) as r:
            return json.loads(r.read().decode())

    def info(self):
        return self._req("GET", "/api/info")

    def send(self, text, sender="PC", replies=None):
        return self._req("POST", "/api/message", {"from": sender, "text": text, "replies": replies or []})["id"]

    def messages(self, since=0):
        return self._req("GET", f"/api/messages?since={since}")

    def last_id(self):
        return self.messages(0)["last"]

    def follow(self, since, interval=1.0):
        """Yield new messages forever (polling)."""
        while True:
            try:
                j = self.messages(since)
            except (urllib.error.URLError, TimeoutError, ConnectionError) as e:
                print(f"[connection problem: {e}; retrying]", file=sys.stderr)
                time.sleep(3)
                continue
            for m in j.get("messages", []):
                yield m
            since = j.get("last", since)
            time.sleep(interval)


def fmt(m):
    t = time.strftime("%H:%M:%S", time.localtime(m["ts"])) if m.get("ts") else "--:--:--"
    who = m["from"] if m["dir"] == "in" else "board"
    arrow = "->" if m["dir"] == "in" else "<-"
    return f"[{t}] {arrow} {who}: {m['text']}"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default=os.environ.get("ESPCHAT_HOST", "esp32-chat.local"))
    ap.add_argument("--token", default=os.environ.get("ESPCHAT_TOKEN", ""))
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("send", help="send one message")
    s.add_argument("text")
    s.add_argument("--from", dest="sender", default=os.environ.get("ESPCHAT_FROM", "PC"))
    s.add_argument("--replies", nargs="*", default=[], help="up to 4 quick-reply buttons")
    s.add_argument("--wait", type=float, default=0, help="seconds to wait for the board's reply (prints it)")
    sub.add_parser("listen", help="print messages as they arrive")
    c = sub.add_parser("chat", help="interactive chat")
    c.add_argument("--from", dest="sender", default=os.environ.get("ESPCHAT_FROM", "PC"))
    sub.add_parser("info", help="show board info")
    a = ap.parse_args()

    b = Board(a.host, a.token)
    try:
        if a.cmd == "info":
            print(json.dumps(b.info(), indent=2))
        elif a.cmd == "send":
            mid = b.send(a.text, a.sender, a.replies[:4])
            print(f"sent #{mid}")
            if a.wait:
                deadline = time.time() + a.wait
                since = mid
                while time.time() < deadline:
                    for m in b.messages(since).get("messages", []):
                        since = m["id"]
                        if m["dir"] == "out" and m.get("reply_to") == mid:
                            print(m["text"])
                            return 0
                    time.sleep(0.5)
                print("(no reply)", file=sys.stderr)
                return 2
        elif a.cmd == "listen":
            for m in b.follow(b.last_id()):
                print(fmt(m), flush=True)
        elif a.cmd == "chat":
            since = b.last_id()

            def rx():
                for m in b.follow(since):
                    if m["dir"] == "out":
                        print(f"\r{fmt(m)}\n> ", end="", flush=True)

            threading.Thread(target=rx, daemon=True).start()
            print(f"Connected to {a.host}. Type messages, Ctrl+D to quit.")
            for line in iter(lambda: input("> "), None):
                if line.strip():
                    b.send(line.strip(), a.sender)
    except (EOFError, KeyboardInterrupt):
        print()
    except urllib.error.URLError as e:
        print(f"Cannot reach {a.host}: {e.reason}. Try --host <board IP>.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
