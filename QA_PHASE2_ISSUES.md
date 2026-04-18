# QA Phase 2 — Issues da aprire su GitHub

17 finding da Phase 2 manual QA, da filare come issue. Stesso flow Phase 1 (vedi `QA_PHASE1_ISSUES.md`).

Risolti durante la sessione (già chiusi via PR, NON filati come issue):
- **P2-B3** → PR #24 (M11 post-process black screen)
- **P2-B12** → PR #27 (InitModule never called + registry ordering)
- **P2-B23** → PR #28 (Quicksave backslash path)

Falsi positivi (chiusi in-log senza issue):
- **P2-B5** → stars actually render (visible in Atlantis cockpit test)

Totale da aprire: 17 (alcuni già coperti dalle issue #25/#26 di Phase 1 ma separati per chiarezza).

---

## P2-B1 — [macOS][cosmetic] Em-dash `—` glyph renders as `?` in Launchpad text

**Labels:** `platform:macos`, `scope:ui`, `priority:low`

**Body:**

Multiple Launchpad places display `?` where the intended character is `—` (em-dash) or similar Unicode typography character. Examples:

- Scenarios tab placeholder: `"Select a scenario to launch ? double-click to start immediately."` (first `?` should be em-dash)
- Video tab hint: `"macOS uses borderless fullscreen ? the OS picks the optimum refresh rate."`
- About tab components: `"XRSound ? Doug Beachy"`, `"ImGui ? Omar Cornut & contributors"`, etc.

Root cause: the Lekton / architext font shipped under M27.a doesn't include the em-dash glyph, and the ImGui font atlas substitutes the missing-glyph fallback.

**Acceptance:**

- [ ] Either embed a font with em-dash coverage, or swap em-dash in UI strings for ASCII `-` / `--`
- [ ] All About tab / hint-text / placeholder strings render without `?` artefacts

---

## P2-B2 — [macOS][low] Launch Orbiter button shows no enabled/disabled visual state

**Labels:** `platform:macos`, `scope:ui`, `priority:low`

**Body:**

In Launchpad Scenarios tab, the "Launch Orbiter" footer button displays the same blue fill regardless of whether a scenario is selected. Functionally it IS gated (clicking with no selection does nothing), but without a grayed-out visual users cannot tell the button is disabled until a scenario is clicked.

**Repro:**
1. Launch `./Orbiter`
2. Open Scenarios tab, do NOT click any scenario
3. Observe Launch button → same blue style as when a scenario IS selected
4. Click Launch → nothing happens (correct, just no feedback)

**Acceptance:**

- [ ] Disabled state renders with reduced opacity / grayed-out text (ImGui `PushStyleColor(ImGuiCol_Button, disabled)`)
- [ ] Hover tooltip explaining why the button is inactive (optional)

---

## P2-B7 — [macOS][cosmetic] Tree-expand icon renders as `?` in Options sidebar

**Labels:** `platform:macos`, `scope:ui`, `priority:low`

**Body:**

Options tab sidebar shows `?` in place of the tree collapse/expand indicator for "Visual helpers" children (Planetarium, Labels, Forces, Frame axes). Related to but distinct from the em-dash glyph issue (P2-B1): here the missing glyph is presumably ▶ / ▼ or a FontAwesome tree-arrow.

**Acceptance:**

- [ ] Collapsed/expanded tree nodes render with proper indicator (ImGui built-in style or matching font glyph)

---

## P2-B8 — [macOS][low] AscentMFD sample plugin exposed in user Modules tab

**Labels:** `platform:macos`, `scope:ui`, `priority:low`

**Body:**

After Phase 1 fix B1 enabled `libAscentMFD.so` to link, the sample SDK plugin shows up in the Launchpad Modules tab under "Miscellaneous". AscentMFD is a developer sample from `Orbitersdk/samples/`, not a user-facing plugin.

**Options:**

1. Hide from user UI by not installing it to `Modules/Plugin/` on release builds
2. Provide a proper `.info` sidecar with category `Developer resources and samples` so it groups alongside Framerate + Meshdebug

**Acceptance:**

- [ ] AscentMFD either hidden or properly categorized with description

---

## P2-B9 — [macOS][medium] XRSound plugin has no `.info` sidecar → appears in Miscellaneous

**Labels:** `platform:macos`, `scope:ui`, `priority:medium`

**Body:**

`libXRSound.dylib` (built at `out/build/macos-arm64-debug/Modules/Plugin/libXRSound.dylib`) has no accompanying `.info` file. It appears in the Launchpad Modules tab under "Miscellaneous" with no description. Users who want vessel audio have no indication this is the module to enable.

**Fix:** add `orbiter_module_info(XRSound_dll CATEGORY "Audio" DESCRIPTION "OpenAL-backed audio engine for vessel sounds (engine, docking, atmospheric). Enable for in-sim audio.")` in `Sound/XRSound/src/CMakeLists.txt`.

**Acceptance:**

- [ ] XRSound appears under its own category (e.g. "Audio") with a clear description

---

## P2-B10 — [macOS][medium] Video tab Width/Height fields show garbage values

**Labels:** `platform:macos`, `scope:ui`, `priority:medium`

**Body:**

Launchpad Video tab shows `Width: 20310460` and `Height: 0` below the Resolution dropdown. Both are uninitialized-memory artefacts. The Resolution dropdown itself works correctly (shows "2560 x 1664" for retina).

**Root cause hypothesis:** `OGLLaunchpad::RenderTabVideo`'s `ImGui::InputInt("Width", ...)` / `ImGui::InputInt("Height", ...)` is bound to variables that are never seeded from the selected dropdown value, so they display whatever was on the stack at first draw.

**Acceptance:**

- [ ] Width/Height mirror the currently-selected Resolution entry
- [ ] Manual edits to Width/Height update the dropdown selection (or disable the fields when Resolution is set)

---

## P2-B11 — [macOS][low] Video tab missing MSAA multisample selector

**Labels:** `platform:macos`, `scope:ui`, `priority:low`

**Body:**

Win32 Video tab exposes a multisample antialiasing combo (0x / 2x / 4x / 8x). The macOS Video tab does not. Unclear if:

1. By-design — macOS Metal backend handles MSAA automatically via the swapchain
2. Regression in M22.c Video tab port

**Acceptance:**

- [ ] Decide between (1) and (2); if (1), document in roadmap M22.c follow-up; if (2), add the combo

---

## P2-B13 — [macOS][medium] Planet texture appears chunky / blocky on Earth

**Labels:** `platform:macos`, `scope:rendering`, `priority:medium`

**Body:**

Demo/Today scenario renders Earth with visibly stepped continent edges (large blocky pixels). Two contributing factors:

1. Default build does not fetch the Blue Marble LOD8 tile pyramid (opt-in via `-DORBITER_FETCH_EARTH_BLUEMARBLE`, roadmap M27.b)
2. Red channel of planet textures is effectively 0 (see #25), collapsing the visual dynamic range and making the remaining green-channel detail look more aliased

Once #25 is fixed, this may become less objectionable. Re-check after red-channel fix before investigating M7 tile LOD selection.

**Acceptance:**

- [ ] After #25 fix, evaluate planet texture quality; if still chunky, investigate M7 tile priority / LOD selection

---

## P2-B14 — [macOS][high] F1 cockpit view renders empty (only stars, no 2D panel, no VC mesh)

**Labels:** `platform:macos`, `scope:rendering`, `priority:high`

**Body:**

From Demo/Today scenario (focus vessel GL-01 / DeltaGlider), pressing F1 to enter cockpit view shows only the starfield — no 2D panel (M16), no virtual-cockpit mesh (M15). DeltaGlider has full panel + VC assets so content should be visible.

**Repro:**
1. Launch Demo/Today
2. Press F1 → camera mode changes to Cockpit (HUD confirms)
3. Scene renders only stars, rest of frame is sceneFBO clear color

**Hypothesis:** similar root cause to the M11 post-process bug resolved by PR #24 — Apple Silicon OpenGL may be silently dropping the cockpit pass writes, OR the dual-pass cockpit visibility filter (M15.b) isn't selecting the correct MESHVIS_* flags.

**Acceptance:**

- [ ] F1 cockpit view shows a DeltaGlider 2D panel (or VC mesh after Alt+F1/M15 dual-pass) with recognisable instruments
- [ ] Panel + VC render in Docked at ISS and Atlantis Ascent scenarios too

---

## P2-B15 — [macOS][low] DlgInfo default selection is Sol instead of focus vessel

**Labels:** `platform:macos`, `scope:dialogs`, `priority:low`

**Body:**

Ctrl+I in-sim opens DlgInfo. The dialog's default object selection is "Sol" regardless of the scenario's `BEGIN_FOCUS` vessel. User has to manually switch to GL-01 via the combo. The combo switching works correctly.

**Acceptance:**

- [ ] Initial selection matches `oapiGetFocusObject()` or the scenario's focus entry
- [ ] Combo still allows switching to any body / vessel as before

---

## P2-B16 — [macOS][medium] DlgMap opens but planetary map surface is empty

**Labels:** `platform:macos`, `scope:rendering`, `priority:medium`

**Body:**

Ctrl+M opens DlgMap dialog with the map area empty — no coastlines, no terminator, no vessel marker. The dialog frame, title and controls are present.

**Hypothesis:** the map is drawn via Sketchpad (M17) into a surface texture displayed in the dialog. If Sketchpad rendering on macOS suffers the same FBO-format issue that affected M11 post-process, the destination surface stays zero-alpha and the viewer sees nothing.

**Acceptance:**

- [ ] Open DlgMap on Demo/Today → coastlines + grid + vessel markers visible
- [ ] Scene behind the map may still show the #25 red-channel issue but the map itself should have content

---

## P2-B17 — [macOS][low] Shift+F1 MFD mode selector unresponsive

**Labels:** `platform:macos`, `scope:input`, `priority:low`

**Body:**

Pressing Shift+F1 in cockpit view (F1) produces no response. Expected: a mode-selection popup listing MFD modes (Orbit, Surface, Map, Align, HSI, Docking, Launch, Terminal, Landing, COM/NAV + plugin-registered).

**Unclear root cause** (depends on P2-B14):

1. Cockpit rendering is broken so the MFD panel area isn't active / receiving the shortcut
2. Keymap binding for Shift+F1 missing on macOS
3. No MFD plugins active in Modules tab so the selector has only built-ins but still doesn't show

**Acceptance:**

- [ ] After P2-B14 resolution, verify Shift+F1 opens MFD mode list
- [ ] If still broken, investigate Keymap dispatch for Shift+F-key combinations

---

## P2-B18 — [macOS][medium] Ctrl+F1 DlgCamera doesn't open

**Labels:** `platform:macos`, `scope:dialogs`, `scope:input`, `priority:medium`

**Body:**

Pressing Ctrl+F1 in simulation does nothing. Expected: DlgCamera dialog opens with camera-mode controls.

**Acceptance:**

- [ ] Verify `Keymap.cpp` has an entry for Ctrl+F1 → DlgCamera dispatch on macOS
- [ ] Pressing Ctrl+F1 opens the camera dialog

---

## P2-B19 — [macOS][medium] Ctrl+F2 DlgTacc dialog opens but is frozen

**Labels:** `platform:macos`, `scope:dialogs`, `priority:medium`

**Body:**

Pressing Ctrl+F2 opens the time-acceleration dialog, but:
- The 1x / 10x / 100x / 1000x buttons do nothing when clicked
- The dialog cannot be dismissed (ESC / X button both no-op)

The user is effectively stuck with the dialog visible until forcing Orbiter to quit.

**Acceptance:**

- [ ] Buttons trigger the corresponding time-acceleration change
- [ ] ESC or the dialog's X button closes the window

---

## P2-B20 — [macOS][medium] Alt+F1 DlgHelp doesn't fire (no browser open)

**Labels:** `platform:macos`, `scope:dialogs`, `scope:input`, `priority:medium`

**Body:**

Pressing Alt+F1 (Option+F1 on macOS) in simulation does nothing. Expected per roadmap M23.a: either a help dialog opens, or the default browser is launched to the project URL via the cross-platform URL fallback shipped in M23.

**Acceptance:**

- [ ] Verify Keymap binding for Alt+F1 on macOS
- [ ] Pressing Alt+F1 either opens a help dialog or launches the default browser to the documented URL

---

## P2-B21 — [macOS][cosmetic] Missing MenuInfoBar/*.png icons for Notes and LuaConsole

**Labels:** `platform:macos`, `scope:packaging`, `priority:low`

**Body:**

In-sim log shows warnings:

```
Cannot find menu item texture: MenuInfoBar/Notes.png
Cannot find menu item texture: MenuInfoBar/LuaConsole.png
```

The menu items still render (presumably with a default fallback), but the intended icons are missing from the install tree. Likely other plugin icon .png files are similarly absent.

**Acceptance:**

- [ ] Audit `MenuInfoBar/` expected png list vs. what the install pipeline actually copies
- [ ] Install the missing icons or provide fallback glyphs

---

## P2-B22 — [macOS][high] XRSound initialises without crash but no audio events heard

**Labels:** `platform:macos`, `scope:audio`, `priority:high`

**Body:**

After PR #27 (InitModule dispatch fixed), XRSound can finally activate from the Launchpad Modules tab. The init phase completes cleanly: OpenAL device opens, the `XRSound/Default/*.wav` pack loads, no crash.

However running Demo/Atlantis Ascent AP (which should fire engine-burn and RCS sounds during the autopilot ascent) produces no audible output. Running Demo/Today (static scene) also quiet, as expected.

**Hypotheses:**

1. Vessel event path not actually invoking `XRSoundEngine::Play*` at the right moments
2. OpenAL device opens but the buffer chain isn't routed to the system output
3. `XRSound-Atlantis.cfg` parsing fails or the cfg points at sound ids that don't exist
4. Depends on P2-B14 cockpit/vessel state being fully initialised (unlikely but possible)

**Repro:**
1. Launchpad → Modules → enable XRSound
2. Launch Demo/Atlantis Ascent AP
3. Expected: engine burn rumble, RCS crackle; observed: silence

**Acceptance:**

- [ ] Demo/Atlantis Ascent AP produces audible engine / RCS sounds
- [ ] Demo/Docked at ISS plays docking clanger when appropriate
- [ ] Demo/DG ISS Approach has audible environmental / vessel sounds

---

# Come aprirle

Stessa procedura di `QA_PHASE1_ISSUES.md`:

```sh
python3 scripts/open_qa_issues.py QA_PHASE2_ISSUES.md  # equivalente a Phase 1 batch
```

Oppure manualmente con `gh issue create --title "..." --label "a,b,c" --body-file p2bN.md`.

Each issue body is self-contained above — copy the section between `## P2-BN` headers into a body file and feed to `gh issue create`.
