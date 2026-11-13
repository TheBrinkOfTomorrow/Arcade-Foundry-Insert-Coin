#!/usr/bin/env python3
"""Read a FATX file system image: a Chihiro GD-ROM image dumped decrypted
("Crazy Taxi High Roller (Rev B) (GDX-0002B).bin" starts with "FATX").

  fatx.py list IMAGE            every file with its size and offset
  fatx.py extract IMAGE DIR     the files into DIR (a new folder)

FATX (the Xbox's FAT): a 4 KB header ("FATX", volume id, sectors per
cluster, root directory cluster), the FAT from 0x1000 (16-bit entries
under 0xfff0 clusters, else 32-bit, its size rounded up to 4 KB), then the
clusters, numbered from 1. The FAT is sized for the whole volume, which a
dump can be trimmed from, so the reader looks for the root directory
rather than trusting the file's size. A directory is a chain of 64-byte entries:
name length (0xe5 deleted; 0x00 or 0xff ends the directory), attributes
(0x10 a directory), 42 bytes of name, first cluster, size, three times.
The image is only read.
"""

import os
import struct
import sys

SECTOR = 512
HEADER = 0x1000


def looks_like_entry(e):
    """A directory entry: name length 1-42, attributes FAT-like, a printable name."""
    if len(e) < 64 or not 1 <= e[0] <= 42 or e[1] & ~0x37:
        return False
    return all(0x20 <= b < 0x7f for b in e[2:2 + e[0]])


class Fatx:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.size = os.fstat(self.f.fileno()).st_size
        head = self.f.read(18)
        if len(head) < 18 or head[:4] != b"FATX":
            raise ValueError(f"{path}: not a FATX image (no FATX at its start)")
        _vol, spc, self.root = struct.unpack_from("<III", head, 4)
        if not spc or spc & (spc - 1):
            raise ValueError(f"{path}: odd sectors per cluster {spc}")
        self.cluster = spc * SECTOR
        # The FAT is sized for the whole volume, which a dump may be trimmed
        # from (Crazy Taxi's GDX-0002B: a 512 MB volume, 0x10000 bytes of FAT,
        # in a 417 MB file). So try the image's own size, then the volume
        # sizes it could be cut from, then every 4 KB step, and keep the first
        # layout whose root directory starts with a real entry.
        for wide, fat_bytes in self.layouts():
            self.f.seek(HEADER + fat_bytes + (self.root - 1) * self.cluster)
            if looks_like_entry(self.f.read(64)):
                break
        else:
            raise ValueError(f"{path}: no root directory found after the FAT")
        self.wide = wide
        self.f.seek(HEADER)
        self.fat = self.f.read(fat_bytes)
        self.data = HEADER + fat_bytes
        self.end = 0xfffffff0 if self.wide else 0xfff0

    def layouts(self):
        """(32-bit FAT, FAT bytes) to try, most likely first."""
        def fat_for(volume):
            clusters = (volume - HEADER) // self.cluster
            wide = clusters >= 0xfff0
            fat = (clusters + 1) * (4 if wide else 2)
            return wide, (fat + 0xfff) & ~0xfff
        tried = []
        volume = 1 << max(self.size - 1, 1).bit_length()
        for v in (self.size, volume, volume * 2):
            tried.append(fat_for(v))
        for fat in range(0x1000, 0x80000 + 1, 0x1000):
            for wide in (False, True):
                tried.append((wide, fat))
        seen = set()
        for t in tried:
            if t not in seen:
                seen.add(t)
                yield t

    def next(self, c):
        if self.wide:
            return struct.unpack_from("<I", self.fat, c * 4)[0]
        return struct.unpack_from("<H", self.fat, c * 2)[0]

    def offset(self, c):
        return self.data + (c - 1) * self.cluster

    def chain(self, first):
        c, seen = first, set()
        while 0 < c < self.end:
            if c in seen:
                raise ValueError(f"FAT loop at cluster {c}")
            seen.add(c)
            yield c
            c = self.next(c)

    def read_chain(self, first, size=None):
        out = bytearray()
        for c in self.chain(first):
            self.f.seek(self.offset(c))
            out += self.f.read(self.cluster)
            if size is not None and len(out) >= size:
                break
        return bytes(out if size is None else out[:size])

    def entries(self, first):
        """(name, is_dir, first cluster, size) of a directory's entries."""
        raw = self.read_chain(first)
        for i in range(0, len(raw) - 63, 64):
            n, attr = raw[i], raw[i + 1]
            if n in (0x00, 0xff):
                return
            if n == 0xe5 or n > 42:
                continue
            name = raw[i + 2:i + 2 + n].decode("latin-1")
            first_c, size = struct.unpack_from("<II", raw, i + 44)
            yield name, bool(attr & 0x10), first_c, size

    def walk(self, first=None, prefix=""):
        """Every file as (path, first cluster, size), directories first-come."""
        for name, is_dir, c, size in self.entries(self.root if first is None else first):
            if name in (".", "..") or "/" in name or "\\" in name:
                continue
            path = prefix + name
            if is_dir:
                yield from self.walk(c, path + "/")
            else:
                yield path, c, size

    def contiguous(self, first, size):
        """The file's offset in the image when its clusters follow one another, else None."""
        prev = None
        need = (size + self.cluster - 1) // self.cluster
        for k, c in enumerate(self.chain(first)):
            if k >= need:
                break
            if prev is not None and c != prev + 1:
                return None
            prev = c
        return self.offset(first) if first else None

    def extract(self, dst):
        os.makedirs(dst)
        for path, c, size in self.walk():
            out = os.path.join(dst, *path.split("/"))
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with open(out, "wb") as o:
                left = size
                for cl in self.chain(c):
                    if left <= 0:
                        break
                    self.f.seek(self.offset(cl))
                    chunk = self.f.read(min(self.cluster, left))
                    o.write(chunk)
                    left -= len(chunk)
                if left > 0:
                    raise ValueError(f"{path}: the chain ends {left} bytes short")


def main():
    if len(sys.argv) < 3 or sys.argv[1] not in ("list", "extract") or (sys.argv[1] == "extract") != (len(sys.argv) == 4):
        sys.exit(__doc__)
    img = Fatx(sys.argv[2])
    if sys.argv[1] == "list":
        print(f"cluster {img.cluster} bytes, root cluster {img.root}, {'32' if img.wide else '16'}-bit FAT, data at 0x{img.data:x}")
        for path, c, size in img.walk():
            at = img.contiguous(c, size)
            print(f"{size:12d}  {'0x%x' % at if at is not None else 'fragmented':>12}  {path}")
    else:
        img.extract(sys.argv[3])


if __name__ == "__main__":
    main()
