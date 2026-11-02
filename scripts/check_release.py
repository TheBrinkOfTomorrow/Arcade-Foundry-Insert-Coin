#!/usr/bin/env python3
"""Check that the app's version numbers agree before a release.

  check_release.py [VERSION]

Reads MARKETING_VERSION in apple/project.yml, the project() VERSION in
linux/CMakeLists.txt and the newest entry in RELEASE-NOTES.md; they must be
the same x.y.z, and the same as VERSION when one is given (the tag about to
be made). Exits 1 with every mismatch listed, so release.sh can stop before
tagging.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        return f.read()


def main():
    found = {}
    m = re.search(r'MARKETING_VERSION:\s*"(\d+\.\d+\.\d+)"', read("apple/project.yml"))
    found["apple/project.yml MARKETING_VERSION"] = m.group(1) if m else None
    m = re.search(r"project\(insert_coin VERSION (\d+\.\d+\.\d+)", read("linux/CMakeLists.txt"))
    found["linux/CMakeLists.txt project() VERSION"] = m.group(1) if m else None
    m = re.search(r"^## (\d+\.\d+\.\d+)", read("RELEASE-NOTES.md"), re.M)
    found["RELEASE-NOTES.md newest entry"] = m.group(1) if m else None
    if len(sys.argv) > 1:
        found["the version given"] = sys.argv[1].lstrip("v")
    versions = set(found.values())
    for where, v in found.items():
        print(f"{where}: {v or 'not found'}")
    if None in versions or len(versions) != 1:
        print("check_release: the versions disagree", file=sys.stderr)
        return 1
    print(f"check_release: {versions.pop()} everywhere")
    return 0


if __name__ == "__main__":
    sys.exit(main())
