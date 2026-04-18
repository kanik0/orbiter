#!/usr/bin/env python3
# Copyright (c) Martin Schweiger
# Licensed under the MIT License
#
# download_earth_lod8.py — fetch the NASA Blue Marble Next Generation
# July 2004 mosaic (CC0 public domain) into the build tree so the OGL
# planet renderer can serve a global Earth texture.
#
# The script intentionally only fetches the source mosaic. The
# tile-pyramid step (PNG → DXT5 + quad-tree split that produces the
# LOD 1..8 tiles Orbiter loads from Textures2/Earth/<level>/<x>_<y>.dds)
# is a separate offline pipeline because it requires a hardware
# texture compressor (texconv on Windows, compressonatorcli on
# macOS / Linux) that we cannot bundle as a CMake dependency without
# pulling in tens of MB of extra tooling.
#
# Invocation:
#   * Triggered automatically by macos_assets.cmake when the build is
#     configured with -DORBITER_FETCH_EARTH_BLUEMARBLE=ON.
#   * Can also be run by hand:
#       python3 cmake/download_earth_lod8.py \
#           --out Textures/EarthBlueMarble.png
#
# All downloads are SHA-256 verified against pinned hashes; the script
# fails closed if the upstream payload changes.

from __future__ import annotations

import argparse
import hashlib
import os
import sys
import urllib.request
from pathlib import Path

# Pinned NASA Blue Marble Next Generation, July 2004, ~5400×2700,
# 21,600px equirectangular composite. Hosted on visibleearth.nasa.gov;
# CC0 / public domain (US government work).
BLUEMARBLE_SOURCES = [
    {
        "name": "Blue Marble 2002 Next Generation (21,600×10,800, ~530 MiB)",
        # Wikimedia Commons object-store URL — stable since 2020-09;
        # original NASA Visible Earth source restructured the eoimages.
        # gsfc.nasa.gov tree multiple times.
        "url":  "https://upload.wikimedia.org/wikipedia/commons/2/23/Blue_Marble_2002.png",
        "sha256": "54a7f9975de38c843fed8db65339bf44fd03d08de5ee799bfaabac6e31507246",
        "out_default": "Textures/EarthBlueMarble.png",
    },
]


def http_get(url: str) -> bytes:
    req = urllib.request.Request(url, headers={
        "User-Agent": "Orbiter-macOS-port/1.0 (CMake build)"
    })
    with urllib.request.urlopen(req, timeout=120) as resp:
        return resp.read()


def fetch(spec, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)

    if out_path.exists() and spec["sha256"]:
        h = hashlib.sha256(out_path.read_bytes()).hexdigest()
        if h == spec["sha256"]:
            print(f"[earth] {out_path.name} already present (sha256 match), skip")
            return

    print(f"[earth] fetching {spec['name']}")
    print(f"[earth]   url: {spec['url']}")
    data = http_get(spec["url"])
    if spec["sha256"]:
        h = hashlib.sha256(data).hexdigest()
        if h != spec["sha256"]:
            raise SystemExit(f"[earth] SHA-256 mismatch for {spec['name']} "
                             f"(expected {spec['sha256']}, got {h})")
    out_path.write_bytes(data)
    print(f"[earth] wrote {out_path} ({len(data) / 1024 / 1024:.1f} MiB)")


def main() -> int:
    ap = argparse.ArgumentParser(description=(
        "Download the NASA Blue Marble Next Generation mosaic into the "
        "Orbiter build tree. Tile-pyramid generation is a separate "
        "offline step (see header comment)."))
    ap.add_argument("--out", default=None,
                    help="destination path; defaults to Textures/EarthBlueMarble.jpg")
    args = ap.parse_args()

    spec = BLUEMARBLE_SOURCES[0]
    out = Path(args.out) if args.out else Path(spec["out_default"])
    try:
        fetch(spec, out)
    except Exception as e:
        print(f"[earth] download failed: {e}", file=sys.stderr)
        return 1

    print()
    print("[earth] Next steps (manual):")
    print("[earth]   1. Tile-split the JPEG into Textures2/Earth/<lvl>/<x>_<y>.dds")
    print("[earth]      using texconv (Windows) or compressonatorcli (POSIX).")
    print("[earth]   2. Levels 1..8 cover the full globe at increasing resolution;")
    print("[earth]      see Doc/HiResTextures.htm for the on-disk layout.")
    print("[earth]   3. Drop the resulting tree into Textures2/Earth/ and the")
    print("[earth]      OGL planet renderer will pick it up at scenario load.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
