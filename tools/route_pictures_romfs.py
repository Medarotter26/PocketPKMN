#!/usr/bin/env python3
"""Copies the route pictures that romfs/routes/*.txt use (picture=<slug>) from art/routes/ into
romfs/routes/pictures/ (byte-identical; the app reads them at send time). Removes pictures no file uses.
usage: python3 -I tools/route_pictures_romfs.py"""
import os
import re
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RDIR = os.path.join(ROOT, "romfs", "routes")
OUT = os.path.join(RDIR, "pictures")
used = set()
for fn in sorted(os.listdir(RDIR)):
    if fn.endswith(".txt"):
        used |= set(re.findall(r"picture=(\w+)", open(os.path.join(RDIR, fn), encoding="utf-8").read()))
os.makedirs(OUT, exist_ok=True)
for slug in sorted(used):
    src = os.path.join(ROOT, "art", "routes", slug + ".txt")
    if not os.path.exists(src):
        raise SystemExit("missing picture: " + src)
    shutil.copyfile(src, os.path.join(OUT, slug + ".txt"))
for fn in os.listdir(OUT):
    if fn[:-4] not in used:
        os.remove(os.path.join(OUT, fn))
print("%d pictures -> %s" % (len(used), OUT))
