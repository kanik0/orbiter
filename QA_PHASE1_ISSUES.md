# QA Phase 1 — Issues da aprire su GitHub

Questi sono i 15 issue derivati dall'audit Phase 1 (cfr. [QA_PHASE1_REPORT.md](QA_PHASE1_REPORT.md)), scritti in formato pronto per `gh issue create`.

**Nota:** I1-I14 sono i deferrable originali del report. **I15** è stato aggiunto dopo che il tentativo di fix B4 (LuaInline) ha esposto un crash runtime: il fix è stato intenzionalmente non-applicato e la porting di LuaInline Win32→POSIX è diventata un tracking separato.

I 4 fix applicati su branch `qa/phase1-fixes` (commit `b4456f43`) hanno risolto: B1 (AscentMFD linker), B2 (`enable_testing` ordering), B3 (Celbody atmosphere path), B5 (module-log garbage).

---

## I1 — [macOS][high] 8 Celbody modules symlink to Win32 PE32 → `dlopen` fails

**Labels:** `platform:macos`, `scope:celbody`, `priority:high`

**Body:**

The post-build step in `Src/Orbiter/CMakeLists.txt` creates `Modules/Celbody/lib<Name>.dylib` symlinks for 8 celbody modules (Ariel, Deimos, Miranda, Oberon, Phobos, Titania, Triton, Umbriel). These point at the raw `.dll` files copied by `Src/Celbody/<Name>/CMakeLists.txt`, which are Windows PE32 binaries — not Mach-O arm64. Result: `dlopen` fails silently every time a scenario references one of these bodies.

```
dlopen(Modules/Celbody/libAriel.dylib, 0x0006): tried: 'Modules/Celbody/libAriel.dylib' (slice is not valid mach-o file)
```

These 8 `CMakeLists.txt` only `file(COPY)` the DLL without a real build step (no `add_library`). The sources may or may not be included in the repo full tree.

**Repro:**
```sh
cmake --preset macos-arm64-debug -DORBITER_BUILD_XRSOUND=ON
cmake --build out/build/macos-arm64-debug --parallel 8
./Orbiter --scenario="Demo/The 1999 solar eclipse" --capture-frame=60 --capture-out=/tmp/eclipse.png
grep -i dlopen ~/Library/Logs/Orbiter/Orbiter.log
```

**Expected:** each celbody loaded cleanly on macOS.

**Acceptance:**
- [ ] 8 bodies either build from sources (if available) as real macOS arm64 dylibs OR are explicitly excluded on non-Win32 with a clear log note.
- [ ] Symlink bridge in `Src/Orbiter/CMakeLists.txt` does not point `.dylib` at Win32 `.dll` on non-Win32.
- [ ] Running Mars scenarios (Phobos/Deimos) or extended Uranus/Neptune scenarios no longer logs `slice is not valid mach-o file`.

**Impact:** scenarios using these bodies fall back to hard-coded orbital parameters or silent positional drift. Impact low for Demo scenarios, potentially visible for Mars/Uranus/Neptune extended scenarios.

---

## I2 — [macOS][medium] `DlgOptions` in-sim missing Visual & Physics pages

**Labels:** `platform:macos`, `scope:dialogs`, `priority:medium`

**Body:**

`Src/Orbiter/DlgOptions.h:28` declares two methods commented out with a TODO:
```cpp
//TODO: add when converting Launchpad
//void DrawVisual();
//void DrawPhysics();
```

Resulting in-sim F6 dialog shows 4 panels (Instrument / Vessel / UI / Joystick) vs. the 6 panels available on Windows (Visual / Physics / Instrument / Vessel / UI / Joystick).

The Launchpad Options tab (via `OGLLaunchpad::RenderOptionsPage*`) ships all 12 pages. It's only the **in-sim dialog** wrapper that's incomplete.

**Acceptance:**
- [ ] `DlgOptions::DrawVisual()` and `DrawPhysics()` implemented mirroring Win32 `DlgOptions` behaviour.
- [ ] F6 in-sim dialog shows all 6 sections.
- [ ] Settings persist through save/load scenario.

---

## I3 — [macOS][low] `oapiClearSurfaceColourKey` silent no-op

**Labels:** `platform:macos`, `scope:api`, `priority:low`

**Body:**

`Src/Orbiter/OrbiterAPI.cpp:2041`:
```cpp
DLLEXPORT void oapiClearSurfaceColourKey (SURFHANDLE surf)
{
    if (!surf) return;
    oapi::GraphicsClient *gc = g_pOrbiter->GetGraphicsClient();
    //if (gc) gc->clbClearSurfaceColourKey (surf); // TODO
#ifdef _WIN32
    ((LPDIRECTDRAWSURFACE7)surf)->SetColorKey (DDCKEY_SRCBLT, 0);
#endif
}
```

On non-Win32 this is a complete no-op. Vessels or plugins calling the API expecting color-key to be cleared silently observe the chroma still being keyed.

**Acceptance:**
- [ ] Route through `oapi::GraphicsClient::clbClearSurfaceColourKey` on non-Win32.
- [ ] `OGLSurface` implements the clear by disabling the shader's color-key discard path.
- [ ] At least one regression test covers the round-trip.

---

## I4 — [macOS][medium] `OpenFileIgnoreCase` not extended beyond `Vessel::OpenConfigFile`

**Labels:** `platform:macos`, `scope:filesystem`, `priority:medium`

**Body:**

Follow-up noted under `MACOS_ROADMAP.md` → M23 post-fix. `OpenFileIgnoreCase` (defined in `Src/Orbiter/Util.cpp:74`, declared in `Src/Orbiter/Util.h:44`) currently wraps only `Vessel::OpenConfigFile` (`Src/Orbiter/Vessel.cpp:249,253`).

Mesh loaders, texture loaders, base loaders, scenario parsers all still open files via raw `std::ifstream` — so case-sensitive POSIX filesystems surface lookup mismatches that the Windows build hides. For example a scenario referencing `DeltaGlider.cfg` works where the on-disk file is `Deltaglider.cfg`; but the same mismatch in `DeltaGlider.msh` silently loads a wireframe fallback.

**Acceptance:**
- [ ] Audit mesh/texture/base/scenario loaders, identify every case-sensitive file open path.
- [ ] Route each through `OpenFileIgnoreCase` (or a new case-insensitive `std::filesystem::path` resolver).
- [ ] Regression test: scenario referencing files with mismatched case loads cleanly on macOS.

---

## I5 — [macOS][low] DMG size 157 MB vs expected ~430 MB

**Labels:** `platform:macos`, `scope:packaging`, `priority:low`

**Body:**

Roadmap M28 noted expected DMG ~430 MB (dominated by `Textures/` ~400 MB + `Scenarios/`). The debug build DMG currently produces **157 MB**:

```
$ du -h out/build/macos-arm64-debug/Orbiter-macos-arm64.dmg
157M    …/Orbiter-macos-arm64.dmg
```

Likely causes:
- Earth LOD8 blue-marble textures are opt-in via `-DORBITER_FETCH_EARTH_BLUEMARBLE=ON` and not fetched by default.
- Some `Textures/` subdirs may not be populated by the debug asset pipeline.

**Acceptance:**
- [ ] Verify which texture assets are missing from the debug build vs. a full release build.
- [ ] Document in `MACOS_ROADMAP.md` the debug/release DMG size delta.
- [ ] (Optional) wire `ORBITER_FETCH_EARTH_BLUEMARBLE=ON` into the release.yml pipeline.

---

## I6 — [build][medium] `ctest Lua.Interpreter` aborts with `@rpath/libLuaInterpreter.dylib` not found

**Labels:** `build:tests`, `platform:macos`, `priority:medium`

**Body:**

After the `enable_testing` fix (B2 from Phase 1 triage), `ctest` now picks up the `Lua.Interpreter` binary but it fails at launch:

```
dyld[…]: Library not loaded: @rpath/libLuaInterpreter.dylib
Reason: tried: '…/macos-arm64-debug/libLuaInterpreter.dylib' (no such file),
              '…/macos-arm64-debug/../libLuaInterpreter.dylib' (no such file)
```

`libLuaInterpreter.dylib` exists at `…/macos-arm64-debug/Modules/libLuaInterpreter.dylib` — the test executable's rpath search list doesn't include `Modules/`.

**Fix sketch:**
```cmake
# Tests/CMakeLists.txt or Lua.Interpreter target
set_target_properties(Lua.Interpreter PROPERTIES
    BUILD_RPATH "@loader_path/Modules"
)
```

Or move `libLuaInterpreter.dylib` to the executable's own directory.

**Acceptance:**
- [ ] `ctest --test-dir out/build/macos-arm64-debug` passes all 3 tests.

---

## I7 — [macOS][medium] rendering_parity baselines are 0-byte placeholders

**Labels:** `platform:macos`, `scope:testing`, `priority:medium`

**Body:**

Tracked in `MACOS_ROADMAP.md` → M30 follow-up. `Tests/rendering_parity/baselines/*.png` ship as 0-byte placeholders:

```
$ ls -la Tests/rendering_parity/baselines/*.png
-rw-r--r-- 1 … 0 … atlantis_iss_dock.png
-rw-r--r-- 1 … 0 … earth_orbit_default.png
-rw-r--r-- 1 … 0 … today_solar_system.png
```

Harness end-to-end runs on every PR but skips SSIM comparison for size-0 baselines, so the test cannot regress on framebuffer drift. The workflow step is additionally `continue-on-error: true` (`.github/workflows/reusable-build-macos.yml:127`).

**Acceptance:**
- [ ] Curate a set of 20 canonical scenario baselines (roadmap M30 target) captured on the macos-14 CI runner.
- [ ] Remove `continue-on-error` from the parity step once baselines are stable.
- [ ] Document the baseline refresh procedure (when it is expected to change, who signs off).

---

## I8 — [macOS][low] HapticFX: no SDL_Haptic fallback for legacy `SDL_Joystick`

**Labels:** `platform:macos`, `scope:input`, `priority:low`

**Body:**

Tracked under M29 follow-up. `Src/Orbiter/HapticFX.cpp` drives rumble via `SDL_GameControllerRumble`. Joysticks that don't expose a GameController mapping (older flight sticks without an SDL GameController profile) don't receive feedback.

**Acceptance:**
- [ ] Detect devices without a GameController mapping, fall back to `SDL_HapticOpenFromJoystick` + `SDL_HapticRumble*`.
- [ ] Document supported joystick classes in `MACOS_ROADMAP.md` M29 section.

---

## I9 — [macOS][low] HapticFX intensity has no Config UI

**Labels:** `platform:macos`, `scope:input`, `priority:low`

**Body:**

Tracked under M29 follow-up. Rumble intensities are hard-coded:
- Touchdown: 0.7
- EngineIgnite: scaled to throttle
- AtmosphericBuffet: 5..50 kPa linear

**Acceptance:**
- [ ] Add a `CfgJoystickPrm.HapticGain` slider (0.0..1.0) in the Joystick calibration dialog.
- [ ] Persist through `Orbiter.cfg`.
- [ ] All three effects scaled by the gain at emit time.

---

## I10 — [macOS][low] HTML viewer inline in Launchpad Options unavailable

**Labels:** `platform:macos`, `scope:ui`, `priority:low`

**Body:**

`ItemLaunchpadOptions` shows a `TextDisabled`: "On macOS the inline HTML viewer is unavailable". Win32 embeds an IWebBrowser2 control to render scenario DESC/HYPERDESC HTML blocks inside the Launchpad. On macOS the DESC renders as plain text.

**Options:**
1. Stub with `NSWebView` / `WKWebView` (Cocoa, adds AppKit dependency)
2. Use ImGui Markdown / plain-text rendering (current state)
3. Spawn default browser for full DESC via `file://` URL + `open`

**Acceptance:**
- [ ] Design decision documented.
- [ ] If implemented: scenario HTML rendered inline or on-click.

---

## I11 — [macOS][medium] OGLSketchpad blit copy-mode subset

**Labels:** `platform:macos`, `scope:rendering`, `priority:medium`

**Body:**

Cross-reference audit (QA_PHASE1_REPORT.md §L.2, §L.4) flags that `D3D9Surface::clbkBlt` / `D3D9Pad::Blit` support a richer copymode enum (ADD / SUB / COLORKEY) than the current `OGLSurface::BlitFrom` / `OGLSketchpad::DrawBltGroup` subset. Scenarios using ADD blending for overlay HUD transparency may render slightly differently on macOS.

**Acceptance:**
- [ ] Enumerate copymode values used by shipping vessels / plugins.
- [ ] Implement missing modes in `OGLSurface` / `OGLSketchpad`.
- [ ] Regression test: HUD overlay visual parity (screenshot diff under SSIM threshold).

---

## I12 — [linux][low] XDG-compliant UserPaths not implemented

**Labels:** `platform:linux`, `scope:paths`, `priority:low`

**Body:**

`Src/Orbiter/UserPaths.cpp:55-60` returns `""` on Linux, falling back to cwd-relative `Orbiter.log` / `Orbiter.cfg`. The macOS branch uses `~/Library/Application Support` and `~/Library/Logs`.

**Acceptance:**
- [ ] `$XDG_CONFIG_HOME/Orbiter/Orbiter.cfg` (fallback `~/.config/Orbiter/Orbiter.cfg`) for config.
- [ ] `$XDG_STATE_HOME/Orbiter/log/` (fallback `~/.local/state/Orbiter/log/`) for logs.
- [ ] Document in `MACOS_ROADMAP.md` M26 follow-up completion.

---

## I13 — [macOS][low] `Orbiter.log` contains NUL bytes (`\x00`)

**Labels:** `platform:macos`, `scope:logging`, `priority:low`

**Body:**

`~/Library/Logs/Orbiter/Orbiter.log` contains scattered NUL bytes that interfere with `grep`/`cat` without `tr -d '\000'` pre-filter:

```
$ cat ~/Library/Logs/Orbiter/Orbiter.log | tail
cat: …/Orbiter.log: stream did not contain valid UTF-8
$ tr -d '\000' < ~/Library/Logs/Orbiter/Orbiter.log | tail
# works
```

Likely related to a format-string writing uninitialised buffer bytes, OR stale buffer reuse in `LOGOUT` macro. Partially mitigated by B5 fix (GetModuleFileName null-termination), but NUL bytes still appear. Worth a second pass.

**Acceptance:**
- [ ] Identify remaining format-string / uninitialised-memory write paths in `Src/Orbiter/Log.cpp`.
- [ ] `cat` / `tail -f` on the log works without pre-filter.

---

## I14 — [macOS][low] ScnEditor 6 tabs intentionally not ported (New/Save/Edit/Elements/Landed/Docking)

**Labels:** `platform:macos`, `scope:scneditor`, `priority:low`

**Body:**

M25.d explicitly scoped out 6 of the 12 Win32 ScnEditor tabs. The ImGui port ships State / Orientation / AngularVel / Propellant / Date / Vessel picker. The remaining tabs have in-sim alternatives documented in `Src/Plugin/ScnEditor/ScnEditorImGui.cpp:14-28`.

Open this issue if / when concrete user demand surfaces for any of the dropped tabs.

**Acceptance:**
- [ ] Per-tab demand ticket children (close parent when all resolved or after 6 months without demand).

---

## I15 — [macOS][high] LuaInline uses Win32 threading (SIGSEGV when loaded on POSIX)

**Labels:** `platform:macos`, `scope:lua`, `priority:high`

**Body:**

`Src/Module/LuaScript/LuaInline/LuaInline.cpp` uses Win32 threading primitives:
- `_beginthreadex`
- `WaitForSingleObject`
- `TerminateThread`
- `CloseHandle`
- `HANDLE`

These are stubbed in `Src/Orbiter/resource_stub.h` but the stubs don't actually spawn threads, so the interpreter thread is never alive. When `ScriptInterface::ExecScriptCmd` is eventually dispatched to `opcExecScriptCmd` (e.g. from `DeltaGlider::AAPSubsystem::AAP`'s constructor), the call touches an uninitialised `Interpreter*` and crashes with SIGSEGV:

```
=== CRASH: signal 11 ===
0   Orbiter                    crashHandler
2   libLuaInline.dylib         opcExecScriptCmd + 64
3   Orbiter                    ScriptInterface::ExecScriptCmd
4   Orbiter                    oapiExecScriptCmd
5   libDeltaGlider.dylib       AAP::AAP
...
```

**Current workaround:** `Src/Module/LuaScript/LuaInline/CMakeLists.txt` deliberately does not set `CMAKE_LIBRARY_OUTPUT_DIRECTORY` to Orbiter root, so `libLuaInline.dylib` stays in `Modules/` where `Script.cpp path="."` cannot find it. Result: scripting silently disabled (no crash).

**Repro (to confirm the bug after the workaround is removed):**
```cmake
# In Src/Module/LuaScript/LuaInline/CMakeLists.txt
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR})
```
```sh
cmake --build … --parallel 8
cmake --build … --target Orbiter  # post-build symlink refresh
cd out/build/macos-arm64-debug
./Orbiter --scenario="Demo/Today" --capture-frame=60 --capture-out=/tmp/t.png
# → SIGSEGV in libLuaInline opcExecScriptCmd
```

**Acceptance:**
- [ ] Port `InterpreterList::Environment::CreateInterpreter` to `std::thread` / `std::condition_variable` (or pthread directly).
- [ ] Remove the Win32 threading comment block in `Src/Module/LuaScript/LuaInline/CMakeLists.txt` and add back `LIBRARY_OUTPUT_DIRECTORY`.
- [ ] Regression: scenarios using `oapiExecScriptCmd` (DeltaGlider AAP, TransX MFD, LuaMFD, LuaConsole, ScriptMFD, ScriptVessel) run without crash.
- [ ] Add a ctest that drives `NewInterpreter` → `ExecScriptCmd("print('hello')")` → `DelInterpreter`.

**Impact:** Without this fix, all Lua-driven features on macOS are inert (silent failure). Currently affects: LuaConsole plugin UI, LuaMFD, ScriptMFD, TransX trajectory optimiser, and any vessel using `oapiExecScriptCmd`.

---

# Come aprirle

Script consigliato (da eseguire dopo aver verificato il testo):

```sh
# Dal repo root, branch main
cd /Users/massimobedini/Documents/source/orbiter

# Esempio per I1 (ripetere per I2-I15 cambiando title/body/labels):
gh issue create \
    --title "[macOS][high] 8 Celbody modules symlink to Win32 PE32 (dlopen fails)" \
    --label "platform:macos,scope:celbody,priority:high" \
    --body "$(sed -n '/^## I1 — /,/^---$/p' QA_PHASE1_ISSUES.md | sed '1d;$d')"
```

Oppure in batch con un piccolo shell loop parsando `QA_PHASE1_ISSUES.md`. Dimmi se vuoi che scriva lo script intero di auto-apertura.
