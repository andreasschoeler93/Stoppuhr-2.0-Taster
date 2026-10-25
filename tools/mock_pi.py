"""Test-Server, der den Pi simuliert: nimmt POST /api/triggers vom Taster an.

Start (Python von PlatformIO, nichts weiter zu installieren):
  & 'C:\\Users\\andre\\.platformio\\penv\\Scripts\\python.exe' tools\\mock_pi.py

Optionen:
  --port 8000       Port (muss zu PI_BASE_URL passen)
  --fail-every N    jede N-te Anfrage mit 503 beantworten (Retry-Test)
  --offline-s S     die ersten S Sekunden nicht antworten (Queue-Test)
"""
import argparse
import json
import time
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

seen = set()
count = 0
started = time.time()
args = None


class Handler(BaseHTTPRequestHandler):
    def _reply(self, code, payload):
        body = json.dumps(payload).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        global count
        if self.path != "/api/triggers":
            self._reply(404, {"ok": False, "error": "not found", "code": "NOT_FOUND"})
            return
        if time.time() - started < args.offline_s:
            time.sleep(5)  # Taster laeuft in seinen Timeout
            return

        count += 1
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length)
        now_ms = int(time.time() * 1000)
        stamp = datetime.now().strftime("%H:%M:%S")

        if args.fail_every and count % args.fail_every == 0:
            print(f"[{stamp}] #{count} -> 503 (simulierter Fehler)")
            self._reply(503, {"ok": False, "error": "simulated", "code": "UNAVAILABLE"})
            return

        try:
            data = json.loads(raw)
            mac, ts = data["mac"], int(data["ts"])
        except (ValueError, KeyError, TypeError):
            print(f"[{stamp}] #{count} -> 400 ungueltiger Body: {raw[:80]!r}")
            self._reply(400, {"ok": False, "error": "invalid body", "code": "BAD_REQUEST"})
            return

        key = (mac, ts)
        if key in seen:
            print(f"[{stamp}] #{count} -> 409 Duplikat mac={mac} ts={ts}")
            self._reply(409, {"ok": False, "error": "duplicate", "code": "DUPLICATE_TRIGGER"})
            return
        seen.add(key)

        age_ms = now_ms - ts
        press = datetime.fromtimestamp(ts / 1000).strftime("%H:%M:%S.%f")[:-3]
        print(f"[{stamp}] #{count} -> 200 mac={mac} Tastendruck={press} (vor {age_ms} ms)")
        self._reply(200, {"ok": True, "received_at": now_ms, "event_id": f"{mac}-{ts}"})

    def log_message(self, *_):
        pass


def main():
    global args
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--fail-every", type=int, default=0)
    parser.add_argument("--offline-s", type=int, default=0)
    args = parser.parse_args()
    print(f"Mock-Pi lauscht auf Port {args.port}. Beenden mit Strg+C.")
    ThreadingHTTPServer(("0.0.0.0", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
