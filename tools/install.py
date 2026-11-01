#!/usr/bin/env python3
"""install.py SOURCE --to DIR [--manifest FILE]: the game's files from a folder or a disc image into
DIR, each checked against the known release (MANIFEST: "sha256 size path" lines; a line for a file
that may be missing starts with "? "). Nothing on the disc is run, only read.

SOURCE is a folder of the extracted game partition or a disc image: a full dump or an extract-xiso
image (an Xbox game partition's file system, XDVDFS, found at the offsets its disc formats use).

Exit codes: 0 done, 2 not the known release, 1 anything else. Prints "<done> <total> <path>" as it
goes, for the launcher. With --list it only writes the manifest of SOURCE to stdout.
"""
import argparse
import hashlib
import os
import shutil
import struct
import sys

SECTOR = 2048
MAGIC = b"MICROSOFT*XBOX*MEDIA"
# Where a dump's game partition starts: an extracted image, XGD1, XGD2, XGD3.
PARTITIONS = (0, 0x18300000, 0x0FD90000, 0x02080000)


class Refused(Exception):
    pass


class Image:
    def __init__(self, path):
        self.f = open(path, "rb")
        for base in PARTITIONS:
            self.f.seek(base + 32 * SECTOR)
            head = self.f.read(28)
            if head[:20] == MAGIC:
                self.base = base
                self.root = struct.unpack("<II", head[20:28])
                return
        raise Refused("this file isn't an Xbox disc image")

    def walk(self):
        """(path, sector, size) of every file"""
        out = []

        def directory(sector, size, prefix):
            self.f.seek(self.base + sector * SECTOR)
            data = self.f.read(size)
            seen = set()

            def entry(offset):
                if offset in seen or offset + 14 > len(data):
                    return
                seen.add(offset)
                left, right, sec, length, attrs, n = struct.unpack("<HHIIBB", data[offset:offset + 14])
                if left == 0xFFFF:                  # (the padding of an empty slot)
                    return
                name = data[offset + 14:offset + 14 + n].decode("latin-1")
                if left:
                    entry(left * 4)
                if attrs & 0x10:
                    if length:
                        directory(sec, length, prefix + name + "/")
                else:
                    out.append((prefix + name, sec, length))
                if right:
                    entry(right * 4)

            entry(0)

        directory(self.root[0], self.root[1], "")
        return out

    def copy(self, sector, size, dst, hasher):
        self.f.seek(self.base + sector * SECTOR)
        left = size
        while left:
            chunk = self.f.read(min(left, 1 << 20))
            if not chunk:
                raise Refused("the image ends early")
            hasher.update(chunk)
            if dst:
                dst.write(chunk)
            left -= len(chunk)


def source_files(src):
    """path (lower case) -> (real path, size, copy function)"""
    files = {}
    if os.path.isdir(src):
        for root, _, names in os.walk(src):
            for n in names:
                p = os.path.join(root, n)
                rel = os.path.relpath(p, src).replace(os.sep, "/")

                def copy(dst, hasher, p=p):
                    with open(p, "rb") as f:
                        while chunk := f.read(1 << 20):
                            hasher.update(chunk)
                            if dst:
                                dst.write(chunk)

                files[rel.lower()] = (rel, os.path.getsize(p), copy)
    else:
        image = Image(src)
        for rel, sector, size in image.walk():
            def copy(dst, hasher, sector=sector, size=size):
                image.copy(sector, size, dst, hasher)

            files[rel.lower()] = (rel, size, copy)
    return files


def read_manifest(path):
    wanted = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            optional = line.startswith("? ")
            sha, size, rel = line[2 if optional else 0:].split(" ", 2)
            wanted.append((sha, int(size), rel, optional))
    return wanted


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("--to")
    ap.add_argument("--manifest", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "disc.manifest"))
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    try:
        files = source_files(a.source)
        if a.list:
            for key in sorted(files):
                rel, size, copy = files[key]
                h = hashlib.sha256()
                copy(None, h)
                print(h.hexdigest(), size, rel)
            return 0
        wanted = read_manifest(a.manifest)
        missing = [w[2] for w in wanted if not w[3] and w[2].lower() not in files]
        if missing:
            print("missing from this copy: " + ", ".join(missing[:5]))
            return 2
        total = sum(w[1] for w in wanted if w[2].lower() in files)
        done = 0
        for sha, size, rel, optional in wanted:
            found = files.get(rel.lower())
            if not found:
                continue
            dst = os.path.join(a.to, *rel.split("/"))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            h = hashlib.sha256()
            with open(dst, "wb") as out:
                found[2](out, h)
            if h.hexdigest() != sha:
                print(rel + " differs from the known release")
                return 2
            done += size
            print(done, total, rel, flush=True)
        return 0
    except Refused as e:
        print(e)
        return 2
    except OSError as e:
        print(e)
        return 1


if __name__ == "__main__":
    sys.exit(main())
