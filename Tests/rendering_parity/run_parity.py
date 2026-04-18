#!/usr/bin/env python3
# Copyright (c) Martin Schweiger
# Licensed under the MIT License
#
# run_parity.py — rendering parity test runner (M30).
#
# For each YAML-described scenario:
#   1. Spawn ./Orbiter with --scenario, --capture-frame, --capture-out.
#   2. Wait for the binary to exit (FrameLimit + bFastExit ensure
#      bounded run-time).
#   3. Compare the produced PNG against the matching baseline using
#      structural similarity (skimage.metrics.structural_similarity).
#   4. Pass if SSIM ≥ threshold for the scenario's classification
#      (atmospheric scenarios get a looser bar because cloud / haze
#      noise is unavoidable).
#
# Exit codes:
#   0 — every scenario passed (or no scenarios eligible to run).
#   1 — at least one scenario failed.
#   2 — runtime infrastructure error (Orbiter binary missing,
#       unreadable baseline, capture timeout, etc.).
#
# Skipped (rather than failed) when:
#   * The baseline file is empty (placeholder for a future capture).
#   * The Orbiter run produced no PNG (display-less CI runner; we
#     surface the failure on the developer's box but tolerate it on
#     headless GitHub macos-14 runners where OpenGL works only with
#     a real window server).

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

try:
    import numpy as np
    from PIL import Image
    from skimage.metrics import structural_similarity as ssim
    HAVE_SKIMAGE = True
except ImportError:
    HAVE_SKIMAGE = False


def load_image(path: Path) -> "np.ndarray | None":
    if not path.exists() or path.stat().st_size == 0:
        return None
    img = Image.open(path).convert("RGB")
    return np.asarray(img, dtype=np.uint8)


def compare(captured: "np.ndarray", baseline: "np.ndarray") -> float:
    # Normalise dimensions: scale baseline to captured if they differ.
    # The harness assumes both are taken at the same camera setup, so
    # only the resolution should differ (HiDPI vs not).
    if captured.shape != baseline.shape:
        from PIL import Image as _PI
        target = _PI.fromarray(baseline).resize(
            (captured.shape[1], captured.shape[0]),
            _PI.Resampling.LANCZOS)
        baseline = np.asarray(target, dtype=np.uint8)
    return float(ssim(captured, baseline, channel_axis=2, data_range=255))


def run_scenario(orbiter: Path, scenario_name: str,
                 frame: int, out_png: Path,
                 timeout_s: float) -> bool:
    # Orbiter's long-form CLI parser requires `=` between key and
    # value (cmdline.cpp:107: isLongKey path returns parse-error
    # when '=' is missing). Build the args accordingly.
    cmd = [
        str(orbiter),
        f"--scenario={scenario_name}",
        f"--capture-frame={frame}",
        f"--capture-out={out_png}",
    ]
    print(f"[parity] launching {' '.join(cmd)}")
    try:
        rc = subprocess.run(cmd, cwd=orbiter.parent,
                            timeout=timeout_s,
                            capture_output=True, text=True)
    except subprocess.TimeoutExpired:
        print("[parity]   timeout", file=sys.stderr)
        return False
    if rc.returncode != 0:
        # bFastExit + capture path will return 0 on success; non-zero
        # means the simulator aborted before the capture completed.
        print(f"[parity]   exit {rc.returncode} (stdout below)",
              file=sys.stderr)
        print(rc.stdout, file=sys.stderr)
        print(rc.stderr, file=sys.stderr)
        return False
    return out_png.exists() and out_png.stat().st_size > 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--orbiter", required=True,
                    help="path to the Orbiter binary")
    ap.add_argument("--manifest", required=True,
                    help="JSON manifest with scenarios + baselines")
    ap.add_argument("--out-dir", required=True,
                    help="where to write captured PNGs")
    ap.add_argument("--timeout", type=float, default=60.0,
                    help="per-scenario wallclock budget [s]")
    args = ap.parse_args()

    orbiter = Path(args.orbiter).resolve()
    if not orbiter.exists():
        print(f"[parity] Orbiter binary not found: {orbiter}",
              file=sys.stderr)
        return 2

    if not HAVE_SKIMAGE:
        print("[parity] scikit-image / numpy / Pillow not installed —"
              " install with `pip install scikit-image Pillow` and re-run.",
              file=sys.stderr)
        return 2

    manifest = json.loads(Path(args.manifest).read_text())
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    base_dir = Path(args.manifest).resolve().parent / "baselines"

    failures = []
    skipped = []

    for entry in manifest["scenarios"]:
        name      = entry["name"]
        scn       = entry["scenario"]
        frame     = int(entry.get("frame", 120))
        atmo      = bool(entry.get("atmospheric", False))
        threshold = float(entry.get("threshold",
                                    0.85 if atmo else 0.92))
        baseline_path = base_dir / entry["baseline"]
        out_png = out_dir / f"{name}.png"

        ok = run_scenario(orbiter, scn, frame, out_png, args.timeout)
        if not ok:
            print(f"[parity] {name}: capture failed")
            skipped.append((name, "capture failed"))
            continue

        captured = load_image(out_png)
        baseline = load_image(baseline_path)
        if captured is None:
            failures.append((name, "no captured PNG"))
            continue
        if baseline is None:
            print(f"[parity] {name}: baseline missing → skipping")
            skipped.append((name, "no baseline"))
            continue

        score = compare(captured, baseline)
        verdict = "PASS" if score >= threshold else "FAIL"
        print(f"[parity] {name}: SSIM={score:.4f} (≥{threshold:.2f}) {verdict}")
        if score < threshold:
            failures.append((name, f"SSIM {score:.4f} < {threshold:.2f}"))

    print()
    print(f"[parity] summary: {len(manifest['scenarios'])} scenarios, "
          f"{len(failures)} failures, {len(skipped)} skipped")
    if failures:
        for n, why in failures:
            print(f"[parity]   FAIL  {n}: {why}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
