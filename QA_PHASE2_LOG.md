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

## Findings Fase 2 (bug tracker live)

Ogni bug scoperto durante Fase 2 viene tracciato qui con ID `P2-B<n>`. A fine sessione apriamo tutti come GitHub issue in batch (stesso flow di Fase 1). Include anche minor/cosmetic.

| ID | severity | step | descrizione breve | file/area | status |
|---|---|---|---|---|---|
| P2-B1 | cosmetic | 1.1 | Em-dash (`—`) renderizza come `?` nel placeholder del pane scenari | font/glyph Lekton/architext | open |
| P2-B2 | low | 1.2 | Pulsante "Launch Orbiter" non differenzia visualmente enabled/disabled (sempre blu) | `OGLLaunchpad::RenderTabScenario` footer | confirmed (cosmetic, button gated funzionalmente) |
| P2-B3 | **RELEASE-BLOCKER** | 1.2 | **M11 post-process pipeline rompeva il rendering**: con flag di default (`OGL_NO_POSTFX` unset) tutti gli scenari renderizzavano TUTTO NERO (solo HUD visibile). Tre bug stacked: OGLEnvMap FBO clobber, GL_RGBA16F silently broken su Apple Silicon Metal, ACES tonemap clamp-to-zero su LDR input. | `OGLEnvMap.cpp` + `OGLPostProcess.cpp` + `tonemap.frag` | ✅ **RESOLVED** via PR #24 (commit `c0ac8442`) |
| P2-B4 | high | 1.2 | Colori planet sbagliati post-fix: Terra nera, continenti verde fosforescente, "spazio" blu. Red channel assente dal scene output. | `shaders/texplanet.frag` o `OGLvPlanet::Render` color sampling | ➡️ **tracked as [#25](https://github.com/kanik0/orbiter/issues/25)** |
| P2-B5 | ~~high~~ | 1.2 | Presunta assenza stelle in Today scenario. | `OGLCelSphere` | ✅ **FALSE POSITIVE — CLOSED**: `Demo/Atlantis Ascent AP` cockpit view post-fix mostra starfield chiaramente. In Today le stelle sono oscurate dal clear-color × exposure = 0.16 blue che satura background; non è un bug di rendering stelle. |
| P2-B6 | medium | 1.2 | No Rayleigh/Mie atmosphere halo visible around Earth. M4 marked ✅ in roadmap ma scatter pass inerte. | `shaders/scatter.frag` | ➡️ **tracked as [#26](https://github.com/kanik0/orbiter/issues/26)** |

---

## Log step-by-step

### STEP 1.1: Launchpad boot + Tab Scenarios  [**PASS** ✅]

**Action:** `cd out/build/macos-arm64-debug && ./Orbiter` senza arg.

**Expected (Win32 reference):** finestra 1280×800, 6 tab (Scenarios/Options/Modules/Video/Extra/About), tab Scenarios attivo, tree categorie, pane descrizione vuoto, splitter, footer con "Start paused" + Launch + Exit.

**My report:**
- Finestra SDL 1280×800 OK
- Title bar "Orbiter Space Flight Simulator" con close button
- 6 tab presenti e corretti
- Tab Scenarios selezionato (highlight blu)
- Tree sinistra: 20 categorie top-level + 3 scenari root (test_far, test_moon, test_simple)
- Pane destra con placeholder
- Splitter presente
- Footer: Start paused + Launch Orbiter + Exit

**Verdict:** PASS ✅ struttura matcha Win32 reference.

**Finding:** P2-B1 (em-dash glyph mancante).

### STEP 1.2: Tree expand + description + thumbnail  [**PASS** ✅ con 1 verify-needed]

**Action:** expand Demo → click Today.

**Expected (Win32):** Demo espande 14 scenari, Today selezionato mostra thumbnail+DESC, Launch button abilitato.

**My report:**
- Demo espanso: 14 scenari (Atlantis Ascent AP, DG ISS Approach, Dione, Docked at ISS, Earth, Galilean system view, ISS Approach, Level 9 textures, Mir, Project Alpha, Saturn, The 1999 solar eclipse, Today, Virtual cockpit) ✅ match
- Today highlight blu ✅
- Pane destra: "Scenario: Demo/Today" + "The solar system at present." ✅ corrisponde a `BEGIN_DESC...END_DESC` di `Today.scn`
- Thumbnail: non mostrato (nessun .jpg/.png in `Scenarios/` repo — by design, Demo scn non hanno thumbnail; Win32 si comporterebbe uguale)
- Launch Orbiter button: blu ma visualmente identico a stato "nessuno scenario selezionato" → finding P2-B2

**Verdict:** PASS ✅ core functionality OK. Finding P2-B2 da verificare live (click comportamento).


