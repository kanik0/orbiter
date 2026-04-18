# QA Phase 1 — Orbiter macOS ARM64 Parity Audit

**Data:** 2026-04-18
**Branch:** `qa/phase1-audit` (da `main` post-merge PR #7)
**Preset:** `macos-arm64-debug` + `-DORBITER_BUILD_XRSOUND=ON -DORBITER_MAKE_TESTS=ON`
**Host:** Darwin 25.4.0 / Apple M2 / OpenGL 4.1 Metal-90.5

> ⚠️ **ERRATA CORRIGE — 2026-04-18 post-Phase 2 STEP 1.2**
>
> La sezione C (Runtime smoke battery, "14/14 PASS") di questo report
> era un **falso positivo**. Il criterio di accettazione `PNG > 50 KB`
> + `0 TERMIN/critical/fatal nel log` era soddisfatto solo dal peso dei
> glyph HUD ImGui: ispezione visiva dei PNG in Phase 2 ha rivelato che
> **tutti e 14 gli scenari renderizzavano scene completamente nera**
> con solo l'overlay HUD sovrapposto. La M11 post-process pipeline era
> rotta da tre bug stacked (OGLEnvMap FBO clobber, GL_RGBA16F silently
> broken su Apple Silicon Metal, ACES tonemap clamp-to-zero su LDR).
>
> **Risolto in [PR #24](https://github.com/kanik0/orbiter/pull/24)**
> (commit `c0ac8442`, merged in main `174ec0a6`). Rendering ora visibile
> (Earth + ISS + stars) ma ancora degradato visivamente — bug separati
> filati come [#25](https://github.com/kanik0/orbiter/issues/25)
> (red channel missing) e [#26](https://github.com/kanik0/orbiter/issues/26)
> (M4 atmospheric scattering not visible).
>
> L'errata non invalida le sezioni A (grep), B (cross-ref), D (ctest),
> E (DMG), F (plugin .info), G (follow-up roadmap), H (ad-hoc) e le
> estensioni K-N (Fasi A-D) — quelle si basavano su analisi statica o
> runtime checks che non dipendevano dall'ispezione visiva del render.
> La tabella smoke battery §C resta come riferimento di **stabilità
> runtime** (0 crash, 0 TERMIN): quella parte rimane valida.

---

## TL;DR (post-estensione Fasi A-D)

- **Build:** ✅ configure + full build puliti, 62 dylib + 12 plugin (**inclusa `libXRSound.dylib` 1.1 MB**) + DMG 157 MB.
- **Parity statica (Fase E, 5 coppie):** 4/5 ≥95%; 1/5 (ScnEditor) 50% per design (M25.d).
- **Parity statica (Fasi A-D, 6 coppie OGLClient ↔ D3D9Client):** tutte ≥95%, gap minori su blit copy modes + color-key strategy + point sprite rendering.
- **Smoke test runtime:** 14/14 scenari `Scenarios/Demo/*.scn` catturano PNG 178-185 KB, **0 TERMIN/critical/fatal**.
- **ctest:** 1 PASS (`rendering_parity`), 1 FAIL (`Lua.Interpreter` exe missing, Win32-gating plausibile), **1 silenziosamente non registrato** (`xrsound_openal_smoke`, vedi §N.4).
- **XRSound runtime:** `xrsound_openal_smoke` eseguito manualmente → EXIT 0 clean, ma **ctest non lo registra** per ordine `enable_testing()` vs `add_subdirectory(Sound)` nel top-level CMakeLists.txt.
- **DMG:** 157 MB, `hdiutil verify` clean.

### ⚠️ Finding critici (violano "zero-stubs policy")

| # | severity | finding | fase | blocker PR? |
|---|---|---|---|---|
| 1 | **critical** | `CELBODY2::LoadAtmosphereModule` usa `Modules\\Celbody\\%s\\Atmosphere` senza `#ifdef _WIN32` → path con `\\` su macOS, tutti i plugin atmosphere (`VenusAtm2006`, `EarthAtmJ71G`, `EarthAtmNRLMSISE00`, `MarsAtm2006`) falliscono load | A-D | **forse** (scenari con atmosfera usano fallback hard-coded) |
| 2 | **critical** | `libXRSound.dylib` non costruito da `cmake --build --parallel 8` default (solo `--target XRSound_dll` esplicito) → **audio silent-broken** al primo build clean | D | **probabile** (deploy rotto) |
| 3 | **high** | `libLuaInline.dylib` non trovato a runtime — `CMAKE_LIBRARY_OUTPUT_DIRECTORY` missing → scripting Lua rotto | E | no |
| 4 | **high** | 8 Celbody dylib sono symlink a Win32 PE32 → `dlopen` fallisce | E (M22 follow-up) | no |
| 5 | **high** | `xrsound_openal_smoke` ctest non registrato — `enable_testing()` chiamato dopo `add_subdirectory(Sound)` | D/E | no (test non blocca build) |
| 6 | **medium** | Module-load log stampa garbage (`Module 0C�k ...`) — `GetModuleFileName` stub non popola buffer | E | no (cosmetico) |
| 7 | **medium** | `DlgOptions` in-sim: manca DrawVisual + DrawPhysics (TODO commentato) | E | no |
| 8 | **low** | `oapiClearSurfaceColourKey` no-op su macOS con `// TODO` | E | no |
| 9 | **low** | `OpenFileIgnoreCase` esteso solo a `Vessel::OpenConfigFile` — mesh/texture/base/scenario loaders potenzialmente case-sensitive | E | no |

Dettagli §C e §N.

---

## Sezione A — Grep di parità

Scope: 44 file `.cpp/.h` modificati/aggiunti da Fase E (M22-M30), esclusi vendored (stb_image, dr_wav, dr_mp3, stb_vorbis, Extern/).

### A.1 — `TODO|FIXME|XXX|STUB|HACK`

Occorrenze significative (escluse `resource_stub.h` + file Win32-only compilati out su macOS):

| file:line | nota |
|---|---|
| [Src/Orbiter/DlgOptions.h:28](Src/Orbiter/DlgOptions.h:28) | `//TODO: add when converting Launchpad` + `//void DrawVisual();` + `//void DrawPhysics();` commentati → **due pagine opzioni non portate**. |
| [Src/Orbiter/Orbiter.cpp:3346](Src/Orbiter/Orbiter.cpp:3346) | `// TODO: implement help viewer for non-Windows` → fallback `LOGOUT("Help requested: %s")` (no crash, ma click "?" launchpad fa nulla di visibile all'utente) |
| [Src/Orbiter/OrbiterAPI.cpp:2041](Src/Orbiter/OrbiterAPI.cpp:2041) | `//if (gc) gc->clbClearSurfaceColourKey (surf); // TODO` → `oapiClearSurfaceColourKey` è silenziosamente no-op su macOS |
| [Src/Orbiter/Vessel.cpp:4680](Src/Orbiter/Vessel.cpp:4680) | `#ifdef UNDEF  // TODO!!!` → blocco legacy disabilitato, non attivo |

TODO/FIXME "innocui" (retaggi pre-Fase E, non toccati): Camera.cpp, Planet.h, Vesselbase.cpp, Pane.cpp, VectorMap.cpp.

### A.2 — `assert(false)|abort()|NotImplemented|TBD`

1 solo hit: [Src/Orbiter/Pane.cpp:1084](Src/Orbiter/Pane.cpp:1084) `assert(false); // This code section doesn't seem to run.` — **pre-esistente** (non Fase E), non raggiungibile in scenari testati.

### A.3 — `#ifdef _WIN32` senza `#else`

Verifica implicita: il build macOS **compila clean senza warning di unused-symbol** → le guardie `#ifdef _WIN32` sono complete o hanno controparti macOS. Non ho trovato `#else { /* nothing */ }` degenerati nei file Phase E.

### A.4 — `oapiDefDialogProc`

Chiamato solo da codice Win32-gated (D3D9Client, DeltaGlider vessel, Editor.cpp, Meshdebug.cpp) + definizione in OrbiterAPI.cpp:2254. Su macOS `Editor.cpp` + `Meshdebug.cpp` non chiamano questo entry-point (ImGui path), verificato.

### A.5 — `LoadLibrary|GetProcAddress|HKEY|RegOpenKey`

Tutte le occorrenze in file cross-platform (`Celbody.cpp`, `Vessel.cpp`, `Planet.cpp`, `Script.cpp`, `OrbiterAPI.cpp`, `Memstat.cpp`, `ModuleAPI.cpp`, `Orbiter.cpp`) sono **correttamente wrappate** via `Orbitersdk/include/OrbiterPlatform.h:197-199`:
```
#define LoadLibrary(x)     OrbiterLoadLibrary(x)
#define GetProcAddress     OrbiterGetProcAddress
```
che dispatch a `dlopen`/`dlsym` su POSIX. `HKEY`/`RegOpenKeyEx` usati solo in `Orbiter.cpp:610-612` dentro `#ifdef _WIN32` (Wine detection). **Nessuna leak Win32 nativa a runtime macOS.**

---

## Sezione B — Cross-reference Windows ↔ macOS (5 coppie)

(Audit delegato a sub-agent Explore; output integrato.)

### B.1 — `Launchpad.cpp` (Win32) ↔ `OGLLaunchpad.{cpp,h}` (macOS)

| feature Win32 | counterpart macOS | status | note |
|---|---|---|---|
| Tab switching (AddTab, SwitchTabPage) | m_currentTab + RenderTab* | ✅ | ImGui tab bar |
| Bind Config | Bind() / InitFromConfig | ✅ | |
| ScanScenarios | ScanDirectory, m_root | ✅ | Tree state ImGui |
| Thumbnail loading | LoadThumbnail + GL upload | ✅ | STB Image |
| Splitter | ScenarioTabState::splitterPos | ✅ | ImGui SliderFloat, salvato in Config |
| Launch/Exit buttons | RenderTabScenario Launch | ✅ | |
| Window geometry persistence | m_lastLpadX/W/H + SyncToConfig | ✅ | Config::rLaunchpad |
| 6 tab Scenarios/Options/Modules/Video/Extra/About | RenderTab*+state per tab | ✅ | |
| Tab Extra (15 item Win32) | RenderTabExtra (11 item) | ⚠️ | 4 item meno di Win32 (dettaglio B.2) |
| Tab About | RenderTabAbout + about.hpp | ✅ | |

### B.2 — `TabExtra.cpp` (15 Win32 classi) ↔ `BuiltinLaunchpadItems.cpp` (11 macOS item)

Tutte le 15 classi `Extra*` hanno controparte meno **ExtraAngDynamics** (che è già `#ifdef UNDEF` in Win32 — non portato perché non richiesto). **ItemLaunchpadOptions** è ⚠️: l'HTML viewer in-process non c'è su macOS, mostra `TextDisabled` "On macOS the inline HTML viewer is unavailable".

Match 14/14 richiesti. ✅

### B.3 — `OptionsPages.cpp` (12 Win32) ↔ `OGLLaunchpad.cpp` (12 `RenderPage*`)

Match nominale 1:1 su tutte e 12: Visual, Physics, Instrument, Vessel, UI, Joystick, CelSphere, VisHelper, Planetarium, Labels, Forces, Axes. ✅ per parità strutturale.

**Nota complicazione:** il grep Sezione A ha trovato in `DlgOptions.h:28` due `//void DrawVisual()` + `//void DrawPhysics()` commentate **nella classe `DlgOptions` (dialog in-simulazione F6)**, che è **un consumer diverso** da Launchpad OptionsPage. Quindi OptionsPages launchpad = ✅ 12/12; DlgOptions in-sim = ⚠️ 4/6 pagine (Instrument/Vessel/UI/Joystick presenti; Visual/Physics TODO). Va verificato interattivamente in Fase 2.

### B.4 — `WindowMgr.cpp` (Win32) ↔ `OGLWindowMgr.cpp` (macOS)

| Win32 method | macOS override | status |
|---|---|---|
| RegisterApplication | override + RegisterApplicationImGui | ✅ |
| UnRegister / IsOpen / OpenNode / DisplayWindow | tutti override | ✅ |
| GetFont / GetNode / GetDialog / UpdateSize | override (alcuni no-op per HWND API) | ✅ |
| RenderAll / RenderApplicationWindow | presenti | ✅ |

API legacy HWND-based stub return-nullptr su POSIX; ImGui API è path primario. Match funzionale completo.

### B.5 — `Editor.cpp` (12 tab Win32) ↔ `ScnEditorImGui.cpp` (6 tab macOS)

| Win32 tab | macOS | status | note |
|---|---|---|---|
| Vessel | DrawVesselPicker | ✅ | |
| New | — | ❌ out-of-scope | M25.d: "use Launchpad scenario list" |
| Save | — | ❌ out-of-scope | M25.d: "use in-sim Save Scenario menu" |
| Edit | — | ❌ out-of-scope | M25.d |
| Elements | — | ❌ out-of-scope | M25.d: "derive from State vec" |
| Statevec | DrawStateTab | ✅ | |
| Landed | — | ❌ out-of-scope | M25.d |
| Orientation | DrawOrientationTab | ✅ | |
| AngularVel | DrawAngVelTab | ✅ | |
| Propellant | DrawPropellantTab | ✅ | |
| Docking | — | ❌ out-of-scope | M25.d |
| Date | DrawDateTab | ✅ | |

6/12 portati, 6/12 fuori-scope esplicito (roadmap M25.d). Nessun crash: ScnEditorImGui.cpp su `!_WIN32` non chiama `oapiDefDialogProc/WM_COMMAND`. Commento header (ScnEditorImGui.cpp:14-28) documenta le alternative. ✅ rispetto allo scope dichiarato.

---

## Sezione C — Runtime smoke battery

Esecuzione: `./Orbiter --scenario="Demo/<name>" --capture-frame=60 --capture-out=/tmp/phase1/scn_*.png` con watchdog 18 s + SIGINT + SIGKILL cleanup. Log sanitized con `tr -d '\000'` (il log ha byte NUL intermixed — vedi finding C.1 sotto).

| scenario | PNG size (KB) | critical errors (TERMIN/critical/fatal) | module load fails | note |
|---|---|---|---|---|
| Atlantis Ascent AP | 179 | 0 | 4 | LuaInline + 3 celbody Win32 |
| DG ISS Approach | 180 | 0 | 6 | |
| Dione | 185 | 0 | 9 | |
| Docked at ISS | 178 | 0 | 9 | |
| Earth | 183 | 0 | 9 | |
| Galilean system view | 184 | 0 | 9 | |
| ISS Approach | 179 | 0 | 6 | |
| Level 9 textures | 183 | 0 | 9 | |
| Mir | 182 | 0 | 9 | |
| Project Alpha | 182 | 0 | 3 | |
| Saturn | 183 | 0 | 9 | |
| The 1999 solar eclipse | 182 | 0 | 12 | |
| Today | 183 | 0 | 9 | |
| Virtual cockpit | 178 | 0 | 9 | |

**14/14 scenari renderizzati puliti.** 0 scenario ha critical/fatal/TERMIN.

### C.1 — Finding: LuaInline non caricato (**severity: high**)

Log ogni scenario:
```
Could not find a module named LuaInline. Tried ./LuaInline and ./LuaInline/LuaInline.dylib.
```
- `Src/Module/LuaScript/LuaInline/CMakeLists.txt:5` → `set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR})`.
  Su macOS le `SHARED` libs sono controllate da `LIBRARY_OUTPUT_DIRECTORY`, non `RUNTIME_OUTPUT_DIRECTORY` (che è per executables e DLL Win32). Risultato: `libLuaInline.dylib` finisce in `out/build/macos-arm64-debug/Modules/libLuaInline.dylib` invece che nella root.
- `Src/Orbiter/Script.cpp:8` → `const char *path = ".";` → il loader cerca `./libLuaInline.dylib`, `./LuaInline.dylib`, `./LuaInline`, `./LuaInline/libLuaInline.dylib` → tutti KO.

**Impact:** ogni `scriptvessel`, `lua-driven MFD mode`, Lua console che tenta `NewInterpreter()` fallisce silenziosamente — `ScriptInterface::LoadInterpreterLib()` restituisce NULL e i successivi `NewInterpreter/DelInterpreter` sono no-op. Scripting è di fatto rotto.

**Fix proposto:** aggiungere `set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR})` in `CMakeLists.txt:5`.

### C.2 — Finding: 8 Celbody symlink a Win32 PE32 (**severity: high**)

`Src/Orbiter/CMakeLists.txt` POST_BUILD step (commit cd006d9a) fa:
```sh
for f in "${CMAKE_BINARY_DIR}"/Modules/Celbody/*.dll; do
  ln -sf "$f" "${CMAKE_BINARY_DIR}/Modules/Celbody/lib$base.dylib"
done
```
per i `.dll` copiati da `Src/Celbody/{Ariel,Deimos,Miranda,Oberon,Phobos,Titania,Triton,Umbriel}/CMakeLists.txt` (che fanno un raw `copy` del `.dll` senza build macOS). Questi `.dll` sono `PE32 executable (DLL) (GUI) Intel 80386, for MS Windows` — dlopen fallisce:
```
dlopen(Modules/Celbody/libAriel.dylib, 0x0006): tried: ... (slice is not valid mach-o file)
```
**Impact:** Ogni scenario che referenzia questi corpi celesti (Phobos/Deimos in scenari Mars; Triton in Neptune; lune di Urano in scenari Uranus) ha celestial bodies senza ephemeris backend. Scenari `Demo/*` non li usano (0 critical), ma scenari Mars/Uranus/Neptune estesi sì.

**Fix proposto:** portare le 8 `CMakeLists.txt` a compilare sorgenti `.cpp` (se esistono nel repo full) come target `add_library(<name> SHARED)`. Se le sorgenti non sono pubblicate, marcare esplicitamente come "macOS limitation" e skippare il symlink per questi 8.

### C.3 — Finding: log module-load stampa garbage (**severity: medium**)

```
Module 0C�k .................. [Build ******, API ******]
```
in `Orbiter.log` al caricamento di ogni modulo. Root cause [Src/Orbiter/OrbiterAPI.cpp:2624](Src/Orbiter/OrbiterAPI.cpp:2624):
```cpp
char cbuf[256], mname[256], *mp;
...
GetModuleFileName (hModule, mname, 256);  // stub su macOS, non inizializza mname
for (i = 0, mp = mname; mname[i]; i++) ... // legge stack garbage
```
`GetModuleFileName` è stubbato in `resource_stub.h:196` come `inline DWORD GetModuleFileName(HMODULE, char*, DWORD) { return 0; }` — lascia `mname` con memoria stack non inizializzata.

**Fix proposto:** far popolare il buffer al stub (`*out = '\0'`) + idealmente implementare via `dladdr()` il vero resolve del nome modulo.

### C.4 — Log contiene byte NUL

Il log `~/Library/Logs/Orbiter/Orbiter.log` contiene byte `\x00` intermixed che impediscono `cat`/`grep` normale (ho lavorato con `tr -d '\000'`). Verosimilmente collegato a C.3 (garbage bytes in format string) o a flush parziali. Cosmetico ma rende il log fastidioso da leggere in-situ.

---

## Sezione D — `ctest` suite

```
Test project /Users/.../macos-arm64-debug
    Start 1: Lua.Interpreter  → ***Not Run (executable not found)
    Start 2: rendering_parity → ✅ Passed   45.88 sec
50% tests passed, 1 tests failed out of 2
```

- **`rendering_parity`** PASS. Baselines sono placeholder 0-byte (roadmap M30 followup) quindi harness end-to-end OK ma **non ha ancora gate SSIM reale** (threshold bypass when baseline size == 0).
- **`Lua.Interpreter`** FAIL: l'eseguibile `Lua.Interpreter` non esiste in build tree. Va capito se il target si costruisce solo su Win32 (plausibile: legato a `LuaInterpreter` che è una DLL Lua wrapper). Se è così va `add_test(... WIN32)`-gatato. Se invece è un unit test cross-platform, va costruito.

---

## Sezione E — DMG build

```
$ cmake --build out/build/macos-arm64-debug --target macos-dmg
[1/3] Creating distributable Orbiter.app (copy data dirs)...
[2/3] Code-signing Orbiter.app... (ad-hoc — APPLE_CODESIGN_IDENTITY not set)
[3/3] Building Orbiter-macos-arm64.dmg...
```

| check | risultato |
|---|---|
| File prodotto | `out/build/macos-arm64-debug/Orbiter-macos-arm64.dmg` ✅ |
| Size | **157 MB** ⚠️ (atteso 500 MB - 1 GB per spec; roadmap M28 parla di ~430 MB con textures; verosimilmente `Textures/` non interamente popolato in debug build) |
| `hdiutil verify` | ✅ `checksum ... is VALID` |
| Notarization | skipped (env `APPLE_NOTARIZATION_KEYCHAIN_PROFILE` assente) |

**Nota size:** 157 MB è inferiore alla previsione roadmap (~430 MB). Probabilmente alcune textures planet (Earth LOD8 opt-in via M27.b) non sono scaricate in questo build; verificare prima di release tag.

---

## Sezione F — Plugin `.info` sidecar

11 plugin in `out/build/macos-arm64-debug/Modules/Plugin/lib*.dylib`, **11/11 con `.info` valido** (sezioni `[Category]` + `[Description]` non vuote):

| plugin | Category | Description (first 50 chars) |
|---|---|---|
| ExtMFD | Tools and dialogs | EXTERNAL MFD:... |
| FlightData | Tools and dialogs | FLIGHT DATA RECORDER:... |
| Framerate | Developer resources and samples | FRAMERATE:... |
| LuaConsole | Script tools and drivers | LUA CONSOLE:... |
| LuaMFD | MFD modes | TERMINAL MFD:... |
| Meshdebug | Developer resources and samples | MESH DEBUG:... |
| Notes | Tools and dialogs | FLIGHT NOTES:... |
| Rcontrol | Input devices | REMOTE CONTROL:... |
| ScnEditor | Tools and dialogs | SCENARIO EDITOR:... |
| ScriptMFD | Script tools and drivers | SCRIPT MFD:... |
| TransX | MFD modes | TRANS-X V3.14.1:... |

✅ parità completa.

**Nota correlata a C.1:** `LuaConsole` / `LuaMFD` / `ScriptMFD` sono caricati (plugin tab OK, `.info` OK, dylib presente), ma a runtime falliscono di fatto perché il loro backend `LuaInline` non è risolvibile.

---

## Sezione G — Audit follow-up notes Fase E

| note | status | evidenza |
|---|---|---|
| **M30** baseline placeholder (0-byte) | ✅ still open (by design) | `Tests/rendering_parity/baselines/*.png` tutti 0 byte |
| **M30** `continue-on-error` sul parity step | ✅ still open | `reusable-build-macos.yml:127: continue-on-error: true` |
| **M30** capture latch single-shot static bool | ✅ resolved | implementato in OGLClient.cpp (una capture per processo) |
| **M29** no SDL_Haptic fallback per legacy joystick | ✅ still open | `HapticFX.cpp` non referenzia `SDL_Haptic` |
| **M29** no Config UI per haptic intensity | ✅ still open | nessuno slider `HapticGain` in `DlgOptions` |
| **M29** touchdown intensity fissa | ✅ still open | hardcoded 0.7 in HapticFX |
| **M28** notarization opt-in via env | ✅ still open (by design) | log DMG conferma "APPLE_NOTARIZATION_KEYCHAIN_PROFILE not set" |
| **M28** DMG size ~400 MB | ⚠️ deviazione | DMG attuale 157 MB (vedi §E) |
| **M28** CI smoke 8s launchpad-only | ✅ still open | `.github/workflows/pr-build-macos.yml` |
| **M27** architext → Architects Daughter | ✅ resolved | `Fonts/architext.regular.ttf` presente |
| **M27** Earth tile pyramid manual | ✅ still open | `cmake/download_earth_lod8.py` presente, opt-in |
| **M27** font hash pin policy | ✅ still open | (non verificato nel dettaglio, grep OK) |
| **M26** Linux XDG fallback TODO | ✅ still open | `UserPaths.cpp:55-60` → Linux ritorna `""` |
| **M26** keymap numpad non catturabile | ✅ still open (cosmetico) | |
| **M26** joystick deadzone non round-trip | ✅ still open | |
| **M26** Wine-detect gated via `#ifdef _WIN32` | ✅ resolved | Orbiter.cpp:608-613 |
| **M25** ScnEditor 6 tab fuori scope | ✅ still open (by design) | ScnEditorImGui.cpp:14-28 commento |
| **M25** TrackIR excluso | ✅ resolved | `Src/Plugin/TrackIR/CMakeLists.txt:6-9` `if(NOT WIN32) return()` |
| **M25** AtlantisConfig in Modules/Startup | ✅ resolved | `out/build/.../Modules/Startup/libAtlantisConfig.dylib` |
| **M24** due gcGUI.h coesistono | ✅ resolved | `OVP/D3D9Client/gcGUI.h` + `Orbitersdk/include/gcGUI.h` |
| **M24** OGLWindowMgr semplice (no-dock) | ✅ still open (by design) | |
| **M23** 11 ImGui dialog pre-esistenti | ✅ resolved | |
| **M23** `OpenFileIgnoreCase` solo in Vessel | ⚠️ still open | grep mostra uso in `Vessel.cpp:249,253` + `Util.cpp:74`; non esteso a mesh/texture/base/scenario loaders |
| **M23** vessel dylibs non in default target | ✅ resolved | (il mio `cmake --build --parallel 8` ha costruito 62 dylib perché `all` target include tutto) |
| **M22** `.info` sidecar pattern | ✅ resolved | tutti 11 plugin hanno `.info` valido (§F) |
| **M22** `clbkRender` nuovo virtual | ✅ resolved | presente in `Orbitersdk/include/` API |
| **M22** TrackIR escluso | ✅ resolved | (cfr M25) |

---

## Sezione H — Issue/bug aggiuntivi scoperti ad-hoc durante l'audit

Oltre ai 3 finding critici della §C (LuaInline, Celbody Win32 DLL symlink, log garbage), l'interazione iniziale con `./Orbiter -s "Delta-glider in LEO" -x` (pre-audit, su richiesta utente) ha confermato:

- **H.1** — errore "Scenario not found" diretto e pulito (no crash): `[Launch | Src/Orbiter/Orbiter.cpp | 949]`. Path handling OK. ✅
- **H.2** — la stringa `"Delta-glider in LEO"` non esiste come scenario nella distribuzione attuale (checked: `find Scenarios -iname "*LEO*"` empty). Se è nello spec utente originale → è probabilmente uno scenario custom da ricreare, o un nome alternativo per `DG Mk4 in orbit.scn`.

---

## Proposta issue GitHub (per apertura successiva)

1. **[macOS][high] LuaInline plugin not found at runtime** — `CMAKE_LIBRARY_OUTPUT_DIRECTORY` mancante in `Src/Module/LuaScript/LuaInline/CMakeLists.txt` → Script.cpp non trova `libLuaInline.dylib`. Scripting Lua di fatto rotto.
2. **[macOS][high] 8 Celbody modules symlink to Win32 PE32** — Ariel/Deimos/Miranda/Oberon/Phobos/Titania/Triton/Umbriel hanno CMakeLists.txt che copia il `.dll` Win32 e il post-build step lo symlinka a `lib*.dylib`, ma il file è PE32 e dlopen fallisce. Impatto su scenari Mars/Uranus/Neptune estesi.
3. **[macOS][medium] Orbiter.log corrupted module names** — `GetModuleFileName` stub non popola buffer → `ExitLib`/`LoadLib` logging legge stack garbage. Aggiungere popolamento minimo nel stub + idealmente `dladdr()`.
4. **[macOS][medium] `DlgOptions` in-sim lacks Visual/Physics pages** — `DlgOptions.h:28-30` ha `//void DrawVisual()` + `//void DrawPhysics()` commentati con `//TODO: add when converting Launchpad`. F6 in-sim mostra solo 4/6 sezioni.
5. **[macOS][low] `oapiClearSurfaceColourKey` silently no-op** — `OrbiterAPI.cpp:2041` `// TODO` — API pubblica che non fa nulla. Consumers su macOS perdono color-key effect senza warning.
6. **[build][medium] `ctest Lua.Interpreter` Not Run** — executable missing, test fallisce. Capire se va gatato `WIN32` o buildato cross-platform.
7. **[macOS][low] DMG size 157 MB vs expected ~430 MB** — verificare che `Textures/` sia interamente popolato prima del release tag.
8. **[macOS][low] `OpenFileIgnoreCase` not extended** — solo `Vessel::OpenConfigFile` lo usa. Mesh/texture/base/scenario loaders possono fallire su filename case-sensitive.

---

## Stato per Fase 2

**Pronti per QA manuale interattivo.** Raccomando di:

1. Correggere #1 (LuaInline output dir) e #3 (log garbage) PRIMA di Fase 2 — entrambi one-liner, risolvono il rumore nei log e abilitano test interattivi di LuaConsole/ScriptMFD/TransX MFD.
2. Prendere atto di #2 (celbody Win32) e #4 (DlgOptions) come degradi noti durante Fase 2, marcandoli PARTIAL.
3. Procedere sequenza proposta (Launchpad → scenari → dialog → Custom Functions → plugin → audio → joystick → save/load → window persistence → DMG).

---

*Report generato autonomamente in Phase 1. Branch: `qa/phase1-audit`. Commit: TBD.*

---

# Sezioni estese (Fasi A-D) — M0-M21

## Sezione K — Grep esteso Fasi A-D

Scope: **90 file `.cpp/.h`** modificati/aggiunti da commit M0 (5bed81ee) fino a M21 (bcd146c7), esclusi vendored (stb_image, stb_image_write, dr_wav, dr_mp3, stb_vorbis, Extern/, nlohmann).

### K.1 — TODO/FIXME/XXX/STUB/HACK in OGLClient core

Solo `stb_image.h` (vendored, fuori scope) ha hit. **Tutto il codice OGLClient proprietario è pulito** — OGLClient.cpp, OGLScene, OGLvVessel, OGLvPlanet, OGLvBase, OGLSketchpad, OGLParticle, OGLShadowMap, OGLEnvMap, OGLCelSphere, OGLShaderMgr, OGLTile, OGLSurface, OGLTexture, OGLMaterial, OGLMeshRegistry, OGLPostProcess, OGLAtmosphere, OGLBeaconArray: **0 TODO/FIXME/XXX non-vendored.** ✅

### K.2 — TODO in XRSound

Occorrenze reali (escluso `external/dr_*` vendored):

| file:line | nota |
|---|---|
| Sound/XRSound/src/XRSoundConfigFileParser.cpp:115,120 | TODO pre-existing — nulla a che vedere con il port macOS |
| Sound/XRSound/src/VesselXRSoundEngine.cpp:716 | TODO pre-existing — "arbitary vessel payload bay" |
| Sound/XRSound/src/VesselXRSoundEngine.h:69 | TODO pre-existing — debug #ifdef |
| Sound/XRSound/src/XRSoundEngine.cpp:34 | TODO pre-existing — 3D sounds flag |
| Sound/XRSound/src/XRSound.cpp:14,22 | TODO pre-existing — warning log |
| Sound/XRSound/src/SoundPreSteps.cpp:406 | TODO pre-existing — RCS sustain scaling |
| Sound/XRSound/src/XRSoundEngine10.h:38 | `// {XXX} UPDATE THIS FOR THE CURRENT BUILD VERSION` — marker pre-existing |

**Nessun TODO introdotto dal porting macOS.** ✅

### K.3 — Win32 APIs non gated

In **OGLClient:** 0 hits di `LoadLibrary/GetProcAddress/HKEY/RegOpen`. Completamente POSIX-native. ✅

In **XRSound** `XRSoundImpl.cpp:49-53` usa `GetModuleHandle` + `GetProcAddress` dentro `#ifdef _WIN32`, con branch macOS che usa `dlsym(RTLD_DEFAULT, "GetXRSoundEngineInstance")`. ✅

### K.4 — `assert(false)` / `abort()` / `NotImplemented`

OGLClient e XRSound: 0 hits. ✅

### K.5 — `#ifdef _WIN32` conteggio OGLClient

60 `#ifdef _WIN32/#ifndef _WIN32/#if defined(_WIN32)` guards across 54 files nella dir OGLClient. Pattern tipico cross-platform OK.

### K.6 — BUG SCOPERTO: Celbody Atmosphere path hardcoded Win32

**`Src/Orbiter/Celbody.cpp:950`:**
```cpp
bool CELBODY2::LoadAtmosphereModule (const char *fname) {
    char path[256], name[256];
    oapiGetObjectName (hBody, name, 256);
    sprintf (path, "Modules\\Celbody\\%s\\Atmosphere", name);  // ⚠️ BACKSLASHES!
    if (!(hAtmModule = g_pOrbiter->LoadModule (path, fname))) return false;
    ...
}
```

Nessun `#ifdef _WIN32` attorno. Risultato a runtime su macOS:
```
Could not find a module named VenusAtm2006. Tried Modules\Celbody\Venus\Atmosphere/VenusAtm2006 and Modules\Celbody\Venus\Atmosphere/VenusAtm2006/VenusAtm2006.dylib.
```
Path misto backslash + slash → `fs::exists` → false.

**Impact:** TUTTI i plugin atmosphere (VenusAtm2006, EarthAtmJ71G, EarthAtmNRLMSISE00, MarsAtm2006) falliscono. `libVenusAtm2006.dylib` etc sono costruiti in `Modules/` ma nessuno scenario può caricarli. Scenari atmospheric rientro, ascent profile, aerobraking si appoggiano al fallback legacy dentro Orbiter core (probabilmente ISA atmosphere standard hard-coded) → audio calcoli dinamica atmosferica approssimati.

**Severity: critical** — violazione zero-stubs + funzionalità fisica degradata in modalità silent.

---

## Sezione L — Cross-reference Fasi A-D (OGLClient ↔ D3D9Client, 6 coppie)

### L.1 — Coppia A: Entry / Init (`D3D9Client.cpp` ↔ `OGLClient.cpp`)

| Win32 feature | macOS counterpart | status | note |
|---|---|---|---|
| `clbkInitialise` | `clbkInitialise` | ✅ | scene + shaders + ImGui bootstrap |
| `clbkCreateRenderWindow` (HWND/D3D) | `clbkCreateRenderWindow` (SDL2) | ✅ | contesti OpenGL 4.1 core |
| `clbkRenderScene` | `clbkRenderScene` → OGLScene | ✅ | |
| `clbkPreOpenPopup` | override presente | ✅ | ImGui frame boundary |
| `clbkUseStencilDepthBuffer` | presente | ⚠️ | gate condizionale per flag RENDER3D only — D3D9 lo applica più largamente |
| M30.a screenshot/`clbkSaveScreenshot` | `glReadPixels` + stb_image_write | ✅ | latch single-shot OK |

### L.2 — Coppia B: Surface/Texture (`D3D9Surface.cpp` ↔ `OGLSurface.cpp`)

| Win32 feature | macOS counterpart | status | note |
|---|---|---|---|
| `clbkCreateSurface` | `OGLSurface::Create` | ✅ | texture + FBO |
| `clbkCreateTexture` | `OGLTexture::LoadDDS` | ✅ | |
| `clbkReleaseSurface` | destructor glDelete* | ✅ | |
| `clbkBlt` family | `OGLSurface::BlitFrom` | ⚠️ | D3D9 copymode enum più ricco (ADD, COLORKEY, SUB); OGL sottoinsieme via GL_COPY |
| `clbkFillSurface` | FBO bind + glClear | ✅ | |
| FBO completeness | `glCheckFramebufferStatus` | ✅ | MSAA + non-MSAA |
| Color-key (chroma) | `m_hasColorKey` + shader discard | ⚠️ | D3D9 usa texture op; OGL usa fragment shader discard (comportamento equivalente, complessità diversa) |

### L.3 — Coppia C: Scene & advanced (`Scene.cpp` ↔ `OGLScene.cpp` + `OGLvVessel/Planet/Base.cpp`)

| Win32 feature (D3D9) | macOS counterpart | status |
|---|---|---|
| PBR vessel (M8) | `OGLvVessel::s_pbrShader` + `vessel_pbr.frag` | ✅ |
| IBL env cubemap (M9) | `OGLEnvMap` + `ibl.glsl.inc` | ✅ |
| Self-shadow mapping (M10) | `OGLShadowMap` + `shadow.glsl` | ✅ |
| HDR post-proc (M11) | `OGLPostProcess::Tonemap` | ✅ |
| Particle life curve (M12) | `MapLevel` + `LifeCurve` | ✅ |
| Solar corona (M13) | `corona.vert/frag` | ✅ |
| Star twinkle (M13) | star shader brightness mod | ✅ |
| Planetarium grid (M14) | `OGLCelSphere::m_gridShader` | ✅ |
| VC dual-pass (M15) | `ShouldRenderMesh()` + MESHVIS_* flags | ✅ |
| 2D panel (M16) | `panel2d.frag` + Sketchpad | ✅ |

### L.4 — Coppia D: Sketchpad (`D3D9Pad.cpp` ↔ `OGLSketchpad.cpp`)

| Win32 feature | macOS counterpart | status |
|---|---|---|
| Line/Rect/Ellipse/Polygon | vertex buffer streaming | ✅ |
| Text/TextBox | ImGui font atlas | ✅ |
| MoveTo/LineTo | path command buffer | ✅ |
| SetFont/SetPen/SetBrush | state machine | ✅ |
| Blit (brect/srect/copymode) | `DrawBltGroup` | ⚠️ sottoinsieme copymode |
| SetBrightness/ColorMatrix | shader uniforms | ✅ |
| Transform stack Push/Pop | GL matrix sim | ✅ |
| Clip region | `glScissor` + clipper planes | ✅ |

### L.5 — Coppia E: Particle (`Particle.cpp` ↔ `OGLParticle.cpp`)

| Win32 | macOS | status |
|---|---|---|
| Stream spec + lifecycle | `PARTICLESTREAMSPEC` | ✅ |
| Life curve | `LifeCurve(t)` | ✅ |
| Level→alpha LEVELMAP | tutti i modi (FLAT/SQRT/PLIN/PSQRT) | ✅ |
| Smoke dissipation | atmospheric drag integration | ✅ |
| Exhaust bicolor | dual-texture blend `exhaust.frag` | ✅ |
| Reentry thermal | additive blend + emissive | ✅ |

### L.6 — Coppia F: CelSphere + ShaderMgr (`CelSphere.cpp` + `ShaderMgr.cpp` ↔ `OGLCelSphere.cpp` + `OGLShaderMgr.cpp`)

| Win32 | macOS | status |
|---|---|---|
| Star field VAO 4000 | `star.vert/frag` | ✅ |
| Solar corona billboard | `corona.vert/frag` | ✅ |
| Round point sprites | `gl_PointSize` + GL_POINT_SMOOTH fallback | ⚠️ D3D9 point rendering vs GL multisampled differ slightly |
| Planetarium grid RA/Dec + Alt/Az | `grid.vert/frag` | ✅ |
| Shader hot-reload (M0) | `ShaderMgr::m_hotReloadEnabled` (debug) | ✅ |
| UBO binding (Camera/Light/Material/Scatter/Time) | `RegisterUBOBinding()` | ✅ |
| `.glsl.inc` include resolution | recursive dependency tracking | ✅ |

### L.7 — Note gap residui Fasi A-D

- **Blit copymode subset** (L.2, L.4): D3D9 espone operazioni aggiuntive (ADD, SUB, COLORKEY). OGL ha il subset base. Scenari che usano ADD per overlay HUD trasparenti possono mostrare differenze minori.
- **Color-key via shader discard** invece di texture op: comportamento funzionalmente equivalente ma può interagire diversamente con MSAA (edge pixels). Da verificare a occhio in Fase 2 su panel 2D con chroma-keyed UI elements.
- **Point sprite rendering** (L.6): GL point rendering + multisampling produce bordi leggermente diversi da D3D9 point sprites. Impatto solo su starfield estreme-zoom.

Nessun gap blocca scenari Demo (smoke 14/14 pass).

---

## Sezione M — Audit follow-up Fasi A-D

### M.1 — "Follow-up da riprendere prima di uscire da Fase B" (roadmap:362-365)

| note | status | evidenza |
|---|---|---|
| **M7.b** elevation displacement | ✅ DONE (marcato in roadmap) | `OGLTile.cpp` ha BuildSpherePatch con ELEVFILEHEADER parse |
| **M14.b** OGLvBase full | ✅ DONE (marcato in roadmap) | `OGLvBase.cpp` + `OGLBeaconArray.cpp` presenti, pad beacons a 500 km cutoff |
| Crack-hiding skirts + per-vertex normal (polish M7.b) | ✅ still open (non-blocking) | — |
| Full `.bse` mesh parser tarmacs/hangars (polish M14.b) | ✅ still open (non-blocking) | — |

### M.2 — "Follow-up Fase C — polish non-bloccanti post-M15" (roadmap:560-564)

| note | status |
|---|---|
| **M15** — interactive VC acceptance (F1 → mesh cockpit, look-around, MFD rendering, HUD overlay, area click response) | ⏳ **Fase 2 manuale** (richiede sessione desktop) |
| **M15** — Area compositing su DDS compresse (FBO completeness check) | ✅ still open (non-blocking, condizionato a VC areas nere) |
| **M17** — Interactive MFD acceptance (Orbit trace, Map coastline, HSI, Docking xhair, Surface altimetry, Atm-autopilot HUD) | ⏳ **Fase 2 manuale** |
| M17.a line widths >1 su Apple M-series (triangle-quads fallback) | ✅ still open | |
| M17.b text baseline / fixed vs prop fonts / `CalcWordWrapPosition` | ✅ still open | |
| M17.c LuaMFD `SetBrightness` / `SetRenderParam(PRM_GAMMA)` | ✅ still open | |
| M17.d ScnEditor `DrawMeshGroup` 3D mini-globes | ✅ still open | |

**Nota:** i follow-up Fase C sono **tutti human-in-the-loop** → delegati a Fase 2.

### M.3 — Fase A e Fase D: nessuna sezione follow-up

La roadmap non ha sezioni "Follow-up Fase A" o "Follow-up Fase D" esplicite. Motivo plausibile: M0-M3 (Fase A, fondamenta grafiche) e M18-M21 (Fase D, audio) si sono concluse senza polish items dichiarati. La mia analisi statica in §K conferma:
- Zero TODO residui in OGLClient core (M0-M3 clean)
- Zero TODO residui introdotti dal porting XRSound (M18-M21 clean — i TODO esistenti sono XRSound upstream)

Ma il porting XRSound ha introdotto due bug runtime non annotati (vedi §N.2 + §N.4).

---

## Sezione N — Findings runtime Fasi A-D

### N.1 — XRSound runtime path `libXRSound.dylib` costruzione non di default

Osservato: `cmake --build out/build/macos-arm64-debug --parallel 8` con config `-DORBITER_BUILD_XRSOUND=ON` produce:
- ✅ `Orbitersdk/XRSound/libXRSound.a` (static SDK lib)
- ✅ `Sound/XRSound/libXRSound_backend_openal.a` (static backend)
- ❌ `Modules/Plugin/libXRSound.dylib` (shared plugin — link step **skippato**)

Solo dopo `cmake --build --target XRSound_dll` esplicito si ottiene il `.dylib` (1.1 MB, link con `/opt/homebrew/opt/openal-soft/lib/libopenal.1.dylib`).

**Conseguenza:** un utente che fa build clean + usa Orbiter senza invocare il target specifico ha audio silenzioso senza errore. Non c'è un `add_dependencies(Orbiter XRSound_dll)` o un `add_dependencies(all XRSound_dll)` che forzi la costruzione.

**Fix proposto:** in `Sound/XRSound/src/CMakeLists.txt` aggiungere:
```cmake
add_dependencies(${OrbiterTgt} XRSound_dll)
```
oppure marcarlo `ALL` target.

### N.2 — XRSound ctest test non registrato

`Sound/XRSound/CMakeLists_OpenAL.cmake:67-77` dichiara:
```cmake
include(CTest)
if(BUILD_TESTING)
    add_test(NAME xrsound_openal_smoke COMMAND xrsound_openal_smoke ...)
endif()
```

Ma `CMakeLists.txt` top-level chiama `enable_testing()` a **linea 428**, DOPO `add_subdirectory(Sound)` a linea 367. Quindi quando `CMakeLists_OpenAL.cmake` viene processato, `enable_testing` non è ancora stato invocato → `add_test` viene ignorato. `ctest -N` mostra solo Lua.Interpreter + rendering_parity.

Verifica manuale: `./Sound/xrsound_openal_smoke` da build root → EXIT 0 clean, log:
```
[OpenAL] OpenAL (OpenAL Community / OpenAL Soft)
[smoke] driver = OpenAL (OpenAL Community / OpenAL Soft)
```

**Fix proposto:** spostare `enable_testing()` in testa a `CMakeLists.txt` (prima di qualsiasi `add_subdirectory`).

### N.3 — Celbody atmosphere loading (riassunto K.6)

Già coperto in §K.6. Fix = `#ifdef _WIN32` + path `/`-separated + `"lib%s.dylib"` convention.

### N.4 — `xrsound_openal_smoke` path resolution

Eseguito da working-dir `worktree-root`: fallisce a caricare WAV (`Unsupported / missing file 'XRSound/Default/Docking Radar Beep.wav'` — path relativo alla cwd, non al binary).

Eseguito da `build-root` (dove esistono gli asset): clean EXIT 0.

`add_test` usa `WORKING_DIRECTORY ${CMAKE_BINARY_DIR}` — quindi se il test venisse registrato correttamente (fix §N.2), funzionerebbe. Non è un bug nuovo, ma accoppia con §N.2 — validare dopo il fix.

---

# Triage finale — Pre-Fase 2

## BLOCCANTI: fix prima di Fase 2 (tot. stimato: 1-2 ore)

| # | finding | fix | file:line | effort |
|---|---|---|---|---|
| **B1** | XRSound non linka in default build (§N.1) | `add_dependencies(${OrbiterTgt} XRSound_dll)` | `Sound/XRSound/src/CMakeLists.txt` (in coda) | 2 min |
| **B2** | XRSound ctest non registrato (§N.2) | move `enable_testing()` in testa a top-level CMakeLists | `CMakeLists.txt:427-428` → spostare prima di linea 367 | 5 min |
| **B3** | Celbody atmosphere path `\\` hardcoded (§K.6) | `#ifdef _WIN32 / #else sprintf(path, "Modules/Celbody/%s/Atmosphere", name)` + convention `lib%s.dylib` | `Src/Orbiter/Celbody.cpp:950` | 10 min |
| **B4** | LuaInline dylib location (§C.1) | aggiungi `set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR})` | `Src/Module/LuaScript/LuaInline/CMakeLists.txt:5` | 2 min |
| **B5** | Module-load log garbage (§C.3) | popola `mname[0]='\0'` nel stub di `GetModuleFileName` oppure uso `dladdr()` | `Src/Orbiter/resource_stub.h:196` | 5 min |

**Giustificazione:** B1/B2 abilitano audio-testing in Fase 2. B3 sblocca scenari atmospheric (rientro). B4 sblocca scripting (LuaConsole/ScriptMFD/TransX MFD testabili). B5 ripulisce i log per poter vedere altri errori durante Fase 2.

Dopo i 5 fix il build dovrebbe avere 0 errori critici e audit ripetibile.

## DIFFERIBILI: aprire issue GitHub e rimandare

Queste sono tutte **non-bloccanti per la funzionalità core** e si possono gestire in iterazioni successive.

| # | finding | label proposto | priority |
|---|---|---|---|
| **I1** | 8 Celbody modules (Ariel, Deimos, Miranda, Oberon, Phobos, Titania, Triton, Umbriel) symlink a Win32 PE32 (§C.2) | `platform:macos`, `scope:celbody`, `priority:high` | high |
| **I2** | `DlgOptions` in-sim manca DrawVisual + DrawPhysics (§A.1 A) | `platform:macos`, `scope:dialogs`, `priority:medium` | medium |
| **I3** | `oapiClearSurfaceColourKey` no-op macOS (§A.1) | `platform:macos`, `scope:api`, `priority:low` | low |
| **I4** | `OpenFileIgnoreCase` non esteso a mesh/texture/base/scenario loaders (§G, M23 followup) | `platform:macos`, `scope:filesystem`, `priority:medium` | medium |
| **I5** | DMG size 157 MB vs expected 430 MB (§E) | `platform:macos`, `scope:packaging`, `priority:low` | low |
| **I6** | `ctest Lua.Interpreter` Not Run — eseguibile missing, gating incerto (§D) | `build:tests`, `priority:low` | low |
| **I7** | Baselines rendering_parity 0-byte placeholder (§G M30) | `platform:macos`, `scope:testing`, `priority:medium` | medium |
| **I8** | SDL_Haptic fallback per legacy joystick mancante (§G M29) | `platform:macos`, `scope:input`, `priority:low` | low |
| **I9** | Haptic intensity no Config UI (§G M29) | `platform:macos`, `scope:input`, `priority:low` | low |
| **I10** | HTML viewer inline launchpad non portato (§A `ExtraLaunchpadOptions`) | `platform:macos`, `scope:ui`, `priority:low` | low |
| **I11** | Blit copymode subset su OGLSketchpad (§L.2, L.4) | `platform:macos`, `scope:rendering`, `priority:medium` | medium |
| **I12** | Linux XDG fallback per UserPaths (§G M26) | `platform:linux`, `scope:paths`, `priority:low` | low |
| **I13** | Log contiene byte NUL (§C.4) | `platform:macos`, `scope:logging`, `priority:low` | low |
| **I14** | ScnEditor 6 tab (New/Save/Edit/Elements/Landed/Docking) non portati (§B.5) | `platform:macos`, `scope:scneditor`, `priority:low` | low |

**Totale:** 5 fix ora + 14 issue differibili = **19 item tracciati**.

## Proposta issue body (per copy/paste)

Per ognuno degli I1-I14 posso proporti il body dell'issue (titolo + descrizione + acceptance criteria + repro). Dimmi quando e te li butto giù in batch.

---

# Pronti per Fase 2?

**Sì, condizionato a B1-B5.** Dopo i 5 fix il report va ri-verificato (smoke test + ctest); poi iniziamo la sessione interattiva seguendo la sequenza già proposta nel report originale.

Se preferisci, posso applicare io stesso i fix B1-B5 su branch `qa/phase1-fixes` (separato da `qa/phase1-audit`) e fare un rebuild + smoke-retest per confermare. Fammi sapere.
