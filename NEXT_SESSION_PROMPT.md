# Prompt per la prossima sessione Claude — Orbiter macOS port issue work

Copia-incolla questo come messaggio iniziale in una nuova sessione. Include tutto il contesto necessario.

---

Stai lavorando al porting macOS ARM64 di **Orbiter Space Flight Simulator** (repo `kanik0/orbiter`, fork di `orbitersim/orbiter`). Il porting è in un stato "foundation solida ma rendering runtime incompleto" dopo due round di QA (Phase 1 audit + Phase 2 interattivo) conclusi il 2026-04-18.

## Contesto repo

- **Main branch**: `main` — include tutti i fix Phase 1 + Phase 2 mergiati (PR #23, #24, #27, #28, #46)
- **Working dir suggerito**: `/Users/massimobedini/Documents/source/orbiter/.claude/worktrees/peaceful-mcnulty-df980a/` (worktree dedicato, già configurato, branch tipico `claude/peaceful-mcnulty-df980a` che traccia main)
- **Build dir**: `out/build/macos-arm64-debug/` (preset `macos-arm64-debug` con `-DORBITER_BUILD_XRSOUND=ON -DORBITER_MAKE_TESTS=ON`)
- **Build comando**: `cmake --build out/build/macos-arm64-debug --parallel 8`
- **Host test**: Darwin 25.4.0, Apple M2, OpenGL 4.1-via-Metal
- **Logs runtime**: `~/Library/Logs/Orbiter/Orbiter.log` (contiene NUL bytes, filtra con `tr -d '\000'`)
- **Config utente**: `~/Library/Application Support/Orbiter/Orbiter.cfg`
- **Scenarios Demo**: `out/build/macos-arm64-debug/Scenarios/Demo/*.scn` (14 scenari)

## Documenti QA di riferimento

Sul branch `main`:

- `MACOS_ROADMAP.md` — roadmap originale M0-M30 ✅, con sezioni "Follow-up Fase X" di note residue
- `MACOS_PORT_STATUS.md` — status storico
- `QA_PHASE1_REPORT.md` — audit statico + smoke + DMG + follow-up (con errata corrige post-Phase 2 che documenta che l'originario "14/14 smoke PASS" era falso positivo)
- `QA_PHASE1_ISSUES.md` — body delle 15 issue Phase 1 (#8-#22)
- `QA_PHASE2_LOG.md` — log interattivo step-by-step con bug tracker P2-B1→P2-B23
- `QA_PHASE2_ISSUES.md` — body delle 17 issue Phase 2 (#29-#45)
- `QA_PHASE2_SUMMARY.md` — verdict table, fix summary, priority roadmap

Leggi questi prima di iniziare, in particolare `QA_PHASE2_SUMMARY.md` che sintetizza tutto.

## Stato del simulatore in 1 riga

**Build + Launchpad + save/load + DMG install OK; cockpit view vuota, MFD inaccessibili, map vuoto, 3 shortcut dead, colori planet rotti, audio senza eventi → NON è utilizzabile come simulatore finché la rendering runtime + input + audio non sono completate.**

Tracked come **issue #47** (umbrella "tracking / release-blocker"). Tutti i blocker pendono da quella.

## Issue aperte (22 totali)

### 🔴 Release-blocker group — rendering runtime (6 issue)
- **[#37](https://github.com/kanik0/orbiter/issues/37)** [high] F1 cockpit view empty — no 2D panel M16, no VC mesh M15. **Probabilmente il singolo fix più sbloccante**: risolve anche #40 Shift+F1 MFD, abilita test plugin MFD (#17/#18/#19/... SKIPPED in Phase 2)
- **[#25](https://github.com/kanik0/orbiter/issues/25)** [high] Planet red channel missing — Earth nera, continenti verdi, spazio blu. Indagine candidate: texplanet.frag swizzle, DXT1/BC1 decoder Apple Silicon, OGLvPlanet material uniform binding
- **[#36](https://github.com/kanik0/orbiter/issues/36)** [medium] Planet texture chunky — può risolversi dopo #25
- **[#26](https://github.com/kanik0/orbiter/issues/26)** [medium] M4 scatter invisible — potrebbe risolversi dopo #25
- **[#39](https://github.com/kanik0/orbiter/issues/39)** [medium] DlgMap empty — M17 Sketchpad suspect, stesso pattern FBO/format bug di PR #24
- **[#40](https://github.com/kanik0/orbiter/issues/40)** [low] Shift+F1 MFD selector — dipende da #37

### 🔴 Release-blocker group — audio (1)
- **[#45](https://github.com/kanik0/orbiter/issues/45)** [high] XRSound init OK ma no audio events — vessel event routing o OpenAL buffer routing

### 🔴 Release-blocker group — input (3)
- **[#41](https://github.com/kanik0/orbiter/issues/41)** [medium] Ctrl+F1 DlgCamera no response — Keymap.cpp macOS entry missing
- **[#42](https://github.com/kanik0/orbiter/issues/42)** [medium] Ctrl+F2 DlgTacc stuck — dialog close handler broken
- **[#43](https://github.com/kanik0/orbiter/issues/43)** [medium] Alt+F1 DlgHelp no response — keymap or URL-open path

### 🟡 Polish pre-release (9)
- **[#34](https://github.com/kanik0/orbiter/issues/34)** [medium] Video tab Width/Height garbage values
- **[#33](https://github.com/kanik0/orbiter/issues/33)** [medium] XRSound plugin no `.info` sidecar
- **[#30](https://github.com/kanik0/orbiter/issues/30)** [low] Launch button no disabled state
- **[#32](https://github.com/kanik0/orbiter/issues/32)** [low] AscentMFD sample exposed in user Modules
- **[#35](https://github.com/kanik0/orbiter/issues/35)** [low] Video tab no MSAA selector
- **[#38](https://github.com/kanik0/orbiter/issues/38)** [low] DlgInfo default Sol instead of focus vessel
- **[#29](https://github.com/kanik0/orbiter/issues/29)** [cosmetic] em-dash glyph missing font
- **[#31](https://github.com/kanik0/orbiter/issues/31)** [cosmetic] Tree-expand glyph `?`
- **[#44](https://github.com/kanik0/orbiter/issues/44)** [cosmetic] Missing MenuInfoBar icons

### 🟡 Phase 1 residuals (still open, lower priority)
- **[#8](https://github.com/kanik0/orbiter/issues/8)** [high] 8 Celbody Win32 PE32 symlinks (Ariel/Deimos/Miranda/Oberon/Phobos/Titania/Triton/Umbriel) — no macOS build
- **[#22](https://github.com/kanik0/orbiter/issues/22)** [high] LuaInline uses Win32 threading — SIGSEGV when loaded, currently workaround is to not load it at all
- **[#9](https://github.com/kanik0/orbiter/issues/9)** [medium] DlgOptions in-sim manca DrawVisual + DrawPhysics
- **[#11](https://github.com/kanik0/orbiter/issues/11)** [medium] `OpenFileIgnoreCase` esteso solo a vessel.cfg
- **[#13](https://github.com/kanik0/orbiter/issues/13)** [medium] ctest Lua.Interpreter rpath broken
- **[#14](https://github.com/kanik0/orbiter/issues/14)** [medium] rendering_parity 0-byte baselines
- **[#18](https://github.com/kanik0/orbiter/issues/18)** [medium] OGLSketchpad blit copymode subset

Altri low / cosmetic: #10, #12, #15, #16, #17, #19, #20, #21, #47 (umbrella).

## Ordine di attacco raccomandato

1. **Start da #37 (F1 cockpit empty)**. È il fix che sblocca più cose (MFD, VC, panel, then #40 cascades). Pattern probabile: simile a PR #24 (FBO/shader regression), ispezionare `OGLvVessel::Render` pass cockpit + `OGLSketchpad` FBO config per panel2d rendering. Se il bug è in Sketchpad, risolve automaticamente anche #39.

2. Poi **#25 red channel**. Test diagnostico: modifica temporaneamente `shaders/texplanet.frag` per output solo `.r`, `.g`, `.b` singolarmente e vedere cosa appare. Probabile che red sia effettivamente 0 dalla texture, non dal shader. Eventuale fix: check DXT1 decoder in `OGLTexture::LoadDDS` su Apple Silicon, o albedo texture path.

3. Dopo #25+#37, **ri-test #26 / #36 / #39 / #40** — potrebbero risolversi automaticamente.

4. **#45 XRSound audio events** — indagine separata. Probabili step: verifica che `VesselXRSoundEngine::clbkPreStep` venga invocato (logga chiamate), che `SoundPreSteps` generi eventi (engine ignite/throttle), che OpenAL device abbia buffer attivo. `xrsound_openal_smoke` ctest passa già → OpenAL funziona a basso livello.

5. **#41/#42/#43 keymap + dialog close** — ispeziona `Src/Orbiter/Keymap.cpp` per dispatch macOS di Ctrl+F1, Ctrl+F2, Alt+F1. Per #42 specificamente controlla `DlgTacc::OnDraw` handlers.

6. Polish UI (#34, #30, #32, #33, #29, #31, #44, #35, #38) — low priority, fare in batch alla fine.

7. Phase 1 residuals quando tempo permette, in particolare **#22 LuaInline** che richiede porting std::thread (più grosso).

## Come lavorare su una issue

Template di workflow:

1. `cd /Users/massimobedini/Documents/source/orbiter/.claude/worktrees/peaceful-mcnulty-df980a`
2. `git checkout main && git pull`
3. `git checkout -b fix/<issue-number>-<short-name>`
4. Investiga: `grep -rn` / `Read` dei file candidati, istrumenta con `fprintf(stderr, "[DEBUG] ...")` se serve
5. Build + test: `cmake --build out/build/macos-arm64-debug --parallel 8`
6. Smoke test rendering (se rilevante): `./Orbiter --scenario="Demo/Today" --capture-frame=60 --capture-out=/tmp/test.png` — visualizza PNG con `Read` tool
7. Smoke test interactive: chiedi all'utente di lanciare `./Orbiter` e riportare a parole / screenshot cosa vede
8. Commit + push + `gh pr create` + `gh pr merge <n> --merge`
9. Chiudi issue con commento che linka la PR (GitHub lo fa automaticamente se usi "Fixes #NN" nel PR body)

## Conosciute regolarità macOS-specifiche

Dal codice del porting + bug trovati:

- **RGBA16F / R11F_G11F_B10F silently broken su Apple Silicon GL 4.1-via-Metal**: usa GL_RGBA8 fallback + boost esposizione nello shader. Già fatto in M11 post-process (PR #24), da ripetere per altri FBO se incontri scene-nera pattern.
- **RTLD_LOCAL su plugin**: i simboli del plugin non sono visibili via RTLD_DEFAULT. Usa `dladdr` + `dlopen(RTLD_NOLOAD)` per handle self (fix PR #27).
- **Path separator `\\`**: cerca `\\\\` in stringhe di codice cross-platform e gate con `#ifdef _WIN32`. Già fixato in Celbody.cpp (PR #23) e Orbiter.cpp:Quicksave (PR #28) — controllane altri.
- **POSIX vs Win32 file search**: `OpenFileIgnoreCase` in `Util.cpp` è tuo amico per fs lookup case-insensitive.
- **NUL bytes nel log**: usa `tr -d '\000' < ~/Library/Logs/Orbiter/Orbiter.log` per grep decente. Bug separato (#20).
- **Visual verification via Read tool**: il Read tool su .png mostra immagini, non serve screenshot esterni per debug render. Chiedi invece screenshot all'utente per verify interactive.

## Deliverable atteso di fine sessione

- Almeno N issue chiuse via PR (N > 3 se possibile in una sessione)
- Un aggiornamento di `QA_PHASE2_SUMMARY.md` o un nuovo documento `PROGRESS_<data>.md` che annota cosa è stato fatto
- PR titles format: `macos: <short description>` (coerente con convention esistente)
- PR body con Fixes #NN link
- Target: poter affermare "il simulatore è ora utilizzabile" = umbrella #47 pronto a essere chiuso

---

**Inizia ora** leggendo i documenti QA + proponi con quale issue vuoi partire. L'utente confermerà il target e da lì parti. Mantieni lo stesso stile di collaborazione interattiva delle sessioni QA (step-by-step, screenshot quando servono, commit + PR frequenti).
