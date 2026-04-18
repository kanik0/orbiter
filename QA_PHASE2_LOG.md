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
| P2-B7 | cosmetic | 1.3 | Tree-expand icons renderizzate come `?` nella sidebar "Visual helpers" (4 sub-item Planetarium/Labels/Forces/Frame axes). Simile a P2-B1 ma glyph differente (tree marker ▶/▼ vs em-dash). | font/glyph set ImGui | open |
| P2-B8 | low | 1.4 | AscentMFD (SDK sample) esposto in Modules tab sotto "Miscellaneous" (post-fix B1 ora builda). Dovrebbe essere hidden dall'UI utente OPPURE avere `.info` "Samples"/dev category. UX cosmetic. | `Orbitersdk/samples/AscentMFD/CMakeLists.txt` (add orbiter_module_info) | open |
| P2-B9 | medium | 1.4 | XRSound manca di `.info` sidecar → appare in Modules tab come "Miscellaneous" generico senza description. Utenti audio non sanno cosa attivare. Dovrebbe avere category "Audio" + description "OpenAL-backed audio engine for vessel sounds". | `Sound/XRSound/src/CMakeLists.txt` (add orbiter_module_info) | open |
| P2-B10 | medium | 1.5 | Video tab: Width field mostra `20310460` e Height field `0` (valori garbage / uninitialized). Resolution dropdown funziona correttamente ("2560 x 1664"), ma i due campi numerici sotto hanno binding sbagliato a variabili non inizializzate. UX confondente. | `OGLLaunchpad::RenderTabVideo` Width/Height `ImGui::InputInt` binding | open |
| P2-B11 | low | 1.5 | Video tab: mancante MSAA/multisample antialias combo (0x/2x/4x/8x) che Win32 Video tab espone. Possibile by-design macOS (Metal MSAA autonomo) o regression M22.c. | `OGLLaunchpad::RenderTabVideo` | open, verify intent |
| P2-B12 | **critical** | 1.6a | **Extra tab AtlantisConfig absent — two stacked bugs, both systemic**: <br>**(a)** `Orbiter.cpp:675` chiamava `LoadStartupModules()` PRIMA di `RegisterBuiltinLaunchpadItems(pConfig)` → registry vuoto quando startup plugins registrano. <br>**(b) Ben peggio**: `Src/Orbitersdk/Orbitersdk.cpp` POSIX constructor chiamava `InitLib(nullptr)` → `dlsym(RTLD_DEFAULT, "InitModule")` ma plugin caricati con `RTLD_LOCAL` → **InitModule di TUTTI i plugin non veniva mai chiamato** su macOS. Impatto reale: tutti i `oapiRegisterMFDMode`/`oapiRegisterLaunchpadItem`/hooks da InitModule rotti silenziosamente nell'intero porting macOS. | `Src/Orbiter/Orbiter.cpp:675` + `Src/Orbitersdk/Orbitersdk.cpp:posix_module_init` | ✅ **RESOLVED** via commits `db9f94c8` (ordering) + `017f3827` (dladdr + RTLD_NOLOAD self-handle). Diagnostic confirms: `root handle = 0x3 for 'Vessel configuration'` + `RegisterLaunchpadItem → 0x10`. |

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

### STEP 1.3: Tab Options + 12 sub-pagine  [**PASS** ✅ con P2-B7 nuovo]

**Action:** click Options tab, inspect sidebar + 3 rappresentative pages (Visual, Physics, Instruments).

**Expected:** 12 OptionsPages 1:1 match Win32 ref (Visual/Physics/Instrument/Vessel/UI/Joystick/CelSphere/VisHelper/Planetarium/Labels/Forces/Axes).

**My report:**
- Sidebar: 8 top-level + 4 children sotto "Visual helpers" = 12 totali. Naming rebrandizzato (Instruments, User interface, Celestial sphere, Visual helpers, Frame axes) ma 1:1 concettualmente. ✅
- Visual page: 13 toggle + 5 slider + dropdown elevation interp. Copre M4/M5/M6/M7/M8/M10/M11/M12 milestones. ✅
- Physics page: 5 toggle + 4 RK propagation levels + thresholds. ✅
- Instruments page: MFD config + pow2 + 2D/VC size + compact glass cockpit. ✅

**Finding:** P2-B7 (tree-expand icon `?` glyph).

**Verdict:** PASS ✅ struttura completa, naming coerente, contenuti popolati per 3 pagine ispezionate. 9 pagine restanti non ispezionate singolarmente ma visibili in sidebar.

### STEP 1.4: Tab Modules  [**PASS** ✅ con P2-B8/B9 polish]

**Action:** click Modules tab, inspect plugin list + categories + footer.

**Expected:** 11 plugin con .info match (ExtMFD, FlightData, Framerate, LuaConsole, LuaMFD, Meshdebug, Notes, Rcontrol, ScnEditor, ScriptMFD, TransX). TrackIR NON presente.

**My report:**
- 13 plugin visibili (2 extra rispetto Phase 1 F baseline)
- Tutti gli 11 attesi presenti con categoria corretta
- TrackIR correctly omitted (Win32-only gating ✅)
- Extras: AscentMFD (sample SDK post-B1 fix) + XRSound (post-B1/B2 fix) in "Miscellaneous" senza .info
- Footer: Deactivate all + Rescan buttons presenti
- Tutti checkbox unchecked di default
- Right pane con prompt "Select a module to see its description"

**Findings:** P2-B8 (AscentMFD expose), P2-B9 (XRSound no .info).

**Verdict:** PASS ✅. Tutti i plugin previsti + category. Side-effect dei fix Phase 1 (AscentMFD/XRSound in misc) da polishare ma non-bloccanti.

### STEP 1.5: Tab Video  [**PARTIAL** ⚠️]

**Action:** click Video tab, inspect controls.

**Expected:** Graphics engine header + Resolution dropdown + fullscreen/vsync/stencil toggles + eventualmente MSAA.

**My report:**
- Graphics engine: "OGLClient (OpenGL 4.1 Core Profile, SDL2)" + "built-in / single client on macOS" ✅
- Resolution dropdown: "2560 x 1664" (native retina) ✅
- Fullscreen, VSync (checked), Try stencil buffer, Stereo (anaglyph) — tutti presenti ✅
- ❌ Width input field: `20310460` (garbage), Height input field: `0`
- MSAA toggle non presente
- Testo hint con `?` artifact (P2-B1)

**Findings:** P2-B10 (width/height garbage), P2-B11 (no MSAA).

**Verdict:** PARTIAL ⚠️ layout e controlli principali presenti e funzionanti; campi Width/Height corrotti creano confusione.

### STEP 1.6a: Tab Extra  [**PASS** ✅ after P2-B12 fix]

**Action:** click Extra tab, inspect tree, verify AtlantisConfig appears under Vessel configuration after P2-B12 double-fix (ordering + POSIX bootstrap).

**Expected:** 5 container + 10 built-in items, plus AtlantisConfig under "Vessel configuration" per M22.f/M25.b + AtmConfig somewhere if its InitModule runs.

**My report (post-fix):**
- Tree (screenshot): ▶ Physics engine, ▶ Instruments and panels, ▼ Vessel configuration → **Atlantis Configuration** ✅, Planet configuration, ▶ Debug options, **Atmosphere Configuration** (top-level, new)
- Atlantis Configuration selected → right pane: "Global configuration for the default Space Shuttle Atlantis." + Edit... button ✅
- Atmosphere Configuration: un-parented bonus (AtmConfig plugin non specifica parent → top-level). Conferma che P2-B12 era sistemico — non solo AtlantisConfig, TUTTI gli startup plugins erano silenzati.

**Verdict:** PASS ✅ tree correttamente popolato. P2-B12 chiuso come resolved.


