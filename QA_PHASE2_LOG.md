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
| P2-B12 | **critical** | 1.6a | **Extra tab AtlantisConfig absent — two stacked bugs, both systemic**: <br>**(a)** `Orbiter.cpp:675` chiamava `LoadStartupModules()` PRIMA di `RegisterBuiltinLaunchpadItems(pConfig)` → registry vuoto quando startup plugins registrano. <br>**(b) Ben peggio**: `Src/Orbitersdk/Orbitersdk.cpp` POSIX constructor chiamava `InitLib(nullptr)` → `dlsym(RTLD_DEFAULT, "InitModule")` ma plugin caricati con `RTLD_LOCAL` → **InitModule di TUTTI i plugin non veniva mai chiamato** su macOS. Impatto reale: tutti i `oapiRegisterMFDMode`/`oapiRegisterLaunchpadItem`/hooks da InitModule rotti silenziosamente nell'intero porting macOS. | `Src/Orbiter/Orbiter.cpp:675` + `Src/Orbitersdk/Orbitersdk.cpp:posix_module_init` | ✅ **RESOLVED** via PR #27 (commits `34d70589` + `7e7b99e9`). |
| P2-B13 | medium | 2.1 | Planet texture chunky/blocky continent edges — low LOD texture without M27.b Blue Marble LOD8 + red channel missing makes stepping very visible. | `OGLTile` / M27.b opt-in | open, depends on #25 |
| P2-B14 | **high** | 2.1 | F1 cockpit view shows only stars — no 2D panel, no VC mesh. M15/M16 rendering regression. | `OGLvVessel::Render` cockpit pass / M15/M16 | open |
| P2-B15 | low | 2.1 | DlgInfo default vessel selection is Sol, not the focus vessel (GL-01 from Today scn `BEGIN_FOCUS`). Selector switch works correctly. | `DlgInfo::OnDraw` initial object | open |
| P2-B16 | medium | 2.1 | DlgMap opens but map content is empty — Sketchpad (M17) rendering regression. | `OGLSketchpad` / DlgMap mapping pass | open |
| P2-B17 | low | 2.1 | Shift+F1 MFD mode selector non responsive in cockpit. Root cause ambigua: può essere keybinding, oppure 0 MFD plugin attivi (Modules tab), oppure legato a M15/M16 P2-B14 cockpit rendering. | Keymap/MFD panel integration | open, verify after P2-B14 fix |
| P2-B18 | medium | 3.1 | Ctrl+F1 DlgCamera: shortcut non fires, nessun dialog appare. | keymap binding / `Keymap.cpp` Ctrl+F1 → DlgCamera dispatch | open |
| P2-B19 | medium | 3.2 | Ctrl+F2 DlgTacc: dialog appare ma pulsanti unresponsive AND il dialog non si chiude (ESC o X non chiudono). Possibile modal-block sticky. | `DlgTacc::OnDraw` close/button handlers | open |
| P2-B20 | medium | 3.7 | Alt+F1 DlgHelp: shortcut non fires, browser non aperto. Roadmap M23.a dichiarava URL fallback cross-platform completo. Regression sospetta. | `Keymap.cpp` Alt+F1 → DlgHelp dispatch / `DlgHelp` URL-open path | open |
| P2-B21 | cosmetic | 5 | Missing icon textures per menu bar: `MenuInfoBar/Notes.png`, `MenuInfoBar/LuaConsole.png` (e probabilmente altri plugin). Menu items comunque visibili. | install pipeline plugin icon assets | open |
| P2-B22 | high | 6 | XRSound: init succeeds (no crash post-PR #27 InitModule fix), ma **no audio events triggering** in Demo/Atlantis Ascent scenario. Root cause candidates: XRSound-Atlantis.cfg parsing issue; OpenAL device buffer non-routed; vessel events non triggering. | `XRSound` runtime event path / OpenAL backend | open, verify post-P2-B14 cockpit fix |

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

### STEP 2.1: Demo/Today scenario launch — render 3D acceptance  [**PARTIAL** ⚠️]

**Action:** launch Demo/Today from Launchpad, observe 3D scene + try F1 cockpit + Shift+F1 MFD selector + Ctrl+I DlgInfo + Ctrl+M DlgMap.

**My report:**
- Earth sferica visibile, continenti verde fosforescente, spazio blu scuro, stelle visibili ✅ (rendering unblocked)
- Colori assolutamente innaturali (Terra nera, nessun blu oceano, nessun altro colore) — tracked as [#25](https://github.com/kanik0/orbiter/issues/25)
- Continenti chunky/sgranati — P2-B13 nuovo finding
- F1 cockpit view → solo stelle, nessuna mesh cockpit o panel → **P2-B14**
- Shift+F1 MFD selector non responsive (in cockpit view con 0 MFD plugin enabled) → **P2-B17**
- Ctrl+I DlgInfo apre dialog, ma default mostra Sol invece di GL-01 focus → **P2-B15**
- Ctrl+M DlgMap apre dialog ma mappa vuota → **P2-B16**

**Verdict:** PARTIAL ⚠️ rendering 3D esterno visibile (degraded), ma cockpit + MFD + panel + map tutti bloccati da regressioni Sketchpad/VC/Panel. Ulteriori fix rendering pipeline richiesti prima di acceptance visual completa.

### STEP 3: In-sim dialogs  [**PARTIAL** ⚠️ 4/7 PASS]

**Action:** test 7 shortcut dialog (Ctrl+F1/F2/F4/F5, F3, F6, Alt+F1).

**Report:**
| shortcut | dialog | verdict |
|---|---|---|
| Ctrl+F1 | DlgCamera | ❌ FAIL (no response) — P2-B18 |
| Ctrl+F2 | DlgTacc | ⚠️ PARTIAL (opens, buttons dead, won't close) — P2-B19 |
| Ctrl+F4 | DlgFunction | ✅ PASS |
| Ctrl+F5 | DlgRecorder | ✅ PASS |
| F3 | DlgFocus | ✅ PASS |
| F6 | DlgOptions | ✅ PASS |
| Alt+F1 | DlgHelp | ❌ FAIL (no response) — P2-B20 |

Skipped Ctrl+I (DlgInfo — tested STEP 2.1 with P2-B15) + Ctrl+M (DlgMap — empty STEP 2.1 P2-B16).

**Verdict:** PARTIAL ⚠️ 4/7 dialog-based features funzionano. Ctrl+F1 + Alt+F1 non rispondono (keymap); Ctrl+F2 ha dialog modal-stuck.

### STEP 4: Custom Functions (via Ctrl+F4)  [**PASS** ✅ for available items]

**Action:** open DlgFunction, inspect registered custom funcs.

**Expected:** builtin (Joystick calibration, Keymap editor) + plugin-registered (Rcontrol, Meshdebug, Scenario editor if Modules tab enabled).

**Report:**
- 2 entries visible: `Joystick calibration`, `Keymap editor` (M26.c/M26.d). ✅
- Entrambi aprono modal ImGui.
- Plugin-dependent custom funcs (Rcontrol/Meshdebug/ScnEditor) non presenti: plugin non attivati in Modules tab — comportamento atteso, non bug.

**SKIPPED sub-steps:**
- 4.1 Rcontrol — plugin not enabled (would need Modules tab activation)
- 4.2 Mesh debugger — plugin not enabled
- 4.3 Scenario editor — plugin not enabled
- 4.5 Joystick calibration deep-test (axis widgets, deadzone slider) — no controller attached

**Verdict:** PASS ✅ builtin custom funcs available and openable. Plugin-based sub-steps legitimately skipped.

### STEP 5: Plugin runtime (Framerate/Notes/LuaConsole)  [**PARTIAL** ⚠️]

**Action:** enable Framerate + Notes + LuaConsole in Modules tab → launch scenario → verify menu items.

**Report:**
- No crash su activation + scenario launch ✅
- Menu items Notes + LuaConsole visibili ✅
- ⚠️ Missing icon textures "MenuInfoBar/Notes.png" e "MenuInfoBar/LuaConsole.png" (log warning) — P2-B21
- Framerate overlay non trovato (non chiaro come attivarlo — probabile require specific shortcut o auto-appears in specific camera mode)

**SKIPPED sub-steps MFD-based:**
- 5.1 TransX MFD — blocked by MFD selector issue P2-B17
- 5.2 ExtMFD — same
- 5.4 FlightData overlay — blocked by Shift+F1 issue
- 5.5 LuaConsole REPL — openable but LuaInline scripting rotto (known since Phase 1 — I15)

**Verdict:** PARTIAL ⚠️ plugin activation + menu registration funziona; asset/icon + MFD-based testing bloccati.

### STEP 6: Audio XRSound  [**PARTIAL** ⚠️]

**Action:** enable XRSound in Modules → scenario Today e Atlantis Ascent → verifica audio events.

**Report:**
- No crash in entrambi gli scenari ✅ (prima fix P2-B12 InitModule XRSound era già silenzato)
- Today: no audio atteso (nessun evento acustico in scenario static)
- Atlantis Ascent: **nessun suono** durante ascent burn → P2-B22
- XRSound init phase completa: OpenAL device probabilmente creato, pack Default/*.wav loaded

**Verdict:** PARTIAL ⚠️ audio pipeline si inizializza ma eventi vessel non triggeranno audio. Serve debug runtime events path (possibilmente legato a P2-B14 cockpit/vessel state non aggiornato).




### STEP 2.2-2.3: Atlantis Ascent AP + Demo/Earth  [**SKIPPED** ⏭️]

Skip: stessa degradazione visiva di Today attesa (same rendering pipeline). Test ridondante finché #25/#26/P2-B14/B16 non sono risolti.

---

## Path decisione (post-STEP 2.1)

Phase 2 STEP 2+ bloccata da rendering pipeline incompleto (cockpit/MFD/panel/map/sketchpad). Decisione condivisa con utente: **percorso A + C**:

- **Continue** Phase 2 su step che non dipendono da rendering 3D runtime (dialog UI, save/load, window geometry, DMG install)
- **SKIP / PARTIAL** step rendering-dependent (cockpit, MFD, VC, 2D panel, planetary map)
- **File** tutti i finding residui come GitHub issue a chiusura Phase 2

### STEP 1.6a: Tab Extra  [**PASS** ✅ after P2-B12 fix]

**Action:** click Extra tab, inspect tree, verify AtlantisConfig appears under Vessel configuration after P2-B12 double-fix (ordering + POSIX bootstrap).

**Expected:** 5 container + 10 built-in items, plus AtlantisConfig under "Vessel configuration" per M22.f/M25.b + AtmConfig somewhere if its InitModule runs.

**My report (post-fix):**
- Tree (screenshot): ▶ Physics engine, ▶ Instruments and panels, ▼ Vessel configuration → **Atlantis Configuration** ✅, Planet configuration, ▶ Debug options, **Atmosphere Configuration** (top-level, new)
- Atlantis Configuration selected → right pane: "Global configuration for the default Space Shuttle Atlantis." + Edit... button ✅
- Atmosphere Configuration: un-parented bonus (AtmConfig plugin non specifica parent → top-level). Conferma che P2-B12 era sistemico — non solo AtlantisConfig, TUTTI gli startup plugins erano silenzati.

**Verdict:** PASS ✅ tree correttamente popolato. P2-B12 chiuso come resolved.


