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


def resize_to_earth_png(src: Path, dst: Path, width: int, height: int) -> None:
    """Load the full-resolution NASA mosaic and produce a much smaller
    equirectangular `Earth.png` the OGL planet renderer can load as its base
    albedo texture. Keeping the big upstream file cached alongside means
    subsequent configures skip both download and resize when the target is
    already up to date."""
    try:
        from PIL import Image
    except ImportError:
        raise SystemExit(
            "[earth] Pillow (PIL) is required to resize the mosaic. "
            "Install with `pip3 install --user pillow` and re-run configure.")

    if dst.exists():
        # Cheap existence check — the file size of a 4096×2048 RGB PNG sits in
        # a predictable band. We compare against a rough floor so a truncated
        # previous run re-generates; SHA pinning would require shipping a
        # pre-computed hash that depends on Pillow's PNG encoder, which is
        # worse for reproducibility than just regenerating when suspicious.
        if dst.stat().st_size > 1_000_000:
            print(f"[earth] {dst.name} already present, skip resize")
            return

    print(f"[earth] resizing {src.name} → {width}×{height} {dst.name}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    with Image.open(src) as im:
        im = im.convert("RGB")
        im = im.resize((width, height), Image.LANCZOS)
        im.save(dst, format="PNG", optimize=True)
    print(f"[earth] wrote {dst} ({dst.stat().st_size / 1024 / 1024:.1f} MiB)")


def main() -> int:
    ap = argparse.ArgumentParser(description=(
        "Download the NASA Blue Marble Next Generation mosaic and optionally "
        "resample it into an equirectangular Earth.png that the OGL planet "
        "renderer loads as its base albedo. Full LOD tile-pyramid generation "
        "(Textures/Earth/Archive/Surf.tree) is still a separate offline step "
        "— see the header comment."))
    ap.add_argument("--out", default=None,
                    help="download destination; defaults to Textures/EarthBlueMarble.png")
    ap.add_argument("--earth-png", default=None,
                    help="if set, resize the downloaded mosaic to this path "
                         "(e.g. Textures/Earth.png) so the OGL renderer picks "
                         "it up without needing a full LOD pyramid")
    ap.add_argument("--earth-png-width", type=int, default=4096,
                    help="width in pixels for the resized Earth.png (default 4096)")
    ap.add_argument("--earth-png-height", type=int, default=2048,
                    help="height in pixels for the resized Earth.png (default 2048)")
    args = ap.parse_args()

    spec = BLUEMARBLE_SOURCES[0]
    out = Path(args.out) if args.out else Path(spec["out_default"])
    try:
        fetch(spec, out)
    except Exception as e:
        print(f"[earth] download failed: {e}", file=sys.stderr)
        return 1

    if args.earth_png:
        try:
            resize_to_earth_png(out, Path(args.earth_png),
                                args.earth_png_width, args.earth_png_height)
        except Exception as e:
            print(f"[earth] resize failed: {e}", file=sys.stderr)
            return 2
    else:
        print()
        print("[earth] Next steps (manual):")
        print("[earth]   1. Resample to a compact equirectangular via --earth-png")
        print("[earth]      to get the OGL renderer's base albedo.")
        print("[earth]   2. Tile-split the mosaic into Textures/Earth/Archive/Surf.tree")
        print("[earth]      for full LOD zoom (separate offline pipeline).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
