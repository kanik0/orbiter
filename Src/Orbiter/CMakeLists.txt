# Copyright (c) Martin Schweiger
# Licensed under the MIT License

if(MSVC)
	# SAFESEH linker flag must be turned off because the DX7 libraries don't support it'
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -SAFESEH:NO")
endif()
FetchContent_MakeAvailable(imgui)
FetchContent_MakeAvailable(implot)

# Sources for all Orbiter executable targets - cross-platform core
set(common_src
# General source files
	Astro.cpp
	Camera.cpp
	cmdline.cpp
	Config.cpp
	Element.cpp
	elevmgr.cpp
	Keymap.cpp
	LightEmitter.cpp
	Mesh.cpp
	Nav.cpp
	Orbiter.cpp
	PlaybackEd.cpp
	Psys.cpp
	Script.cpp
	Shadow.cpp
	State.cpp
	Vecmat.cpp
	VectorMap.cpp
	TimeData.cpp
# Dialogs (ImGui-based, cross-platform)
	DlgCamera.cpp
	DlgCapture.cpp
	DlgFocus.cpp
	DlgFunction.cpp
	DlgHelp.cpp
	DlgInfo.cpp
	DlgMap.cpp
	DlgMenuCfg.cpp
	DlgMgr.cpp
	DlgOptions.cpp
	DlgRecorder.cpp
	DlgTacc.cpp
	Select.cpp
	OptionsPages.cpp
# Body classes
	Body.cpp
	BodyIntegrator.cpp
	PinesGrav.cpp
	Celbody.cpp
	Planet.cpp
	Rigidbody.cpp
	Star.cpp
# Vessel classes
	FlightRecorder.cpp
	SuperVessel.cpp
	Vessel.cpp
	Vesselbase.cpp
	Vesselstatus.cpp
# Surface base classes
	Base.cpp
	Baseobj.cpp
# Cockpit classes
	Defpanel.cpp
	hud.cpp
	MenuInfoBar.cpp
	Pane.cpp
	Panel.cpp
	Panel2D.cpp
	VCockpit.cpp
# MFD classes
	Mfd.cpp
	MfdAlign.cpp
	MfdComms.cpp
	MfdDocking.cpp
	MfdHsi.cpp
	MfdLanding.cpp
	MfdMap.cpp
	MfdMap_old.cpp
	MfdOrbit.cpp
	MfdSurface.cpp
	MfdSync.cpp
	MfdTransfer.cpp
	MfdUser.cpp
# API implementations
	CamAPI.cpp
	CelSphereAPI.cpp
	DrawAPI.cpp
	GraphicsAPI.cpp
	MFDAPI.cpp
	ModuleAPI.cpp
	OrbiterAPI.cpp
# Graphics utils
	D3d7util.cpp
	D3dmath.cpp
# Utils
	Log.cpp
	Memstat.cpp
	Util.cpp
	ZTreeMgr.cpp
	# Cross-platform default vtable for the LaunchpadItem API
	LaunchpadItem.cpp
	LaunchpadRegistry.cpp
	# Built-in Extra-tab items, ImGui-based (compiled-out on Windows
	# where TabExtra.cpp owns the equivalent Win32 dialog flow).
	BuiltinLaunchpadItems.cpp
	# User-writable paths (~/Library/Application Support, ~/Library/Logs)
	UserPaths.cpp
	# Gamepad force-feedback / rumble (SDL_GameController-based on
	# macOS / Linux, no-op stub on Win32 where DirectInput already
	# exposes IDirectInputDevice8::SetEffect natively).
	HapticFX.cpp
)

# Platform-specific sources
if(WIN32)
	list(APPEND common_src
		# Windows-specific
		console_ng.cpp
		ConsoleManager.cpp
		Help.cpp
		Input.cpp
		Di7frame.cpp
		# Win32 Launchpad
		Launchpad.cpp
		LpadTab.cpp
		TabAbout.cpp
		TabExtra.cpp
		TabModule.cpp
		TabOptions.cpp
		TabScenario.cpp
		TabVideo.cpp
		# Win32 Dialogs
		CustomControls.cpp
		DialogWin.cpp
		htmlctrl.c
		# GDI Client
		${GDICLIENT_DIR}/GDIClient.cpp
		# Resources
		Orbiter.rc
		Orbiter.ico
		${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
	)
else()
	# macOS/Linux platform sources - SDL2 integration + stubs
	list(APPEND common_src
		platform_stubs_posix.cpp
		SDLPlatform.cpp
	)
endif()

set(Orbiter_includes
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/Orbitersdk/include
	${CMAKE_SOURCE_DIR}/OVP
	${CMAKE_CURRENT_BINARY_DIR}
)

if(WIN32)
	list(APPEND Orbiter_includes ${HTMLHELP_INCLUDE_DIR})
endif()

# Platform-specific libraries
if(WIN32)
	set(Orbiter_common_libs
		Winmm.dll
		comctl32.lib
		UxTheme.lib
		version.lib
		${HTML_HELP_LIBRARY}
		zlib
		${HTMLHELP_LIB}
		$<TARGET_FILE:Orbitersdk>
		$<TARGET_FILE:DlgCtrl>
	)
	set(Orbiter_libs
		dxguid.lib
		dinput8.lib
	)
elseif(APPLE)
	find_package(ZLIB REQUIRED)
	set(Orbiter_common_libs
		ZLIB::ZLIB
		${SDL2_LIBRARIES}
		${OPENGL_LIBRARIES}
		$<TARGET_FILE:Orbitersdk>
		$<TARGET_FILE:DlgCtrl>
		$<TARGET_FILE:OGLClient>
		"-framework CoreFoundation"
		"-framework IOKit"
		dl
	)
	set(Orbiter_libs "")
else()
	set(Orbiter_common_libs
		zlib
		$<TARGET_FILE:Orbitersdk>
		$<TARGET_FILE:DlgCtrl>
		dl
	)
	set(Orbiter_libs "")
endif()

set(Orbiter_depends
	Orbitersdk
	DlgCtrl
)

# Orbiter executable
add_executable(Orbiter ${common_src})

if(WIN32)
	set_target_properties(Orbiter
		PROPERTIES
		LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:WinMainCRTStartup"
		RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
		OUTPUT_NAME Orbiter
		VS_DEBUGGER_WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
		COMPILE_DEFINITIONS "NOGRAPHICS;DIRECTINPUT_VERSION=0x0800"
		FOLDER Core
	)
else()
	set_target_properties(Orbiter
		PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
		OUTPUT_NAME Orbiter
		FOLDER Core
	)
	target_compile_definitions(Orbiter PRIVATE NOGRAPHICS GENERATE_LOG)
endif()

target_include_directories(Orbiter PUBLIC ${Orbiter_includes} ${TRACY_CLIENT_INCLUDE} ${imgui_SOURCE_DIR}/ ${imgui_SOURCE_DIR}/backends ${implot_SOURCE_DIR}/)

if(APPLE)
	target_include_directories(Orbiter PUBLIC ${SDL2_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/OVP/OGLClient)
endif()

target_link_libraries(Orbiter ${Orbiter_common_libs} ${Orbiter_libs} ${TRACY_CLIENT})

if(NOT WIN32)
	list(APPEND Orbiter_depends OGLClient)
endif()
add_dependencies(Orbiter ${Orbiter_depends})

# Installation: Orbiter executable
install(TARGETS Orbiter	RUNTIME DESTINATION ${ORBITER_INSTALL_ROOT_DIR})

if(WIN32)
	set_target_properties(Orbiter PROPERTIES ENABLE_EXPORTS 1)
	install(FILES $<TARGET_LINKER_FILE:Orbiter>	DESTINATION ${ORBITER_INSTALL_SDK_DIR}/lib)
else()
	# On macOS: export all symbols so dylib plugins can resolve at runtime
	set_target_properties(Orbiter PROPERTIES ENABLE_EXPORTS 1)
endif()

# Copy library to its Orbitersdk/lib location so that external project integrated into the build can find it
if(WIN32)
	add_custom_command(TARGET ${OrbiterTgt}	POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory ${ORBITER_BINARY_SDK_DIR}/lib
		COMMAND ${CMAKE_COMMAND} -E copy ${ORBITER_LIB} ${ORBITER_BINARY_SDK_DIR}/lib/
	)
else()
	add_custom_command(TARGET ${OrbiterTgt}	POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory ${ORBITER_BINARY_SDK_DIR}/lib
	)
	# macOS post-build: create Modules/Celbody symlinks and copy fonts
	if(APPLE)
		set(_symlink_script "${CMAKE_BINARY_DIR}/create_celbody_symlinks.sh")
		file(WRITE ${_symlink_script}
			"#!/bin/sh\n"
			"mkdir -p \"${CMAKE_BINARY_DIR}/Modules/Celbody\" \"${CMAKE_BINARY_DIR}/Fonts\"\n"
			"# (1) Bridge the prebuilt Win32 .dll archives shipped under\n"
			"#     Modules/Celbody/ to .dylib symlinks so dlopen finds them.\n"
			"for f in \"${CMAKE_BINARY_DIR}\"/Modules/Celbody/*.dll; do\n"
			"  [ -f \"$f\" ] || continue\n"
			"  base=$(basename \"$f\" .dll)\n"
			"  ln -sf \"$f\" \"${CMAKE_BINARY_DIR}/Modules/Celbody/lib$base.dylib\" 2>/dev/null\n"
			"done\n"
			"# (2) Bridge the planet / Galsat / Satsat / Vsop87 dylibs\n"
			"#     that get built into Modules/ (because SHARED libraries\n"
			"#     on macOS use the LIBRARY_OUTPUT_DIRECTORY — Modules/ —\n"
			"#     instead of the Modules/Celbody/ value set as RUNTIME).\n"
			"#     Orbiter dlopen's Modules/Celbody/lib<Name>.dylib, so we\n"
			"#     symlink only the known celbody libs (avoids shadowing\n"
			"#     plugin libs that also live in Modules/Plugin/).\n"
			"for name in Sun Mercury Venus Earth Mars Jupiter Saturn Uranus Neptune \\\n"
			"            Moon Vsop87 Galsat Io Europa Ganymede Callisto \\\n"
			"            Satsat Mimas Enceladus Tethys Dione Rhea Titan \\\n"
			"            Hyperion Iapetus Phoebe Vesta Nereid Proteus; do\n"
			"  src=\"${CMAKE_BINARY_DIR}/Modules/lib$name.dylib\"\n"
			"  dst=\"${CMAKE_BINARY_DIR}/Modules/Celbody/lib$name.dylib\"\n"
			"  [ -f \"$src\" ] && ln -sf \"../lib$name.dylib\" \"$dst\" 2>/dev/null\n"
			"done\n"
			"# (3) Bridge atmosphere modules to Modules/Celbody/<Planet>/Atmosphere/.\n"
			"#     CELBODY2::LoadAtmosphereModule resolves lib<Name>.dylib relative\n"
			"#     to that subpath. Build products land in Modules/ flat, so we\n"
			"#     symlink them into the expected per-planet layout.\n"
			"bridge_atm() {\n"
			"  planet=$1; name=$2\n"
			"  src=\"${CMAKE_BINARY_DIR}/Modules/lib$name.dylib\"\n"
			"  atm_dir=\"${CMAKE_BINARY_DIR}/Modules/Celbody/$planet/Atmosphere\"\n"
			"  if [ -f \"$src\" ]; then\n"
			"    mkdir -p \"$atm_dir\"\n"
			"    ln -sf \"../../../lib$name.dylib\" \"$atm_dir/lib$name.dylib\" 2>/dev/null\n"
			"  fi\n"
			"}\n"
			"bridge_atm Venus VenusAtm2006\n"
			"bridge_atm Earth EarthAtm2006\n"
			"bridge_atm Earth EarthAtmJ71G\n"
			"bridge_atm Earth EarthAtmNRLMSISE00\n"
			"bridge_atm Mars MarsAtm2006\n"
			"exit 0\n"
		)
		add_custom_command(TARGET ${OrbiterTgt} POST_BUILD
			COMMENT "Creating Modules/Celbody symlinks and copying fonts..."
			COMMAND sh ${_symlink_script}
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf ${CMAKE_BINARY_DIR}/Fonts/
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${imgui_SOURCE_DIR}/misc/fonts/Cousine-Regular.ttf ${CMAKE_BINARY_DIR}/Fonts/
		)
	endif()
endif()

string(TIMESTAMP DATE "%d %b %Y")
string(TIMESTAMP VERSION "%y%m%d")
configure_file(about.hpp.in about.hpp @ONLY)

# macOS .app bundle + DMG packaging
if(APPLE)
	# Code-signing helper + macos-bundle-distributable + macos-dmg
	# targets. Pulls in APPLE_CODESIGN_IDENTITY +
	# APPLE_NOTARIZATION_KEYCHAIN_PROFILE cache vars.
	include(${CMAKE_SOURCE_DIR}/cmake/codesign.cmake)
endif()

if(APPLE)
	set(ORBITER_VERSION "${VERSION}")
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in ${CMAKE_BINARY_DIR}/Info.plist @ONLY)

	set(_app "${CMAKE_BINARY_DIR}/Orbiter.app")
	add_custom_target(macos-bundle
		COMMENT "Creating Orbiter.app bundle..."
		DEPENDS Orbiter
		# Create .app directory structure
		COMMAND ${CMAKE_COMMAND} -E make_directory "${_app}/Contents/MacOS"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${_app}/Contents/Resources"
		# Copy executable and Info.plist
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/Orbiter" "${_app}/Contents/MacOS/"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/Info.plist" "${_app}/Contents/"
		# Symlink data directories into Resources (avoid copying gigabytes)
		COMMAND ln -sfn "${CMAKE_BINARY_DIR}/Config" "${_app}/Contents/Resources/Config"
		COMMAND ln -sfn "${CMAKE_BINARY_DIR}/Meshes" "${_app}/Contents/Resources/Meshes"
		COMMAND ln -sfn "${CMAKE_BINARY_DIR}/Textures" "${_app}/Contents/Resources/Textures"
		COMMAND ln -sfn "${CMAKE_BINARY_DIR}/Scenarios" "${_app}/Contents/Resources/Scenarios"
		COMMAND ln -sfn "${CMAKE_BINARY_DIR}/Modules" "${_app}/Contents/Resources/Modules"
		COMMAND ln -sfn "${CMAKE_BINARY_DIR}/Fonts" "${_app}/Contents/Resources/Fonts"
		COMMAND ln -sfn "${CMAKE_BINARY_DIR}/GravityModels" "${_app}/Contents/Resources/GravityModels"
	)
endif()
