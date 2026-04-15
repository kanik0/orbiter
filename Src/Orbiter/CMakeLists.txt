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
	target_compile_definitions(Orbiter PRIVATE NOGRAPHICS)
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

set_target_properties(Orbiter PROPERTIES ENABLE_EXPORTS 1)

install(FILES $<TARGET_LINKER_FILE:Orbiter>	DESTINATION ${ORBITER_INSTALL_SDK_DIR}/lib)

# Copy library to its Orbitersdk/lib location so that external project integrated into the build can find it
add_custom_command(TARGET ${OrbiterTgt}	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${ORBITER_BINARY_SDK_DIR}/lib
	COMMAND ${CMAKE_COMMAND} -E copy ${ORBITER_LIB} ${ORBITER_BINARY_SDK_DIR}/lib/
)

string(TIMESTAMP DATE "%d %b %Y")
string(TIMESTAMP VERSION "%y%m%d")
configure_file(about.hpp.in about.hpp @ONLY)
