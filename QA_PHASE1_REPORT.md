# QA Phase 1 — Orbiter macOS ARM64 Parity Audit

**Data:** 2026-04-18
**Branch:** `qa/phase1-audit` (da `main` post-merge PR #7)
**Preset:** `macos-arm64-debug` + `-DORBITER_BUILD_XRSOUND=ON -DORBITER_MAKE_TESTS=ON`
**Host:** Darwin 25.4.0 / Apple M2 / OpenGL 4.1 Metal-90.5

---

## TL;DR

- **Build:** ✅ configure + full build puliti, 62 dylib reali + 11 plugin + DMG 157 MB.
- **Parity statica (Section B):** 4/5 coppie file ≥ 95% parity; 1/5 (ScnEditor) a 50% **per design** (roadmap M25.d).
- **Smoke test runtime:** 14/14 scenari `Scenarios/Demo/*.scn` rendono frame 60 e catturano PNG 178-185 KB, **0 TERMIN/critical/fatal**.
- **Tests (ctest):** 1 PASS (`rendering_parity` 45.88 s), 1 FAIL (`Lua.Interpreter` exe non presente — gating Win32-only plausibile ma non verificato).
- **DMG:** 157 MB, `hdiutil verify` clean, ad-hoc signed (no notarization env).

### ⚠️ Finding critici (violano "zero-stubs policy")

| # | severity | finding | blocker PR? |
|---|---|---|---|
| 1 | **high** | `libLuaInline.dylib` non trovato a runtime → scripting Lua rotto in ogni scenario | no (degradato, non crash) |
| 2 | **high** | 8 Celbody dylib sono symlink a Win32 PE32 → `dlopen` fallisce ripetutamente | no (degradato, non crash) |
| 3 | **medium** | Module-load log stampa nomi corrotti (`Module p��o ...`) | no (cosmetico) |
| 4 | **medium** | `DlgOptions` macOS: mancano pagine `DrawVisual` / `DrawPhysics` (TODO commentato) | no (degradato) |
| 5 | **low** | `oapiClearSurfaceColourKey` no-op su macOS con `// TODO` | no (silent no-op) |

Dettagli sotto.

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
