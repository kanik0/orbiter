# QA Phase 2 — Closing Report

**Data chiusura:** 2026-04-18
**Branch:** `qa/phase2-manual` (storico); fix merged via PR #24, #27, #28
**Scope:** verifica interattiva human-in-the-loop su macOS ARM64 (Apple M2 / Darwin 25.4.0)

---

## TL;DR

Phase 2 ha esposto **23 finding (P2-B1 → P2-B23)** distribuiti su Launchpad UI, dialog in-sim, rendering 3D, audio e I/O scenarios. Di questi:

- **3 RESOLVED** via 3 PR mergiate su main durante la sessione
- **17 FILED** come GitHub issues (#29 → #45) per iterazione successiva
- **1 CLOSED** come falso positivo
- **2 SKIPPED** per dipendenze (controller, MFD plugins)

Il porting macOS è in uno stato **"foundation working, polish + rendering-completeness needed"**: il Launchpad UI è completamente funzionante, i dialog base fanno il loro lavoro, la build si completa, il DMG installa e lancia — ma la scena 3D è degradata visivamente, cockpit/VC/2D-panel/map sono rotti, e alcune scorciatoie di dialog non rispondono.

## Verdict per STEP

| step | area | verdict | note |
|---|---|---|---|
| 1.1 | Launchpad boot + Scenarios | ✅ PASS | P2-B1 |
| 1.2 | Scenario select + render | ✅ PASS | P2-B2, P2-B3→RESOLVED, P2-B4/B6 → #25/#26 |
| 1.3 | Options tab 12 pages | ✅ PASS | P2-B7 |
| 1.4 | Modules tab | ✅ PASS | P2-B8, P2-B9 |
| 1.5 | Video tab | ⚠️ PARTIAL | P2-B10, P2-B11 |
| 1.6a | Extra tab | ✅ PASS | P2-B12 → RESOLVED via PR #27 |
| 1.6b | About tab | ✅ PASS | P2-B1 recurrent |
| 2.1 | Scenario runtime render | ⚠️ PARTIAL | P2-B13, P2-B14, P2-B15, P2-B16, P2-B17 |
| 2.2-2.3 | Other scenarios | ⏭️ SKIP | same render path as 2.1 |
| 3 | 7 dialog-shortcuts | ⚠️ PARTIAL 4/7 | P2-B18, P2-B19, P2-B20 |
| 4 | Custom funcs | ✅ PASS | joystick + keymap editor ok; plugin-based skipped |
| 5 | Plugin runtime | ⚠️ PARTIAL | activate OK, P2-B21 icon miss; MFD-based blocked |
| 6 | Audio XRSound | ⚠️ PARTIAL | init OK, P2-B22 no audio events |
| 7 | Joystick | ⏭️ SKIP | no controller |
| 8 | Save/Load | ✅ PASS | P2-B23 → RESOLVED via PR #28 |
| 9 | Window geom persist | ✅ PASS | M22.h confermato |
| 10 | DMG install | ✅ PASS | ad-hoc DMG opens + launches |

**Sommario**: 8 PASS, 6 PARTIAL, 2 SKIP. 3 fix applicati inline (P2-B3, P2-B12, P2-B23).

## Fix applicati durante la sessione

| PR | commit | fix |
|---|---|---|
| [#24](https://github.com/kanik0/orbiter/pull/24) | `c0ac8442` | M11 post-process black-screen (OGLEnvMap FBO save/restore, GL_RGBA8 fallback on Apple Silicon, ACES tonemap LDR path) — unlocked 3D rendering |
| [#27](https://github.com/kanik0/orbiter/pull/27) | `34d70589` + `7e7b99e9` | Plugin InitModule systemic failure (Launchpad registry order + POSIX bootstrap `dladdr` self-handle) — unlocked plugin registrations |
| [#28](https://github.com/kanik0/orbiter/pull/28) | `a056b4a3` | Quicksave Win32 backslash path leak — unlocked Ctrl+S save |

Tutte mergiate in main. Senza questi fix l'intera Phase 2 sarebbe stata **"HUD only / no save / no plugins"**, un deliverable completamente degradato.

## Issue filed (17)

### High severity (3)
- [#25](https://github.com/kanik0/orbiter/issues/25) Planet red channel missing (Phase 1 P2-B4)
- [#37](https://github.com/kanik0/orbiter/issues/37) F1 cockpit view empty (P2-B14)
- [#45](https://github.com/kanik0/orbiter/issues/45) XRSound no audio events (P2-B22)

### Medium severity (8)
- [#26](https://github.com/kanik0/orbiter/issues/26) M4 atmospheric scattering not visible (Phase 1 P2-B6)
- [#33](https://github.com/kanik0/orbiter/issues/33) XRSound no `.info` (P2-B9)
- [#34](https://github.com/kanik0/orbiter/issues/34) Video tab Width/Height garbage (P2-B10)
- [#36](https://github.com/kanik0/orbiter/issues/36) Planet texture chunky (P2-B13)
- [#39](https://github.com/kanik0/orbiter/issues/39) DlgMap empty (P2-B16)
- [#41](https://github.com/kanik0/orbiter/issues/41) Ctrl+F1 DlgCamera no response (P2-B18)
- [#42](https://github.com/kanik0/orbiter/issues/42) Ctrl+F2 DlgTacc frozen (P2-B19)
- [#43](https://github.com/kanik0/orbiter/issues/43) Alt+F1 DlgHelp no response (P2-B20)

### Low / cosmetic (6)
- [#29](https://github.com/kanik0/orbiter/issues/29) Em-dash glyph `?` (P2-B1)
- [#30](https://github.com/kanik0/orbiter/issues/30) Launch button no disabled state (P2-B2)
- [#31](https://github.com/kanik0/orbiter/issues/31) Tree-expand glyph `?` (P2-B7)
- [#32](https://github.com/kanik0/orbiter/issues/32) AscentMFD sample exposed (P2-B8)
- [#35](https://github.com/kanik0/orbiter/issues/35) Video tab no MSAA (P2-B11)
- [#38](https://github.com/kanik0/orbiter/issues/38) DlgInfo default Sol (P2-B15)
- [#40](https://github.com/kanik0/orbiter/issues/40) Shift+F1 MFD selector (P2-B17)
- [#44](https://github.com/kanik0/orbiter/issues/44) Missing menu icons (P2-B21)

## Stato consolidato porting macOS

### ✅ Operativo
- Launchpad UI (6 tab completi, 12 options pages, 11+2 plugins, Extra tab con plugin-registered items, About con credits)
- Window geometry persistence
- Configure + build + DMG + install
- Plugin load + activation + InitModule dispatch (post-PR #27)
- Save/Load scenarios (post-PR #28)
- 3D scene rendering base (post-PR #24, colori degradati)

### ⚠️ Degradato / broken (issue aperte)
- **Rendering**: red channel missing (#25), atmospheric scattering off (#26), cockpit empty (#37), DlgMap empty (#39), planet texture chunky (#36)
- **Audio**: init ok ma no event triggering (#45)
- **Input**: 3 keyboard shortcut non rispondono (#41/#43), 1 modal stuck (#42), Shift+F1 (#40)
- **Polish UI**: glyph / button states / icons (#29/#30/#31/#32/#44)
- **Config**: Video tab width/height (#34), MSAA combo (#35)

### ⏭️ Skipped
- MFD runtime testing (plugins non attivi + cockpit broken)
- Joystick input (no controller)

## Raccomandazioni per la prossima iterazione

Priority order per risolvere gli issue:

1. **#37** F1 cockpit + **#39** DlgMap — likely stesso root cause family (rendering pipeline M15/M16/M17). Risolvere questi sblocca MFD testing, VC, 2D panel, map — 4 STEP Phase 2 in un colpo.
2. **#25** red channel + **#36** planet texture — fixare il color pipeline permette acceptance visuale completa del render e sblocca eventuali baseline per M30 rendering_parity.
3. **#45** XRSound event routing — l'audio è il deliverable più visibile dopo rendering.
4. **#41/#42/#43** keyboard dispatch — sistemare Keymap.cpp macOS entries.
5. **#34** Video tab Width/Height — quick fix, UX critico.
6. **#30/#31/#32/#29** polish UI — low priority, do in batch.
7. **#26** atmospheric scattering — potrebbe risolversi automaticamente post-#25.

## Artefatti di Phase 2

- [`QA_PHASE2_LOG.md`](QA_PHASE2_LOG.md) — log step-by-step con screenshot reference e verdetti
- [`QA_PHASE2_ISSUES.md`](QA_PHASE2_ISSUES.md) — body di ognuna delle 17 issue filed
- [`QA_PHASE2_SUMMARY.md`](QA_PHASE2_SUMMARY.md) — questo documento
- 3 PR merged: #24, #27, #28
- 17 GitHub issues: #29-#45
- Git branch: `qa/phase2-manual` con 16 commit di log + 3 commit di fix (ora su main)
