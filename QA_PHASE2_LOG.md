# QA Phase 2 — Manual interactive verification

**Data inizio:** 2026-04-18
**Branch:** `qa/phase2-manual` (da `main` post-merge PR #23)
**Build:** `out/build/macos-arm64-debug/Orbiter` (con 4 fix Phase 1 applicati)
**Host:** Darwin 25.4.0 / Apple M2

---

## Sequenza verifica (sezione_step : feature)

1. **Launchpad** — 6 tab
   - 1.1 Scenarios (tree, descrizione, thumbnail, splitter, Start paused)
   - 1.2 Options (12 sub-page, persistenza)
   - 1.3 Modules (12 plugin, toggle, persistenza)
   - 1.4 Video (combo SDL risoluzioni, fullscreen, vsync, stencil)
   - 1.5 Extra (15 builtin + AtlantisConfig)
   - 1.6 About (version, URL button)
2. **Avvio scenario** — render 3D
   - 2.1 Demo/Today
   - 2.2 Atlantis Ascent AP
   - 2.3 Demo/Earth
3. **Dialogs in-sim** — Ctrl+ shortcuts
   - 3.1 Ctrl+I DlgInfo
   - 3.2 Ctrl+M DlgMap
   - 3.3 Ctrl+F1 DlgCamera
   - 3.4 Ctrl+F2 DlgTacc
   - 3.5 Ctrl+F4 DlgFunction
   - 3.6 Ctrl+F5 DlgRecorder
   - 3.7 F3 DlgFocus
   - 3.8 Alt+F1 DlgHelp
   - 3.9 F6 DlgOptions
4. **Custom Functions** (Ctrl+F4)
   - 4.1 Rcontrol (slider thrust, RCS)
   - 4.2 Mesh debugger
   - 4.3 Scenario editor (tab State live)
   - 4.4 Keymap editor
   - 4.5 Joystick calibration (se controller)
5. **Plugin runtime**
   - 5.1 TransX MFD
   - 5.2 ExtMFD float window
   - 5.3 Notes
   - 5.4 FlightData / Framerate overlay
   - 5.5 LuaConsole REPL
6. **Audio XRSound**
   - 6.1 DeltaGlider main engine throttle → pitch
   - 6.2 Docking clanger
   - 6.3 Altair music loop
7. **Joystick** (se controller)
   - 7.1 Axis → vessel attitude
   - 7.2 Touchdown rumble
   - 7.3 Engine ignite rumble
   - 7.4 Atm buffet rumble
8. **Save / Load**
   - 8.1 Save Scenario
   - 8.2 Quicksave Ctrl+S
   - 8.3 Reload
9. **Window geometry persistence**
   - 9.1 Launchpad resize → restart → same
   - 9.2 Splitter → restart → same
10. **.app bundle + DMG**
    - 10.1 `cmake --build --target macos-dmg`
    - 10.2 Drag in /Applications → launch

---

## Verdict legend

- **PASS** ✅ — feature funziona come su Windows
- **PARTIAL** ⚠️ — feature parzialmente funzionante (con fallback documentato)
- **FAIL** ❌ — feature non funziona / crash
- **SKIP** ⏭️ — saltato (es: no controller, no test hardware)

---

## Log step-by-step

(Verdetti compilati live mentre progrediamo.)
