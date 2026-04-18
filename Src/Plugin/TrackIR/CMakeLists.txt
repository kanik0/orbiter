# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# TrackIR depends on the Windows-only NaturalPoint NPClient driver.
# Skip the build entirely on non-Windows platforms so the Launchpad
# does not list a non-functional entry. (See Fase E roadmap M25.)
if(NOT WIN32)
	return()
endif()

add_library(TrackIR SHARED
	TrackIR.cpp
	TrackIR.rc
	TrackIRconfig.cpp
	NPClientWraps.cpp
)

target_include_directories(TrackIR
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
	PUBLIC ${MODULE_COMMON_DIR}
)

target_link_libraries(TrackIR
	${ORBITER_LIB}
	${ORBITER_SDK_LIB}
	$<$<PLATFORM_ID:Windows>:UxTheme.lib>
)

add_dependencies(TrackIR
	${OrbiterTgt}
	Orbitersdk
)

set_target_properties(TrackIR
	PROPERTIES
	FOLDER Modules
)

# Installation
install(TARGETS TrackIR
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_PLUGIN_DIR}
)
