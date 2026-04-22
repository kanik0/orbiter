![Orbiter logo](./Src/Orbiter/Bitmaps/banner.png)

# Orbiter Space Flight Simulator — macOS / Apple Silicon port

> **Experimental, entirely vibe-coded port of [Orbiter](https://github.com/orbitersim/orbiter)
> to macOS on Apple Silicon.** Built by iterating with an LLM coding assistant on top
> of the original Windows/DirectX codebase: the Win32 platform layer was replaced with
> SDL2, the DirectX graphics client was rewritten as an OpenGL 4.1 client (OGLClient),
> and the ~66 vessel/planet/plugin modules were ported to `.dylib`. Treat this as a
> research-grade build, not a supported product — it runs, it flies, but there will
> be rough edges.

The upstream, authoritative Orbiter project for Windows lives at
[orbitersim/orbiter](https://github.com/orbitersim/orbiter). This fork exists only
to track the macOS port; Windows users should grab releases from upstream.

---

## About Orbiter

Orbiter is a spaceflight simulator based on Newtonian mechanics. Its playground is
our solar system with many of its major bodies – the sun, planets and moons. You
take control of a spacecraft – either historic, hypothetical, or purely science
fiction. There are no predefined missions to complete (except the ones you set
yourself), no aliens to destroy and no goods to trade. Instead, you will get a
pretty good idea about what is involved in real space flight – how to plan an
ascent into orbit, how to rendezvous with a space station, or how to fly to
another planet.

## License

Orbiter is published as an Open Source project under the MIT License (see
[LICENSE](./LICENSE)). The D3D9Client graphics engine (Windows-only, unused on
macOS) is licensed under LGPL, see [LGPL](./OVP/D3D9Client/LGPL.txt).

---

## Install (macOS, pre-built)

1. Grab the latest `Orbiter-macos-arm64.dmg` from the
   [Releases page](https://github.com/kanik0/orbiter/releases).
2. Open the DMG and drag `Orbiter.app` to `/Applications`.
3. Because the build is not notarized, the first launch will require:
   ```bash
   xattr -dr com.apple.quarantine /Applications/Orbiter.app
   ```
4. Double-click to launch.

The `.app` bundle is self-contained — scenarios, textures, meshes, fonts, gravity
models, Lua scripts, XRSound audio pack, HTML help and PDF documentation all ship
inside `Contents/Resources/`.

## Build from source (macOS)

The macOS port uses SDL2 and OpenGL 4.1 (via Metal) instead of DirectX / Win32.

### Prerequisites

```bash
brew install sdl2 ninja cmake
```

### Configure and build

```bash
cmake --preset macos-arm64-debug
cmake --build out/build/macos-arm64-debug --parallel 8
```

A post-build step creates the `Modules/Celbody` symlinks and copies fonts.

### Run from the build tree

```bash
cd out/build/macos-arm64-debug
./Orbiter -s "Delta-glider in LEO" -x
```

### Create a distributable `.app` / `.dmg`

```bash
# Self-contained .app bundle with all data directories copied in
cmake --build out/build/macos-arm64-debug --target macos-bundle-distributable

# Ad-hoc codesigned, zipped into a UDZO .dmg
cmake --build out/build/macos-arm64-debug --target macos-dmg
```

Set `APPLE_CODESIGN_IDENTITY` / `APPLE_NOTARIZATION_KEYCHAIN_PROFILE` in the
environment to produce a properly signed + notarized DMG (see
[cmake/codesign.cmake](cmake/codesign.cmake)).

## Build from source (Windows)

This fork does not distribute Windows builds. Go to
[orbitersim/orbiter](https://github.com/orbitersim/orbiter) for the upstream
Windows build, docs and community forum.

---

## Planet textures

The repository does not include the full planetary texture pack — those are a
separate, large download. The `Textures/` directory shipped with the release
DMG contains the base set needed to launch every built-in scenario. If you
want high-resolution Earth / Moon / Mars tiles, follow the upstream instructions
on [orbiter-forum.com](https://www.orbiter-forum.com) and point Orbiter at the
external texture directory via `Orbiter.cfg` (`PlanetTexDir`).

## Help

- In-game help: `Alt+F1` inside the simulator.
- `Doc/Orbiter User Manual.pdf` in the app bundle is the primary reference.
- Community forum: [orbiter-forum.com](https://www.orbiter-forum.com).
