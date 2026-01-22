#!/usr/bin/env python3
import json
import uuid
from datetime import datetime
from http.server import BaseHTTPRequestHandler, HTTPServer

JOBS = []


def iso_now():
    return datetime.utcnow().isoformat() + "Z"


class Handler(BaseHTTPRequestHandler):
    def _send(self, code, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_json(self):
        length = int(self.headers.get("Content-Length", "0"))
        if length == 0:
            return {}
        raw = self.rfile.read(length)
        try:
            return json.loads(raw.decode("utf-8"))
        except Exception:
            return {}

    def do_GET(self):
        if self.path == "/health":
            self._send(200, {"status": "ok"})
            return
        if self.path == "/api/jobs":
            self._send(200, JOBS)
            return
        self._send(404, {"error": "not found"})

    def do_POST(self):
        if self.path not in ("/api/build", "/api/test"):
            self._send(404, {"error": "not found"})
            return

        payload = self._read_json()
        kind = "build" if self.path == "/api/build" else "test"
        job = {
            "id": str(uuid.uuid4()),
            "kind": kind,
            "status": "queued",
            "createdAt": iso_now(),
            "message": payload.get("payload", {}).get("project_path", ""),
        }
        JOBS.insert(0, job)
        self._send(200, job)

    def log_message(self, fmt, *args):
        return


def main():
    server = HTTPServer(("0.0.0.0", 8787), Handler)
    print("Remote build server listening on http://0.0.0.0:8787")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
