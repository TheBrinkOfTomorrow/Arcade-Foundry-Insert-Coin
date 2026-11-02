#!/usr/bin/env python3
"""Checks a ROM server the way the app uses it, and prints what happens at
each step, so a failing download shows where it goes wrong:

  rom_server_test.py URL [--sets srallyc hotd4a ...] [--download SET]

URL is the address typed into the app (for example
http://192.168.1.10:8000). The steps:

  1. the address: a URL with http:// or https://, as the app needs
  2. the host name, looked up (DNS, or the .local name on the network)
  3. a TCP connection to the host's port
  4. a request for the address itself (any answer means a web server is there)
  5. for each set, the names the app asks for, <URL>/<name>.zip, .7z, or
     .tar for a Lindbergh game, until one is found (as the app does)
  6. with --download, the whole of one set's file, read and thrown away,
     to time it

Only the standard library is used, so it runs with the python3 on macOS or
Linux. Nothing is written to disk.
"""

import argparse
import http.client
import os
import socket
import ssl
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rom_index import names  # noqa: E402

TIMEOUT = 30  # seconds, as the app (RomServer.session)
ROM_EXTENSIONS = ("zip", "7z")  # Storage.romExtensions
LINDBERGH = ("hotd4a", "letsgoju", "segartv", "outr2sdx", "initiad4")  # folder sets, served as <set>.tar
DEFAULT_SETS = ["srallyc", "daytona", "vf3", "hotd4a"]


def ok(msg):
    print(f"  OK    {msg}")


def fail(msg, hint=None):
    print(f"  FAIL  {msg}")
    if hint:
        print(f"        {hint}")


def size(n):
    for unit in ("bytes", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return f"{n:.0f} {unit}" if unit == "bytes" else f"{n:.1f} {unit}"
        n /= 1024


# Straight to the server, as the app goes: no proxy from http_proxy and the like.
OPENER = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def request(url, method="GET", headers=None):
    """(status, headers, response) for url, following redirects; raises on no answer."""
    req = urllib.request.Request(url, method=method, headers={"User-Agent": "rom_server_test", **(headers or {})})
    try:
        r = OPENER.open(req, timeout=TIMEOUT)
        return r.status, r.headers, r
    except urllib.error.HTTPError as e:  # an answer, just not 200
        return e.code, e.headers, e


def check_address(url):
    print(f"1. Address: {url}")
    u = urllib.parse.urlsplit(url.strip())
    if u.scheme not in ("http", "https") or not u.hostname:
        fail("not a URL the app can use", "Type it with http:// in front, for example http://192.168.1.10:8000")
        return None
    port = u.port or (443 if u.scheme == "https" else 80)
    ok(f"{u.scheme}, host {u.hostname}, port {port}, path {u.path or '/'}")
    if u.path.strip("/"):
        print("        Note: serve_roms.py answers only <URL>/<set>.zip with no folder in the path.")
        print("        If you run serve_roms.py, the address should end at the port (no /roms).")
    return u, port


def check_dns(host):
    print(f"\n2. Looking up {host}")
    t = time.monotonic()
    try:
        infos = socket.getaddrinfo(host, None, type=socket.SOCK_STREAM)
    except socket.gaierror as e:
        fail(f"cannot find the host: {e}",
             "Check the spelling, or use the server's IP address (on the server: ipconfig getifaddr en0 on a Mac, "
             "hostname -I on Linux).")
        return []
    addrs = sorted({i[4][0] for i in infos})
    ok(f"{', '.join(addrs)} ({(time.monotonic() - t) * 1000:.0f} ms)")
    return addrs


def check_tcp(host, port, addrs):
    print(f"\n3. Connecting to port {port}")
    reached = False
    for addr in addrs:
        t = time.monotonic()
        try:
            with socket.create_connection((addr, port), timeout=10):
                ok(f"{addr}:{port} answered ({(time.monotonic() - t) * 1000:.0f} ms)")
                reached = True
        except ConnectionRefusedError:
            fail(f"{addr}:{port} refused the connection",
                 "Nothing is listening on that port: is the server running, and on this port?")
        except socket.timeout:
            fail(f"{addr}:{port} did not answer in 10 s",
                 "A firewall may be dropping it (macOS: System Settings > Network > Firewall on the server), "
                 "or the address is on another network.")
        except OSError as e:
            fail(f"{addr}:{port}: {e}",
                 "No route to the host: is this machine on the same network? On macOS, Local Network "
                 "access for Terminal (System Settings > Privacy & Security > Local Network) can block this too.")
    return reached


def check_root(url):
    print(f"\n4. Asking for {url}")
    t = time.monotonic()
    try:
        status, headers, r = request(url)
        r.close()
    except ssl.SSLError as e:
        fail(f"TLS failed: {e}", "Use http:// unless the server really serves https.")
        return False
    except (urllib.error.URLError, OSError, http.client.HTTPException) as e:
        fail(f"no answer: {getattr(e, 'reason', e)}")
        return False
    ok(f"HTTP {status} in {(time.monotonic() - t) * 1000:.0f} ms, server: {headers.get('Server', '?')}")
    if status == 404:
        print("        (404 here is fine for serve_roms.py, which serves only the set files)")
    return True


def set_files(s):
    exts = ("tar",) if s in LINDBERGH else ROM_EXTENSIONS
    return [f"{n}.{e}" for n in names(s) for e in exts]


def check_set(base, s):
    """The URL of the file the app would download for set s, or None."""
    print(f"\n   {s}:")
    for name in set_files(s):
        url = f"{base}/{urllib.parse.quote(name)}"
        t = time.monotonic()
        try:
            # A ranged GET: some servers do not answer HEAD, and this reads only one byte.
            status, headers, r = request(url, headers={"Range": "bytes=0-0"})
            r.close()
        except (urllib.error.URLError, OSError, http.client.HTTPException) as e:
            fail(f"{name}: no answer: {getattr(e, 'reason', e)}")
            return None
        ms = (time.monotonic() - t) * 1000
        if status in (200, 206):
            total = headers.get("Content-Range", "").rpartition("/")[2] or headers.get("Content-Length")
            ok(f"{name}: HTTP {status}, {size(int(total)) if total and total.isdigit() else 'size unknown'} "
               f"({ms:.0f} ms)")
            if status == 200 and headers.get("Content-Length") is None:
                print("        Note: no Content-Length, so the app cannot show progress.")
            return url
        print(f"  --    {name}: HTTP {status} ({ms:.0f} ms)")
    fail(f"none of {', '.join(set_files(s))} is on the server",
         "The app would say the set is not on the server. Check the file name and that it is in the served folder.")
    return None


def download(url):
    print(f"\n6. Downloading {url}")
    t = time.monotonic()
    got = 0
    try:
        status, headers, r = request(url)
        if status != 200:
            fail(f"HTTP {status}")
            return
        last = t
        while chunk := r.read(1 << 20):
            got += len(chunk)
            if time.monotonic() - last > 2:
                last = time.monotonic()
                print(f"        {size(got)} ({got / (last - t) / 1e6:.1f} MB/s)")
        r.close()
    except (urllib.error.URLError, OSError, http.client.HTTPException) as e:
        fail(f"stopped after {size(got)}: {getattr(e, 'reason', e)}")
        return
    secs = time.monotonic() - t
    expected = headers.get("Content-Length")
    if expected and int(expected) != got:
        fail(f"got {size(got)} of {size(int(expected))}")
        return
    ok(f"{size(got)} in {secs:.1f} s ({got / max(secs, 1e-6) / 1e6:.1f} MB/s)")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("url", help="the ROM server address, as typed into the app")
    ap.add_argument("--sets", nargs="+", default=DEFAULT_SETS, help=f"sets to ask for (default {' '.join(DEFAULT_SETS)})")
    ap.add_argument("--download", metavar="SET", help="also download this set's whole file, to time it")
    args = ap.parse_args()

    parsed = check_address(args.url)
    if not parsed:
        sys.exit(1)
    u, port = parsed
    addrs = check_dns(u.hostname)
    if not addrs or not check_tcp(u.hostname, port, addrs):
        sys.exit(1)
    base = urllib.parse.urlunsplit((u.scheme, u.netloc, u.path.rstrip("/"), "", ""))
    if not check_root(base + "/"):
        sys.exit(1)

    print("\n5. Asking for sets, as the app does")
    found = {s: check_set(base, s) for s in dict.fromkeys(args.sets + ([args.download] if args.download else []))}
    if args.download:
        if found.get(args.download):
            download(found[args.download])
        else:
            print(f"\n6. Not downloading {args.download}: it is not on the server")

    missing = [s for s, url in found.items() if not url]
    print("\nResult:", "the server works for every set asked for." if not missing else
          f"the server answers, but {', '.join(missing)} {'is' if len(missing) == 1 else 'are'} not on it.")
    sys.exit(1 if missing else 0)


if __name__ == "__main__":
    main()
