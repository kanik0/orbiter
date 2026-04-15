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

### Vessel Mesh Pipeline (implemented but not yet visible)
- **Mesh loading**: ShuttlePB.msh loaded (7 groups)
- **GPU caching**: NTVERTEX data + indices uploaded to VAO/VBO/EBO
- **Vessel shader**: GLSL with position/normal/UV, material diffuse+emissive
- **Issue**: Camera Z convention mismatch prevents vessel visibility (see below)

---

## Known Issues

### Critical: Camera Z Convention (blocks vessel visibility)
**File**: `OVP/OGLClient/OGLClient.cpp` lines ~370-395

Orbiter's camera looks along **+Z** (D3D convention).
OpenGL looks along **-Z**.

The current projection matrix uses OpenGL's -Z convention which works
for **planet rendering** (distance normalization places planets at coordinates
that happen to work with -Z). But **vessel rendering** places vessels at +Z
in camera space, which the -Z projection clips.

**Attempted fixes**: 
- Negating Z row in view matrix: broke planets
- Using Orbiter's +Z projection: needs `glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE)` from GL 4.5 (not available in GL 4.1)
- Adapting +Z projection for GL NDC [-1,1]: broke planets

**Proper fix approaches**:
1. Negate Z in BOTH the planet model matrices AND view matrix consistently
2. Use a two-pass approach: planet pass with current -Z, vessel pass with +Z
3. Upgrade to GL 4.5+ for glClipControl
4. Pre-multiply all camera-relative coordinates by a Z-flip before entering the rendering pipeline

### Module Loading: Symlinks Required
After clean rebuild, `Modules/Celbody/` symlinks to `Modules/lib*.dylib` must
be manually recreated. The CMake install doesn't create them automatically.

### Vessel Module Loading: GetMeshTemplate returns null
ShuttlePB's module loads but `vessel->GetMeshTemplate(0)` returns null.
Workaround in place: `oapiLoadMeshGlobal(className)` as fallback.

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

### 1. Fix Camera Z Convention (HIGH - blocks vessel visibility)
The entire rendering pipeline needs a consistent coordinate convention.
Best approach: in `clbkRenderScene`, after computing camera-relative coordinates
for ALL objects (planets and vessels), negate Z before passing to OpenGL.
This means: in planet model matrix, use `nrz = -nrz`; in vessel model matrix,
use `nvz = -nvz`. The view matrix stays as-is (transpose of camRot).
The standard OpenGL -Z projection stays as-is.

### 2. Keyboard Input (HIGH - for interactive use)  
The SDL keyboard mapping is implemented but needs testing.
Users need Ctrl+arrow for camera rotation, T for time warp, etc.
File: `Src/Orbiter/SDLPlatform.cpp` + `Src/Orbiter/Orbiter.cpp::UserInput()`

### 3. HUD / Text Rendering (MEDIUM)
Implement `clbkCreateFont` using stb_truetype or FreeType.
Implement `clbkGetSketchpad` for 2D line/text rendering.
This enables the flight HUD, speed indicator, altitude display.

### 4. Texture .tex Container (MEDIUM)
Parse `.tex` files (concatenated DDS) for planet surface textures.
This enables proper Earth/Moon/Mars surface rendering instead of flat colors.

### 5. Module Symlink Automation (LOW)
Add CMake post-build step to create Modules/Celbody symlinks automatically.

### 6. PNG Texture Loading (LOW)
Add stb_image or similar for PNG support (MenuInfoBar icons).

### 7. Audio (LOW)
OpenAL or SDL_mixer backend for sound effects.

### 8. macOS .app Bundle (LOW)
Info.plist, icon, proper distribution packaging.
