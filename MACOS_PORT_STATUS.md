# Orbiter macOS Apple Silicon Port - Status Document

## Branch: `claude/crazy-mccarthy` (worktree)
## Date: 2026-04-15
## 19 commits on top of `main`

---

## What Works

### Compilation (100%)
- **Orbiter executable**: Mach-O 64-bit ARM64, ~9.3 MB
- **66 .dylib modules**: ALL vessels, planets, plugins compile and link
- **Build system**: `cmake --preset macos-arm64-debug` + Ninja
- **Dependencies**: SDL2, OpenGL 4.1, ImGui, Lua 5.1, zlib (system), Tracy

### Runtime
- **Simulation loop**: Stable 30+ seconds, no crashes
- **Scenario loading**: "test_simple" with ShuttlePB in Earth orbit
- **Gravity models**: All 6 loaded (egm96, jgl165, jgmro, jgmess, mod_shgj, JGDWN)
- **Celbody modules**: Vsop87, Moon (ELP82), Earth/Mars/Venus atmospheres
- **DDS texture loading**: DXT1/DXT3/DXT5 with mipmaps (129+ textures loaded)
- **BMP texture loading**: 8/24/32-bit
- **Logging**: Orbiter.log functional
- **SDL input**: Full keyboard mapping (100+ keys SDL→DirectInput)
- **Mouse**: Click events + wheel zoom
- **ImGui**: SDL2+OpenGL3 backends, dialog rendering

### Rendering (OGLClient)
- **Stars**: 4000 procedural points with GLSL shaders
- **Earth**: Visible as blue sphere with solar illumination + day/night terminator
- **All planets**: Rendered as flat-color spheres with correct positions and lighting
- **Distance normalization**: Prevents float precision loss at astronomical distances
- **Camera tracking**: View-projection from Orbiter's camera rotation matrix

### Vessel Mesh Pipeline (WORKING)
- **Mesh loading**: ShuttlePB.msh loaded (7 groups)
- **GPU caching**: NTVERTEX data + indices uploaded to VAO/VBO/EBO
- **Vessel shader**: GLSL with position/normal/UV, material diffuse+emissive+texture
- **Rendering**: ShuttlePB visible as gray mesh with solar lighting at screen center

---

## Known Issues

### RESOLVED: Camera Z Convention
**Fixed in**: `OVP/OGLClient/OGLClient.cpp` - view matrix construction

The root cause was twofold:
1. **Wrong view matrix encoding**: `oapiCameraRotationMatrix` returns GRot (local→global).
   The D3D9Client stores it directly because D3D uses row-vector multiplication (v*M)
   which implicitly transposes. OpenGL uses column-vector multiplication (M*v), so we
   need GRot^T. The old code stored GRot's columns as OpenGL columns (= GRot), but
   the fix stores GRot's rows as OpenGL columns (= GRot^T).
2. **Z convention flip**: Orbiter looks along +Z, OpenGL expects -Z. The third row of
   the transposed view matrix is negated: V = flipZ * GRot^T.

Additionally, `GetMeshTemplate()` returns null on macOS, and the fallback mesh loading
was only attempted once (frame 1) due to a `static bool` guard. Fixed by caching
fallback mesh handles per vessel class name.

### Module Loading: Symlinks Required
After clean rebuild, `Modules/Celbody/` symlinks to `Modules/lib*.dylib` must
be manually recreated. The CMake install doesn't create them automatically.

### Vessel Module Loading: GetMeshTemplate returns null
ShuttlePB's module loads but `vessel->GetMeshTemplate(0)` returns null.
Workaround: `oapiLoadMeshGlobal(className)` as fallback, cached per class name.

### Font Loading: Missing font files
`fa-solid-900.ttf`, `Lekton-Bold.ttf`, `architext.regular.ttf` are not shipped.
Font loading now falls back to ImGui default font when files are missing
(`Src/Orbiter/DlgMgr.cpp`).

### Screenshot Capture
Debug code in OGLClient saves frame 15 as `screenshot.bmp` (2560x1600 BMP).
This should be removed or made optional for production.

---

## Architecture

### New Files Created
```
Orbitersdk/include/OrbiterPlatform.h     - Platform abstraction (600+ typedefs/stubs)
Orbitersdk/include/resource_stub.h       - Win32 resource/dialog stubs
Orbitersdk/include/gcCoreAPI.h           - D3D9Client API stub
Src/Orbiter/d3d_compat.h                 - D3D7 type definitions
Src/Orbiter/SDLPlatform.h/cpp            - SDL2 window/event/input layer
Src/Orbiter/platform_stubs_posix.cpp     - Stubs for Win32-only classes
Src/Orbiter/DlgCtrl_stub.cpp             - DlgCtrl Win32 custom controls stub
OVP/OGLClient/OGLClient.h/cpp            - OpenGL 4.1 Graphics Client
OVP/OGLClient/OGLTexture.h/cpp           - BMP/DDS texture loader
OVP/OGLClient/CMakeLists.txt             - Build for OGLClient
Utils/meshc/d3d_stub.h                   - D3D types for mesh compiler
```

### Key Modifications
```
CMakeLists.txt                - macOS detection, SDL2, Clang flags, rpath
CMakePresets.json             - macos-arm64-debug/release presets
Src/Orbiter/CMakeLists.txt    - Non-Windows source selection, OGLClient linkage
Src/Orbiter/Orbiter.cpp       - POSIX main(), SDL2 Run() loop, null guards
Src/Orbiter/Config.cpp        - Path normalization (\→/), POSIX defaults
Src/Orbiter/Vessel.cpp        - .dylib paths, null stream guard
Src/Orbiter/Celbody.cpp       - .dylib paths, gravity model paths
Src/Orbiter/DlgMgr.cpp        - #ifdef _WIN32 for ImGui_ImplWin32
Orbitersdk/include/OrbiterAPI.h    - Platform-conditional includes
Orbitersdk/include/GraphicsAPI.h   - Remove windows.h dependency
Orbitersdk/include/DrawAPI.h       - ARM NEON for SIMD
~50 header files               - windows.h → OrbiterPlatform.h guards
~25 vessel/plugin headers      - Forward declarations for friend class pattern
```

### Rendering Pipeline (OGLClient::clbkRenderScene)
1. Clear framebuffer (dark blue)
2. Build VP matrix from `oapiCameraRotationMatrix` + projection
3. Render starfield (4000 GL_POINTS, no view translation)
4. Render planets (distance-normalized spheres, flat color or texture)
5. Render vessels (mesh groups from NTVERTEX data) ← Z convention issue
6. Render 2D overlay (ImGui dialogs via Render2DOverlay)
7. Screenshot capture (debug, frame 15)

---

## How to Build

```bash
# Prerequisites
brew install sdl2 ninja cmake

# Configure
cmake --preset macos-arm64-debug

# Build
cmake --build out/build/macos-arm64-debug --parallel 8

# After clean build: create module symlinks
cd out/build/macos-arm64-debug
mkdir -p Modules/Celbody Fonts
for f in Modules/lib*.dylib; do ln -sf "../$(basename $f)" "Modules/Celbody/"; done
cp _deps/imgui-src/misc/fonts/Roboto-Medium.ttf Fonts/
cp _deps/imgui-src/misc/fonts/Cousine-Regular.ttf Fonts/

# Create test scenario
cat > Scenarios/test_simple.scn << 'EOF'
BEGIN_ENVIRONMENT
  System Sol
  Date MJD 52170.5
END_ENVIRONMENT
BEGIN_FOCUS
  Ship GL-01
END_FOCUS
BEGIN_CAMERA
  TARGET GL-01
  MODE Extern
  POS 10.00 30.00 0.01
  TRACKMODE TargetRelative
  FOV 50.00
END_CAMERA
BEGIN_SHIPS
GL-01:ShuttlePB
  STATUS Orbiting Earth
  RPOS 6731000 0 0
  RVEL 0 7791 0
  AROT 0 0 0
END
END_SHIPS
EOF

# Run
./Orbiter -s "test_simple" -x
```

---

## Next Steps (Priority Order)

### 1. ~~Fix Camera Z Convention~~ DONE
View matrix fixed: V = flipZ * GRot^T. Fallback mesh caching fixed.
Vessels now render correctly alongside planets and stars.

### 2. ~~Keyboard Input~~ DONE (buffered events added)
SDL keyboard mapping was already in place (95+ key mappings). The missing piece
was **buffered key event generation**: the macOS path only had immediate (continuous)
key processing but not buffered (edge-triggered) events. Time warp (T/R), MFD
controls, and all single-press actions now work via key-state-transition detection.
Files: `Src/Orbiter/Orbiter.cpp::UserInput()`, `Orbiter.h`, `platform_stubs_posix.cpp`

### 3. ~~HUD / Text Rendering~~ DONE (ImGui-backed Sketchpad)
Implemented OGLFont (wraps ImFont), OGLPen, OGLBrush, and OGLSketchpad using
ImGui's `ImDrawList` API. Supports: Text(), Line(), MoveTo/LineTo(), Rectangle(),
Ellipse(), Polygon(), Polyline(), SetTextAlign(), SetTextColor(), font metrics.
The HUD renders green text/line overlays for speed, altitude, heading indicators.
Screenshot capture moved to `clbkDisplayFrame()` to include ImGui overlay.

### 4. ~~Texture .tex Container~~ DONE
Parse `.tex` files implemented — scans for `'DDS '` markers, extracts first DDS texture.
Mercury, Venus, Saturn, Uranus, Neptune, Titan, and all moons now render with textures.
Earth is flat-color only (no Earth.tex in base distribution — needs hi-res data pack).

### 5. ~~PNG Texture Loading~~ DONE
Added stb_image.h (from Tracy dependency) for PNG support.
All MenuInfoBar icons (ship, camera, speed, etc.) now load successfully.

### 6. ~~Module Symlink Automation~~ DONE
CMake post-build step creates Modules/Celbody symlinks and copies ImGui fonts.

### 7. Audio (LOW)
OpenAL or SDL_mixer backend for sound effects.

### 8. ~~macOS .app Bundle~~ DONE
`cmake --build ... --target macos-bundle` creates `Orbiter.app` with proper
Info.plist, executable in MacOS/, data symlinks in Resources/. Startup chdir
resolves to Resources/ when in .app or to exe dir when running standalone.

---

## What's Missing for Production

### Working Now
- 3D scene: stars (4000), planets (textured from .tex/DDS), vessel meshes (ShuttlePB)
- Camera: correct view matrix (flipZ * GRot^T), mouse wheel zoom, external tracking
- HUD: green text/line overlay via ImGui-backed Sketchpad
- Keyboard: immediate + buffered events (T/R time warp, Ctrl+arrow camera, RCS)
- Textures: DDS, BMP, .tex container, PNG all loading
- Font/Pen/Brush/Sketchpad: full 2D drawing via ImDrawList
- ImGui dialogs: rendering with SDL2+OpenGL3 backends

### Remaining Gaps (all visual/polish — core sim is functional)
1. **Cloud layers**: Earth clouds overlay
2. **Atmospheric scattering**: Sunset/haze effects
3. **Exhaust particles**: Thruster flame visualization
4. **Planetary rings**: Saturn/Uranus rings
5. **Night city lights**: Texture overlay on dark side
6. **Audio**: Sound effects via OpenAL or SDL_mixer
