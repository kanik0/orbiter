# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_library(FlightData SHARED
	FlightData.cpp
)

target_include_directories(FlightData
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
	PUBLIC ${MODULE_COMMON_DIR}
)

target_link_libraries(FlightData
	${ORBITER_LIB}
	${ORBITER_SDK_LIB}
	imgui
)

add_dependencies(FlightData
	${OrbiterTgt}
	Orbitersdk
)

set_target_properties(FlightData
	PROPERTIES
	FOLDER Modules
)

# Installation
install(TARGETS FlightData
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_PLUGIN_DIR}
)

install(FILES FlightData.png
	DESTINATION ${ORBITER_INSTALL_TEXTURES_DIR}/MenuInfoBar
)

# Also stage the icon in the build tree so the menu bar finds it
# without requiring a full install (#44).
add_custom_command(TARGET FlightData POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
		${ORBITER_BINARY_TEXTURES_DIR}/MenuInfoBar
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_CURRENT_SOURCE_DIR}/FlightData.png
		${ORBITER_BINARY_TEXTURES_DIR}/MenuInfoBar/FlightData.png
	VERBATIM
)

orbiter_module_info(FlightData
	CATEGORY "Tools and dialogs"
	DESCRIPTION "FLIGHT DATA RECORDER:\n\nGraph and log atmospheric flight parameters (altitude, dynamic pressure, mach, AoA, vertical speed, ...) for the focus vessel. Open the recorder window from the Custom Functions dialog (Ctrl-F4)."
)
