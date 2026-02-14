#!/usr/bin/env python3
"""Serve static files with COI headers required for SharedArrayBuffer/WASM."""

from __future__ import annotations

import argparse
import http.server
import os
import socketserver


class CoiStaticHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self) -> None:
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Serve static files with COI headers."
    )
    parser.add_argument("--port", type=int, default=8000, help="Port to bind.")
    parser.add_argument(
        "--directory",
        default=".",
        help="Directory to serve.",
    )
    args = parser.parse_args()

    os.chdir(args.directory)
    with socketserver.TCPServer(("127.0.0.1", args.port), CoiStaticHandler) as httpd:
        print(f"[serve_static_with_coi] Serving {os.getcwd()} on 127.0.0.1:{args.port}")
        httpd.serve_forever()


if __name__ == "__main__":
    main()
