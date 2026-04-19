# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# The target directory for plugin DLLs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_PLUGIN_DIR})

# Sibling .info metadata so the Launchpad groups this sample alongside
# the other Developer-category plugins (Framerate, Meshdebug) rather than
# the default "Miscellaneous" bucket where it was showing up unannotated
# as a user-facing plugin (#32).
include(${CMAKE_SOURCE_DIR}/cmake/orbiter_module_info.cmake)

add_library(AscentMFD MODULE
	AscentMFD.cpp
	AscentMFD.rc
)

add_dependencies(AscentMFD
	${OrbiterTgt}
	Orbitersdk
)

target_link_libraries(AscentMFD
	${ORBITER_LIB}
	${ORBITER_SDK_LIB}
)

target_include_directories(AscentMFD
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
)

set_target_properties(AscentMFD
	PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Modules/Plugin
	LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Modules/Plugin
	FOLDER Modules
)

# On macOS / Linux the sample MFD references oapi* symbols (oapiGetMass,
# oapiRegisterMFDMode, GraphMFD::SetAutoRange, ...) that live in the Orbiter
# main executable, not in libOrbitersdk.a. Resolve them at runtime via
# dynamic lookup — same trick used by the vessel SHARED libraries.
if(APPLE OR UNIX)
	set_target_properties(AscentMFD PROPERTIES
		LINK_FLAGS "-undefined dynamic_lookup"
	)
endif()

install(TARGETS AscentMFD
	LIBRARY
	DESTINATION ${ORBITER_INSTALL_PLUGIN_DIR}
)

orbiter_module_info(AscentMFD
	CATEGORY "Developer resources and samples"
	DESCRIPTION "ASCENT MFD:\n\nAscent profile analysis MFD — SDK sample demonstrating custom MFD registration, trajectory plotting and vessel parameter queries. Source under Orbitersdk/samples/AscentMFD. Engage from any vessel's MFD mode list."
)

