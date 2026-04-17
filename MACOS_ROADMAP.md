# Orbiter macOS ARM64 — Roadmap per la parità 100% con la versione Windows

> Documento operativo persistente. Mantenuto nel repo per essere ripreso in sessioni future.
> Stato di partenza del porting: vedi `MACOS_PORT_STATUS.md`.
> Branch corrente: `claude/thirsty-greider-f3579d` (worktree).
> Data stesura: 2026-04-17.

---

## Context

Orbiter è storicamente Windows-only (DirectX 9, DirectInput, irrKlang). Il port a macOS ARM64 è già funzionante al ~50%: eseguibile Mach-O ARM64 + 66 moduli `.dylib` compilano, runtime stabile, OGLClient (OpenGL 4.1 + SDL2 + ImGui) sostituisce D3D9Client, input/mesh/planet/HUD/ring/exhaust renderizzano.

Il deliverable richiesto è **parità 100% identica a Windows** in grafica, audio e UX, con **zero stubs/TODO** residui nel prodotto finale. Questa roadmap organizza in milestone concrete il lavoro residuo, primariamente:

- Porting shader HLSL→GLSL mancanti (~6000 righe da tradurre: Scatter, PBR, clouds, shadow, IBL, post-FX completi).
- Virtual Cockpit (disabilitato per crash F1 nel commit `673013a4`).
- Audio XRSound (`ORBITER_BUILD_XRSOUND=OFF` forzato, irrKlang incompatibile macOS).
- Launchpad/Dialoghi/MFD feature parity (~10k righe Win32→ImGui).
- Distribuzione (`.app` bundle firmato+notarized, CI/CD macOS).

---

## Decisioni architetturali

| Decisione | Scelta | Motivo |
|---|---|---|
| Backend grafico | OpenGL 4.1 Core Profile | Metal richiederebbe riscrittura OGLClient (14k LOC) + shader + pipeline state. Metal è phase post-parità opzionale. |
| Conversione shader | Manuale HLSL→GLSL per feature | spirv-cross/HLSLcc non parsano D3D9 FX framework legacy. Porting feature-by-feature con UBO moderni e sampler espliciti. |
| Backend audio | Compat layer `IAudioBackend` → `OpenALBackend` | Mantiene codice XRSound, sostituisce solo backend irrKlang. OpenAL Soft via Homebrew. |
| `CString` (MFC) | Wrapper `XRString` compat 100 righe | Evita reimplementare 5000 righe XRSound. |
| Test parità visiva | Reference screenshot/video pubblici + checklist manuale | User non ha build Windows. SSIM opzionale. |
| Config persistence | `~/Library/Application Support/Orbiter/` + `~/Library/Logs/Orbiter/` | macOS-native, niente registry. |
| Earth texture | Post-install download NASA Blue Marble LOD 1–8 (~500MB CC0) | Bundle .app resta leggero. |

Effort totale stimato: **~189 giorni-sviluppo senior singolo**. Critical path sequenziale: **75gg**. Con 2 dev in parallelo: ~100gg calendario; 3 dev: ~75gg.

---

## Fase A — Fondamenta grafiche (14gg)

### M0. Shader sourcing + hot-reload
- **Obiettivo:** `OGLShaderMgr` → gestore completo: preprocessor `#include` GLSL custom, uniform block introspection, hot-reload in debug via filesystem watch, UBO binding centralizzato.
- **File:** `OVP/OGLClient/OGLShaderMgr.cpp/.h` (161→~500), `OVP/OGLClient/shaders/common.glsl` (6→~200), nuova dir `OVP/OGLClient/shaders/include/*.glsl.inc`.
- **Verifica:** `touch shaders/vessel.frag` → reload senza crash. Log al primo frame mostra shader + UBO.
- **Dipendenze:** nessuna. **Bloccante** per M8–M17.
- **Rischio:** GLSL 410 no-native `#include` → parser regex in `PreprocessSource()`.
- **Effort:** 3gg.

### M1. Framebuffer + render target API completa
- **Obiettivo:** FBO colori 32-bit + depth/stencil, MSAA 4x/8x, mipmap auto-gen on-unbind, FBO pool, `clbkCreateSurfaceEx` con flag `OAPISURFACE_RENDERTARGET|MIPMAPS|TEXTURE|ALPHA`. Risolvere TODO `OGLClient.cpp:456`.
- **File:** `OVP/OGLClient/OGLSurface.cpp` (175→~400), `.h`, `OVP/OGLClient/OGLClient.cpp` (451–511).
- **Verifica:** RT 1024x1024 RGBA8, PNG ogni 60 frame. `clbkBlt(nullptr, ...)` disegna su backbuffer.
- **Dipendenze:** nessuna. **Bloccante** per M15 (VC) e M17 (MFD).
- **Rischio:** state leak `GL_DRAW/READ_FRAMEBUFFER` → RAII `FBOBinder`.
- **Effort:** 4gg.

### M2. Mesh GPU cache + invalidation
- **Obiettivo:** Risolvere TODO `OGLClient.cpp:700`. `MeshRegistry` singleton con callback `clbkRegisterMeshUser`/`clbkUnregisterMeshUser`/`clbkDeleteMesh`. Supporto `MESHGROUP_FX` dinamico, LOD, dirty flag.
- **File:** nuovo `OVP/OGLClient/OGLMeshRegistry.cpp/.h`. `OGLClient.cpp` (690–720). `OGLvVessel.cpp` (17–93 migrare static cache).
- **Verifica:** DeltaGlider gear up/down: no re-upload totale, log "cache hit: 132, miss: 4" dopo 60s.
- **Dipendenze:** M0.
- **Rischio:** thread safety module main thread ↔ render thread → mutex try_lock + dirty queue.
- **Effort:** 3gg.

### M3. Material system unificato
- **Obiettivo:** UBO `UBOMaterial` condiviso: diffuse/ambient/specular/emissive/reflective, fresnel power, metalness, roughness, map flags. Adeguare vessel, vessel_pbr, panel2d, particle. Risolvere stubs `OGLClient.cpp:847/853`.
- **File:** `OVP/OGLClient/OGLvVessel.cpp` (142–298). Shaders `vessel.*`, `vessel_pbr.*`. Nuovo `shaders/include/material.glsl.inc`. `OGLClient.cpp:847/853`.
- **Verifica:** ShuttlePB/DeltaGlider/Atlantis colori indistinguibili da reference Windows (SSIM>0.96 @ 1920×1080).
- **Dipendenze:** M0, M2.
- **Rischio:** D3DMATERIAL7 pre-PBR → mapping euristico identico a Windows `MaterialMgr`.
- **Effort:** 4gg.

---

## Fase B — Grafica: shader feature parity (61gg)

### M4. Atmosphere Rayleigh + Mie scattering
- **Obiettivo:** Port `OVP/D3D9Client/shaders/Scatter.hlsl` (771 righe) → GLSL: Rayleigh, Mie, sun disc, horizon halo, density integration, scale height, limb darkening, night-side bleed. Integrazione in `OGLvPlanet` per Earth/Mars/Venus/Titan.
- **File:** nuovi `shaders/scatter.frag/.vert`, `shaders/include/scatter_common.glsl.inc`. `OGLvPlanet.cpp` (1–420), `OGLAtmosphere.cpp` espandere. Rimuovere `haze.frag/vert` (fusi).
- **Verifica:** Scenario "Earth orbit sunrise": terminator con gradient arancione-rosso, orizzonte blu+halo, zodiacale night-side.
- **Dipendenze:** M0, M1, M3.
- **Rischio:** instabilità numerica angolo critico → LUT 256×64 precomputed `Textures/atmo_lut.dds`.
- **Effort:** 10gg. **(Blocco shader singolo più grande.)**

### M5. Cloud layers
- **Obiettivo:** Earth `Clouds_NN.dds` con `clouds.frag/vert`: sample cilindrico, rotazione differenziale, shadow projection, alpha Fresnel horizon, self-shadowing volumetrico leggero.
- **File:** nuovi `shaders/clouds.frag/.vert`, `cloud_shadow.frag`. `OGLvPlanet.cpp` RenderClouds pass. Reference `OVP/D3D9Client/CloudMgr.cpp` + `Cloudmgr2.cpp`.
- **Verifica:** Scenario "Demo\Earth View": clouds visibili, ombre su terreno, scroll differenziato F9.
- **Dipendenze:** M4.
- **Rischio:** Z-fighting superficie/cloud 8km → depth offset + log Z.
- **Effort:** 5gg.

### M6. Night city lights
- **Obiettivo:** `Earth_Night.dds` (hi-res data pack) con blend moltiplicativo sul lato ombra. Trigger `dot(N, sunDir) < 0`.
- **File:** `shaders/planet.frag` + night sampler. `OGLvPlanet.cpp` bind texture. `OGLTile.cpp` (1–485) LOD carica night variant.
- **Verifica:** Terminator Earth con città visibili. Venere/Luna senza city lights.
- **Dipendenze:** M4.
- **Rischio:** texture absent non-Earth → skip pass.
- **Effort:** 2gg.

### M7. Planet tile LOD completa
- **Obiettivo:** Parità con `Tilemgr2.cpp` (1826) + `Surfmgr2.cpp` + `ZTreeMgr.cpp`: quad-tree 18 livelli, threaded loading, elevation Z-offset mesh, archive `.ztree`, horizon culling.
- **File:** `OGLTile.cpp` (485→~1200), `.h`. Nuovo `OGLTileLoader.cpp` (thread pool). `OGLvPlanet.cpp` integration.
- **Verifica:** Atlantis landing KSC: transitions smooth, load <200ms, no crack LOD, elevation da `.elv`.
- **Dipendenze:** M4, M5.
- **Rischio:** thread contention con M2 → lockfree queue; `.ztree` parser byte-per-byte match Windows.
- **Effort:** 12gg.

### M8. PBR vessel completo
- **Obiettivo:** `vessel_pbr.frag` (147) → parità `PBR.fx` (518) + `Metalness.fx` (393): TBN preciso, IBL irradiance+specular, anisotropic, clearcoat (DeltaGlider canopy).
- **File:** shaders `vessel_pbr.*` (→~400). Nuovi `shaders/include/brdf.glsl.inc`, `ibl.glsl.inc`. `OGLvVessel.cpp` (142+) upload tangenti VBO, bind env map. Reference `PBR.fx`, `Metalness.fx`.
- **Verifica:** DeltaGlider glass/metal/painted. Orbit Demo: Earth reflections canopy. Apollo CM, Atlantis materials corretti.
- **Dipendenze:** M3, M0.
- **Rischio:** NTVERTEX legacy senza tangents → calc per-triangolo al load (+30% memory) o derivative approx in FS.
- **Effort:** 7gg.

### M9. IBL environment maps
- **Obiettivo:** Cubemap 32×32 diffuse irradiance + specular prefilter 256×256 con 5 mip pre-convoluted. Capture da celsphere+ambient, rebuild ogni 4 frames. Reference `IrradianceInteg.hlsl`, `EnvMapBlur.hlsl`.
- **File:** nuovi `shaders/env_capture.*`, `env_irradiance.frag`, `env_prefilter.frag`. Nuovo `OGLEnvMap.cpp/.h`. `OGLScene.cpp` pre-vessel.
- **Verifica:** DeltaGlider canopy reflections = scene ambient. Debug cubemap `env_debug.dds` 6 faces coerenti.
- **Dipendenze:** M8, M1.
- **Rischio:** costo GPU → rebuild 1/4 frames.
- **Effort:** 5gg.

### M10. Shadow mapping
- **Obiettivo:** `shadow.glsl` (22 stub) → directional cascaded 2 cascate (50m/2km), PCF 3×3, depth bias. Solo vessel lit side.
- **File:** `shaders/shadow.glsl`. Nuovi `shaders/shadow_cast.vert/.frag`. Integrare in `vessel.frag`/`vessel_pbr.frag`. Nuovo `OGLShadowMap.cpp/.h`.
- **Verifica:** Atlantis sulla pista: ombre su runway. Debug view Ctrl+F11.
- **Dipendenze:** M8.
- **Rischio:** Peter Panning/acne → constant+slope bias tuning.
- **Effort:** 4gg.

### M11. Post-processing HDR (bloom, tonemap, lens flare)
- **Obiettivo:** Parità `LightBlur.hlsl` (181), `LensFlare.hlsl` (199), `Glare.hlsl` (248), `SceneTech.fx`. HDR float 16-bit, 2-pass Gaussian 9-tap, sprite ghosts + chromatic aberration, filmic ACES.
- **File:** shaders `bloom_*.frag/.vert`, `lensflare.frag`, `tonemap.frag`. `OGLPostProcess.cpp` espandere. `OGLScene.cpp` pipeline. `OGLClient.cpp` render-to-HDR-texture.
- **Verifica:** Sole da orbita Earth: lens flares, bloom limbo, tonemap no clip highlights.
- **Dipendenze:** M1, M3.
- **Rischio:** perf HDR 16F su Apple Silicon integrated ~40% framerate → config low-spec skip HDR.
- **Effort:** 5gg.

### M12. Particle systems completo
- **Obiettivo:** Parità `Particle.fx` (112): contrail decay, size-over-life, alpha curve, wind drift, smoke dissipation. `OGLParticle.cpp` conformità `EXHAUSTSPEC`/`CONTRAILSPEC`.
- **File:** shaders `particle.*`, `exhaust.*` (glow inner/outer). `OGLParticle.cpp`.
- **Verifica:** DeltaGlider take-off KSC: contrail 5+s, exhaust glow bicolore, smoke dissipation.
- **Dipendenze:** M0, M3. **Rischio:** bassi. **Effort:** 3gg.

### M13. Glare/corona sole e stelle
- **Obiettivo:** `Glare.hlsl` (248) + `BeaconArray.fx`: corona procedurale sole/stelle brillanti, chromatic halo, star twinkle.
- **File:** nuovi `shaders/glare.*`, `corona.*`. `OGLCelSphere.cpp` (161) integrare glare pass. `shaders/beacon.*` estendere.
- **Verifica:** Vista Sole con corona soft 3° radius. Sirio/Canopus con twinkle.
- **Dipendenze:** M11. **Rischio:** bassi. **Effort:** 3gg.

### M14. Runway lights, annotations, labels, planetarium grid
- **Obiettivo:** Runway approach KSC, beacon arrays ISS, annotation overlay 3D, planetarium grid+costellazioni. Reference `RunwayLights.cpp`, `BeaconArray.cpp`, `TileLabel.cpp`.
- **File:** `OGLBeaconArray.cpp`, `OGLAnnotation.cpp` espandere. Nuovi `OGLRunwayLights.cpp`, `OGLTileLabel.cpp`.
- **Verifica:** Atlantis approach KSC notturno: runway edges+PAPI. F9 planetarium grid+costellazioni. ISS beacon nav lights.
- **Dipendenze:** M1, M3. **Rischio:** bassi. **Effort:** 5gg.

---

## Fase C — Virtual Cockpit e cockpit 2D (37gg)

### M15. Virtual Cockpit rendering pipeline
- **Obiettivo:** Risolvere crash F1 (commit `673013a4`): probabile `clbkCreateSurfaceEx` con flag RGB565 16-bit non gestito da `OGLSurface`. Implementare `VirtualCockpit::DefineArea` → surface creata → `clbkVCRedrawEvent` sketchpad → compositing aree mesh VC. Riabilitare F1 macOS.
- **File:** nuovo `OVP/OGLClient/OGLvVirtualCockpit.cpp/.h`. `OGLSurface.cpp` fix format 16-bit→RGBA8888. `OGLvVessel.cpp` render VC quando `g_camera->IsInternal()`. `Src/Orbiter/Orbiter.cpp` (2739–2748) riabilitare F1. `Pane.cpp:629` invocare VC. `VCockpit.cpp` rimuovere `#ifdef`.
- **Verifica:** DeltaGlider F1: cockpit VC visibile, look-around mouse-drag, area click no-crash, HUD VC renderizzato.
- **Dipendenze:** M1, M2, M8.
- **Rischio:** **ALTO.** Multipli code path morti. Partire da Atlantis (mesh VC semplice). Scenario test dedicato.
- **Effort:** 14gg.

### M16. 2D Panel cockpit (panel2D)
- **Obiettivo:** `RegisterPanelBackground`+`RegisterPanelArea`+`TriggerPanelRedrawArea` → blit sub-regioni + panel2d shader compose fullscreen. Test DeltaGlider→ShuttleA→Atlantis STS.
- **File:** `OGLClient.cpp` (517–600) `clbkRender2DPanel` completare `MESHGROUPEX`, additive, alpha, clipping. `shaders/panel2d.*` additive+chroma key. `Panel2D.cpp` (565), `Panel.cpp` (446) rimuovere `#ifdef _WIN32`.
- **Verifica:** DeltaGlider Shift+F1: glass cockpit, MFD 1/2 black, HUD overlay, mouse su pulsanti. Atlantis STS 2D panel con MFD.
- **Dipendenze:** M1, M15.
- **Rischio:** chroma-key D3D9 → shader discard oppure pre-process texture at load.
- **Effort:** 8gg.

### M17. MFD Sketchpad API copertura 100%
- **Obiettivo:** `oapi::Sketchpad` ~40 metodi; OGL oggi wrappa `ImDrawList` minimale. Parità `D3D9Pad.cpp` (2097) + `D3D9Pad2/3` (1334): polyline curva, Bezier, filled polygon, text alignment avanzato, metrics hit test, state save/restore, transform stack, clip rect, bitmap blit alpha stretch, GDI text layout.
- **File:** nuovo `OVP/OGLClient/OGLSketchpad.cpp/.h` (~1500). `Orbitersdk/include/DrawAPI.h` non modificare. Reference `D3D9Pad*.cpp`.
- **Verifica:** 15+ MFD integrati + plugin (TransX, LuaMFD/ScriptMFD) disegnano identici a Windows: Orbit (curva+markers), Map (coastline+terminator), Align Planes, HSI, VOR/VTOL, Surface (altimetria), Attitude Ref, Docking (T-cross+crosshair).
- **Dipendenze:** M1, M16.
- **Rischio:** **ALTO.** API granulare stateful, test matrix combinatoriale. Iterare MFD-per-MFD con visual diff.
- **Effort:** 15gg.

---

## Fase D — Audio XRSound su OpenAL (18gg)

### M18. IAudioBackend + OpenALBackend
- **Obiettivo:** Interfaccia `IAudioBackend`: `play2D(file, loop)`, `play3D(pos)`, `setVolume`, `setPlaybackSpeed`, `setSoundVolume`, `removeAllSoundSources`, `getDriverName`. `IrrklangBackend` (Windows) + `OpenALBackend` (macOS/Linux, OpenAL Soft + AL_POSITION/AL_PITCH).
- **File:** nuovo `Sound/XRSound/src/IAudioBackend.h`. `OpenALBackend.cpp/.h` esistenti espandere + decoders (`dr_wav.h`, `stb_vorbis.c`, `dr_mp3.h`). Nuovo `IrrklangBackend.cpp/.h`. Nuovo `AudioBackendFactory.cpp` `#ifdef` selection.
- **Verifica:** Unit test WAV 16-bit mono → plays, duration via `AL_BUFFERS_PROCESSED`. `otool -L` mostra `libopenal.1.dylib`.
- **Dipendenze:** nessuna.
- **Rischio:** licensing: WAV/OGG OK, MP3 via `dr_mp3.h` public domain.
- **Effort:** 7gg.

### M19. CString compat + irrKlang API shim
- **Obiettivo:** `XRString` wrapper 100 righe con `.Format`, `.GetLength`, `.GetAt`, cast `const char*`. Replace `pSoundEngine->play2D` → `pBackend->Play2D`.
- **File:** nuovo `Sound/XRSound/src/XRString.h`. Sostituzioni: `DefaultSoundGroupPreSteps.cpp`, `ModuleXRSoundEngine.cpp`, `SoundPreSteps.cpp`, `VesselXRSoundEngine.cpp` (838), `XRSoundConfigFileParser.cpp`, `XRSoundDLL.cpp`, `XRSoundImpl.cpp` (~40 CString totali).
- **Verifica:** `Sound/XRSound/CMakeLists.txt` compila `.dylib`. `libXRSound.dylib` in `Modules/`.
- **Dipendenze:** M18.
- **Rischio:** `CString::Format` buffer/encoding diversi → unit test wrapper.
- **Effort:** 6gg.

### M20. Integrazione XRSound in Orbiter core
- **Obiettivo:** Rimuovere `set(ORBITER_BUILD_XRSOUND OFF ... FORCE)` da `CMakeLists.txt:55`. Orbiter carica XRSound via dlopen. Vessel modules usano `GetXRSoundEngineInstance(hVessel)`. `clbkSimulationPreStep` per listener 3D.
- **File:** `CMakeLists.txt:55`. `Sound/CMakeLists.txt` abilitare subdirectory macOS. `Sound/XRSound/CMakeLists.txt` link OpenAL + backend. `Src/Orbiter/CMakeLists.txt` link XRSound plugin.
- **Verifica:** Log "XRSound: OpenAL Soft driver loaded". DeltaGlider main engine throttle: pitch varia con thrust. Docking clanger. Altair music loop.
- **Dipendenze:** M18, M19.
- **Rischio:** listener positioning astronomical-scale → relative a nearest body frame.
- **Effort:** 4gg.

### M21. Audio default sounds pack
- **Obiettivo:** Verificare 40+ WAV default XRSound `Sound/XRSound/Default/*.wav` licenza + install in bundle `.app`.
- **File:** `Sound/XRSound/CMakeLists.txt` (48–61). `cmake/CPackOptions.cmake.in`.
- **Verifica:** `Orbiter.app/Contents/Resources/XRSound/Default/` con tutti WAV. Playback confermato.
- **Dipendenze:** M20.
- **Rischio:** licensing → SPDX verify.
- **Effort:** 1gg.

---

## Fase E — Launchpad, Dialogs, Plugins, Distribuzione (59gg)

### M22. Launchpad ImGui completo (6 tab)
- **Obiettivo:** `OGLLaunchpad.cpp` (243) → ~3250 righe parità Windows: `Launchpad.cpp` (617) + TabScenario (735), TabExtra (1666), TabModule (419), TabOptions (79), TabVideo (260), TabAbout (93).
- **File:** nuovi `OGLTabScenario.cpp/.h`, `OGLTabExtra.*`, `OGLTabModule.*`, `OGLTabVideo.*`, `OGLTabAbout.*`, `OGLTabOptions.*`. `OGLLaunchpad.cpp` orchestrator. Thumbnail loading per scenario folder. Module chooser con description parsing.
- **Verifica:** Launchpad 6 tab navigabili, scenario list con thumbnail, Video tab risoluzione+fullscreen toggle, Extra tab plugin list+description.
- **Dipendenze:** M1.
- **Rischio:** HTML description parsing Win HtmlHelp → regex parser o plain description.txt.
- **Effort:** 10gg.

### M23. Dialogs core (F3–F10)
- **Obiettivo:** `Src/Orbiter/Dlg*.cpp` (5528 totali) usano `DialogWin.cpp` Win32. Equivalenti ImGui.
- **File:** `DlgMgr.cpp` orchestrator. Nuovi `DlgInfoImgui.cpp`, `DlgMapImgui.cpp`, `DlgCameraImgui.cpp`, `DlgHelpImgui.cpp`, `DlgFocusImgui.cpp`, `DlgRecorderImgui.cpp`, `DlgTaccImgui.cpp`, `DlgCaptureImgui.cpp`, `DlgFunctionImgui.cpp`, `DlgOptionsImgui.cpp`, `DlgMenuCfgImgui.cpp`.
- **Verifica:** Tutti F3–F10 aprono dialog funzionalità identica Windows. `DlgMap.cpp` (454) renderizza mappa 2D planetaria corretta.
- **Dipendenze:** M1, M17.
- **Rischio:** custom Win32 controls `DlgCtrl.cpp` → porting 1:1 ImGui widgets.
- **Effort:** 12gg.

### M24. WindowMgr dialog system (plugin gcGUI)
- **Obiettivo:** `OVP/D3D9Client/WindowMgr.cpp` (1825) sistema dialoghi render window per plugin (DebugControls, AtmoControls). OGL equivalente ImGui con stesso `gcGUI` API.
- **File:** nuovo `OVP/OGLClient/OGLWindowMgr.cpp/.h` (~1500). `Orbitersdk/include/gcGUI.h` API stabile.
- **Verifica:** Plugin DebugControls (oggi skipped macOS) apre finestra ImGui con controls.
- **Dipendenze:** M1, M23. **Rischio:** bassi. **Effort:** 6gg.

### M25. Win32 plugins (ScnEditor, TrackIR, Atlantis config, Rcontrol, Meshdebug)
- **Obiettivo:** ScnEditor Win32 → ImGui. TrackIR: exclude build macOS + messaggio Launchpad. Rcontrol: input via SDL events. Atlantis MFC payload → ImGui. Meshdebug: UI port.
- **File:** `Src/Plugin/ScnEditor/ScnEditor.cpp/.h`, `Editor.cpp/.h`. `Src/Plugin/TrackIR/CMakeLists.txt` exclude macOS. `Src/Plugin/Rcontrol/Rcontrol.cpp`. `Src/Vessel/Atlantis/AtlantisConfig/AtlantisConfig.cpp`. `Src/Plugin/Meshdebug/Meshdebug.cpp`.
- **Verifica:** ScnEditor modifica pos/vel ShuttlePB live. TrackIR assente no-crash. Atlantis config payload selectable.
- **Dipendenze:** M23, M24.
- **Rischio:** ScnEditor IMFD conversion → prioritizzare feature usate.
- **Effort:** 10gg.

### M26. Keyboard map editor, joystick calibration, config persistence
- **Obiettivo:** Keyboard map 2D colorato + joystick axis calibration wizard con deadzone. Config in `~/Library/Application Support/Orbiter/Orbiter.cfg`. Log in `~/Library/Logs/Orbiter/Orbiter.log`.
- **File:** nuovo `OGLKeymapEditor.cpp/.h`. Nuovo `OGLJoystickCalibration.cpp/.h`. `Src/Orbiter/Config.cpp` defaults macOS. `Log.cpp` log path. `Orbiter.cpp` (552–553) rimuovere `HKEY`/`RegOpenKeyEx` Wine detection → env var.
- **Verifica:** Keyboard editor US-ANSI binding modificabile. Joystick calibration: move axes, deadzone visibile.
- **Dipendenze:** M22. **Rischio:** bassi. **Effort:** 5gg.

### M27. Missing assets (fonts, Earth texture installer)
- **Obiettivo:** `fa-solid-900.ttf`, `Lekton-Bold.ttf`, `architext.regular.ttf` + Earth.tex hi-res base (~500MB NASA Blue Marble LOD 1–8).
- **File:** nuovo `cmake/macos_assets.cmake` (download+SHA256+install). Nuovo `cmake/download_earth_lod8.py` (NASA Blue Marble CC0). `DlgMgr.cpp` rimuovere font fallback generic. Nuovo `Assets/macos/` con fonts (Lekton SIL OFL, FontAwesome SIL OFL, Liberation fallback).
- **Verifica:** Post-install: Earth renderizza con texture base grado 8 (continenti). ImGui mostra glyphs FontAwesome.
- **Dipendenze:** M4, M7.
- **Rischio:** licensing TTF → SPDX verify, preferire SIL OFL/GPL.
- **Effort:** 3gg.

### M28. CI/CD macOS workflows
- **Obiettivo:** GitHub Actions macOS: on-push, PR build, release (`.app` → `.dmg`), code signing (optional CI, required distribution).
- **File:** nuovi `.github/workflows/on-push-macos.yml`, `pr-build-macos.yml`. `release.yml` aggiungere macOS job. `reusable-build.yml` runner `macos-14` ARM64. Nuovo `cmake/codesign.cmake`.
- **Verifica:** PR: CI runs macos-arm64-debug+release, smoke test `test_simple` screenshot match. Release `Orbiter-macos-arm64.dmg` firmato+notarized.
- **Dipendenze:** nessuna tecnica (ma utile post-M0 per gating continuo).
- **Rischio:** Apple notarization richiede Apple ID + app-specific password → fallback "unsigned" con Gatekeeper override documentato.
- **Effort:** 4gg.

### M29. Force feedback/haptic (SDL_Haptic)
- **Obiettivo:** SDL_Haptic native: rumble on landing gear touchdown, vibrazione atmospheric buffeting.
- **File:** `SDLPlatform.cpp` (514) aggiungere SDL_Haptic init+effect. `Input.cpp` bridge. Nuovo `HapticFX.cpp`.
- **Verifica:** Joystick FF (CH Fighterstick, Thrustmaster): vibrazione on landing. Senza FF: no-op silent.
- **Dipendenze:** M20. **Rischio:** bassi. **Effort:** 3gg.

### M30. Test harness parità visiva
- **Obiettivo:** Framework test: (a) Orbiter headless scenario predefinito, (b) cattura framebuffer frame N, (c) PNG con metadata, (d) confronto reference public screenshot/video + SSIM opzionale + checklist manuale, (e) PASS/FAIL + diff.
- **File:** nuova dir `Tests/rendering_parity/`. `CMakeLists.txt`. 20 scenari `scenarios/*.scn`. Reference `baselines/public/*.png` (YouTube/forum ufficiali, diritti uso interno-test, no redistribuzione). `run_parity.py` con `scikit-image` SSIM. Integration `.github/workflows/pr-build-macos.yml` test gate.
- **Verifica:** `ctest -R rendering_parity`: 20 test, SSIM + checklist (sunset aranciato, clouds, VC, MFD, PBR metal). Threshold PASS: SSIM>0.92 non-atmo, >0.85 atmo.
- **Dipendenze:** M4–M17 complete.
- **Rischio:** reference public fidelity limitata → checklist manuale cura per scenario.
- **Effort:** 6gg.

---

## Critical Path

```
M0 ─┬─ M1 ─┬─ M2 ─ M3 ─ M8 ─ M9 ─ M10 ─ M11 ─ M12
    │      ├─ M4 ─ M5 ─ M6 ─ M7
    │      ├─ M13 ─ M14
    │      └─ M15 ─ M16 ─ M17 ─ M23 ─ M24 ─ M25 ─ M26
    │
    ├─ M18 ─ M19 ─ M20 ─ M21
    ├─ M22
    └─ M28 (parallel)

M27 ← M4, M7    |    M29 ← M20 (parallel)    |    M30 ← M4–M17
```

Critical path sequenziale: **M0→M1→M4→M7→M15→M16→M17→M23→M25→M30 ≈ 75gg**.

Effort totale: **A(14) + B(61) + C(37) + D(18) + E(59) = 189gg** single-dev.

---

## Critical Files (riferimento rapido)

| Area | File principali |
|---|---|
| OGLClient core | `OVP/OGLClient/OGLClient.cpp`, `OGLShaderMgr.cpp`, `OGLSurface.cpp`, `OGLScene.cpp` |
| OGL vessel | `OVP/OGLClient/OGLvVessel.cpp`, shaders `vessel*.{frag,vert}` |
| OGL planet | `OVP/OGLClient/OGLvPlanet.cpp`, `OGLTile.cpp`, `OGLAtmosphere.cpp`, shaders `planet.*`, `scatter.*` (nuovo), `clouds.*` (nuovo) |
| Post-FX | `OVP/OGLClient/OGLPostProcess.cpp`, shaders `bloom_*`, `tonemap.*`, `lensflare.*` |
| VC + panel | `OVP/OGLClient/OGLvVirtualCockpit.*` (nuovo), `OGLSketchpad.*` (nuovo), `Src/Orbiter/VCockpit.cpp`, `Panel2D.cpp` |
| Audio | `Sound/XRSound/src/OpenALBackend.cpp`, `IAudioBackend.h` (nuovo), `XRString.h` (nuovo) |
| Launchpad/Dialogs | `OVP/OGLClient/OGLLaunchpad.cpp`, `Src/Orbiter/DlgMgr.cpp`, `Dlg*Imgui.cpp` (nuovi) |
| Reference D3D9 | `OVP/D3D9Client/shaders/*.{hlsl,fx}`, `D3D9Pad*.cpp`, `Tilemgr2.cpp`, `Surfmgr2.cpp`, `MaterialMgr.cpp`, `WindowMgr.cpp` |
| Platform | `Orbitersdk/include/OrbiterPlatform.h`, `Src/Orbiter/SDLPlatform.*`, `platform_stubs_posix.cpp` |
| Build | `CMakeLists.txt:55` (XRSound FORCE off), `CMakePresets.json`, `cmake/*` |

---

## Criteri di uscita "Parità 100%"

1. `grep -rE 'TODO|FIXME|XXX|STUB|stub' OVP/OGLClient/ Src/Orbiter/ Sound/XRSound/src/` = 0 risultati (esclusi `Extern/`, `stb_image.h`).
2. `ORBITER_BUILD_XRSOUND=ON` default su macOS senza override.
3. F1 abilitato su macOS, 0 crash in 20 scenari standard (VC DeltaGlider/Atlantis/ShuttleA).
4. Launchpad 6 tab funzionali (scenari+thumbnails, moduli, video, extra, about, options).
5. Rendering parity (M30): ≥95% test passa threshold SSIM, checklist visiva 100% verde.
6. Distribuzione: `Orbiter-macOS-ARM64.dmg` firmata+notarized, install on-click, scenari in `~/Documents/Orbiter Scenarios/`.
7. Documentazione: `README_MACOS.md` con 0 menzioni "not supported / not yet".
8. GitHub Issues: 0 open `platform:macos` critical/high.

---

## Come continuare in sessioni future

Ogni milestone comincia con:

1. `git checkout -b feature/Mxx-short-name` dal branch corrente.
2. Lettura `MACOS_PORT_STATUS.md` (stato storico) + questo file (roadmap).
3. Lettura dei file target della milestone (path elencati sopra).
4. Baseline verde: `cmake --preset macos-arm64-debug && cmake --build out/build/macos-arm64-debug --parallel 8`.
5. Implementazione seguendo **Obiettivo / File / Verifica** della milestone.
6. Test con scenario dedicato (vedi **Verifica** di ciascun Mxx).
7. PR con diff visivo (before/after screenshot obbligatorio per M4–M17).
8. Merge squashato `Mxx: <obiettivo>`, aggiornare questo file marcando Mxx done.

**Milestone ad alto rischio** (M4 scattering, M7 tiles, M15 VC, M17 MFD): branch esplorativo su singolo sotto-task per validare approach prima di committarsi all'intera milestone.

---

## Stato avanzamento

| # | Milestone | Fase | Stato | PR / Commit |
|---|---|---|---|---|
| M0 | Shader sourcing + hot-reload | A | ✅ | this branch |
| M1 | Framebuffer + RT API | A | ✅ | this branch |
| M2 | Mesh GPU cache | A | ✅ | this branch |
| M3 | Material system UBO | A | ☐ | — |
| M4 | Rayleigh+Mie scattering | B | ☐ | — |
| M5 | Cloud layers | B | ☐ | — |
| M6 | Night city lights | B | ☐ | — |
| M7 | Planet tile LOD | B | ☐ | — |
| M8 | PBR vessel | B | ☐ | — |
| M9 | IBL environment maps | B | ☐ | — |
| M10 | Shadow mapping | B | ☐ | — |
| M11 | Post-processing HDR | B | ☐ | — |
| M12 | Particle systems | B | ☐ | — |
| M13 | Glare/corona | B | ☐ | — |
| M14 | Runway lights+annotations | B | ☐ | — |
| M15 | Virtual Cockpit | C | ☐ | — |
| M16 | 2D Panel | C | ☐ | — |
| M17 | MFD Sketchpad 100% | C | ☐ | — |
| M18 | IAudioBackend + OpenAL | D | ☐ | — |
| M19 | CString + irrKlang shim | D | ☐ | — |
| M20 | XRSound core integration | D | ☐ | — |
| M21 | Default sounds pack | D | ☐ | — |
| M22 | Launchpad 6 tab | E | ☐ | — |
| M23 | Dialogs core F3–F10 | E | ☐ | — |
| M24 | WindowMgr gcGUI | E | ☐ | — |
| M25 | Win32 plugins port | E | ☐ | — |
| M26 | Keymap/joystick/config | E | ☐ | — |
| M27 | Missing assets | E | ☐ | — |
| M28 | CI/CD macOS | E | ☐ | — |
| M29 | Force feedback | E | ☐ | — |
| M30 | Parity test harness | E | ☐ | — |
