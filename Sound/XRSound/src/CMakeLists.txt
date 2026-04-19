## XRSound_dll
set(_XRSOUND_CORE_SOURCES
	DefaultSoundGroupPreSteps.cpp
	ModuleXRSoundEngine.cpp
	SoundPreSteps.cpp
	VesselXRSoundEngine.cpp
	XRSoundConfigFileParser.cpp
	XRSoundDLL.cpp
	XRSoundEngine.cpp
	XRSoundEngine30.cpp
	Utils/ConfigFileParser.cpp
	Utils/FileList.cpp
)
if(WIN32)
	# Windows gets the resource script + the legacy VS-generated dllmain.
	list(APPEND _XRSOUND_CORE_SOURCES Resource.rc)
endif()

add_library(XRSound_dll SHARED ${_XRSOUND_CORE_SOURCES})

if(WIN32)
	set_target_properties(XRSound_dll PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_PLUGIN_DIR}
		OUTPUT_NAME XRSound
		COMPILE_DEFINITIONS "XRSOUND_DLL_BUILD;_CRT_SECURE_NO_WARNINGS;_USRDLL;XRSOUNDDLL_EXPORTS"
	)
else()
	# macOS / Linux: ship as libXRSound.dylib / .so in Modules/Plugin.
	set_target_properties(XRSound_dll PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY ${ORBITER_BINARY_PLUGIN_DIR}
		OUTPUT_NAME XRSound
		COMPILE_DEFINITIONS "XRSOUND_DLL_BUILD"
	)
endif()

target_include_directories(XRSound_dll PUBLIC
	${ORBITER_SOURCE_SDK_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/Utils
)
if(WIN32)
	target_include_directories(XRSound_dll PUBLIC ${IRRKLANG_DIR}/include)
endif()

if(WIN32)
	target_link_libraries(XRSound_dll
		${ORBITER_LIB}
		${ORBITER_SDK_LIB}
		${IRRKLANG_LIB}
	)
else()
	# XRSound_backend_openal owns OpenAL + vendored decoders; the
	# ${OPENAL_LIBRARY} dependency comes through as transitive.
	target_link_libraries(XRSound_dll
		${ORBITER_LIB}
		${ORBITER_SDK_LIB}
		XRSound_backend_openal
	)
endif()

add_dependencies(XRSound_dll
	${OrbiterTgt}
	Orbitersdk
)

install(TARGETS XRSound_dll
	RUNTIME DESTINATION ${ORBITER_INSTALL_PLUGIN_DIR}
)

if(NOT WIN32)
	# Sidecar .info for the OGL Launchpad — without it XRSound falls
	# into the default "Miscellaneous" category with no description.
	include(${CMAKE_SOURCE_DIR}/cmake/orbiter_module_info.cmake)
	orbiter_module_info(XRSound_dll
		CATEGORY "Audio"
		DESCRIPTION "XRSOUND:\n\nOpenAL-backed audio engine for vessel sounds (engine, RCS, docking, atmospheric buffet, UI cues). Enable to hear in-sim audio. Configure per-vessel sound sets under Config/XRSound/."
	)
endif()

## XRSound_lib
add_library(XRSound_lib STATIC
	XRSound.cpp
	XRSoundImpl.cpp
)

set_target_properties(XRSound_lib
	PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY ${ORBITER_BINARY_SDK_DIR}/$<1:XRSound>
	OUTPUT_NAME XRSound
)

target_include_directories(XRSound_lib PUBLIC
	${ORBITER_SOURCE_SDK_INCLUDE_DIR}
)

install(TARGETS XRSound_lib
	ARCHIVE DESTINATION ${ORBITER_INSTALL_SDK_DIR}/XRSound
)
