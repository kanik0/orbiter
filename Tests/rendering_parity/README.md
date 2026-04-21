# Rendering parity harness

`run_parity.py` spawns the freshly-built `Orbiter` binary once per entry
in `manifest.json`, drives it with `--scenario` and `--capture-frame`,
and then SSIM-compares the captured PNG against the corresponding file
under `baselines/`. Scenarios whose baseline is a 0-byte placeholder are
reported as **skipped** rather than failed — drop a real PNG in place to
start gating on it.

## Layout

- `manifest.json` — the list of scenarios (name, scenario path, capture
  frame, baseline filename, SSIM threshold).
- `baselines/*.png` — reference captures. Empty (0-byte) files act as
  placeholders that the harness skips.
- `run_parity.py` — the CTest driver.

## Refreshing baselines

Real baselines are captured on the `macos-14` GitHub runner so CI
compares against the same rasterization backend that future PRs will
run. To generate them:

1. Trigger the **Regenerate parity baselines** workflow in the Actions
   tab (choose `release` configuration unless you're specifically
   debugging a debug-build regression).
2. Wait for the job to finish and download the `Orbiter-parity-captures-*`
   artifact. Each captured PNG in the artifact is named after the
   scenario's `name` field in `manifest.json`.
3. Copy the PNGs that you want to promote into
   `Tests/rendering_parity/baselines/`, replacing the empty placeholders.
   **Eyeball the image first** — the point of baselines is that a human
   has accepted the reference frame.
4. Commit the new PNGs in a dedicated PR (no code changes) so reviewers
   can diff with `git show --stat` and the binary comparison is easy to
   revert if something's off.

Once every scenario in `manifest.json` has a real baseline, flip
`continue-on-error` to `false` in
`.github/workflows/reusable-build-macos.yml` so the parity step starts
gating PRs.

## Adding a new scenario

1. Append a new object to the `scenarios` array in `manifest.json`.
2. Touch an empty file at `baselines/<name>.png` so git tracks the slot.
3. Follow the refresh procedure above to capture the real baseline.
