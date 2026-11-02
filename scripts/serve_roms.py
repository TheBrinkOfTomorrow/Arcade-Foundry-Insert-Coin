#!/usr/bin/env python3
"""A ROM server for the app, from a folder that may keep its sets in a
folder per board (DIR/Model2/srallyc.zip):

  serve_roms.py --roms DIR [--port 8000]

The app asks for <URL>/<set>.zip (or .7z, .tar); this answers each name from
wherever it is in DIR or its board folders (scripts/rom_index.py: the
copy at the top if there are two). The folder is searched again when a name is not found, so
sets added while it runs are served too. Only those files are served,
read-only; nothing else in DIR can be fetched.
"""

import argparse
import http.server
import os
import shutil
import socket
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rom_index import index  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--roms", required=True, help="folder with your ROM sets (or its board folders)")
    ap.add_argument("--port", type=int, default=8000)
    args = ap.parse_args()
    if not os.path.isdir(args.roms):
        sys.exit(f"serve_roms: no folder {args.roms}")
    sets = {k: v for k, v in index(args.roms).items() if os.path.isfile(v)}

    class Handler(http.server.BaseHTTPRequestHandler):
        def send_set(self, body):
            nonlocal sets
            name = self.path.split("?", 1)[0].lstrip("/")
            if name not in sets:
                sets = {k: v for k, v in index(args.roms, warn=False).items() if os.path.isfile(v)}
            path = sets.get(name)
            if not path or "/" in name:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(os.path.getsize(path)))
            self.end_headers()
            if body:
                with open(path, "rb") as f:
                    shutil.copyfileobj(f, self.wfile)

        def do_GET(self):
            self.send_set(True)

        def do_HEAD(self):
            self.send_set(False)

    print(f"serve_roms: {len(sets)} files from {args.roms} on port {args.port}")
    try:
        server = DualStack(("::", args.port), Handler)
    except OSError:  # no IPv6 on this machine
        server = http.server.ThreadingHTTPServer(("", args.port), Handler)
    server.serve_forever()


class DualStack(http.server.ThreadingHTTPServer):
    """Listens on IPv6 and IPv4: a .local name gives IPv6 addresses too, and
    the app tries those first."""
    address_family = socket.AF_INET6

    def server_bind(self):
        self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        super().server_bind()


if __name__ == "__main__":
    main()
