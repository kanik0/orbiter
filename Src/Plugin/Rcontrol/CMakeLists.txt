# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_library(Rcontrol SHARED
	Rcontrol.cpp
)

target_include_directories(Rcontrol
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
)

target_link_libraries(Rcontrol
	${ORBITER_LIB}
	${ORBITER_SDK_LIB}
	${ORBITER_DLGCTRL_LIB}
	imgui
)

add_dependencies(Rcontrol
	${OrbiterTgt}
	Orbitersdk
)

set_target_properties(Rcontrol
	PROPERTIES
	FOLDER Modules
)

# Installation
install(TARGETS Rcontrol
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_PLUGIN_DIR}
)

install(FILES Rcontrol.png
	DESTINATION ${ORBITER_INSTALL_TEXTURES_DIR}/MenuInfoBar
)

# Also stage the icon in the build tree so the menu bar finds it
# without requiring a full install (#44).
add_custom_command(TARGET Rcontrol POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
		${ORBITER_BINARY_TEXTURES_DIR}/MenuInfoBar
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_CURRENT_SOURCE_DIR}/Rcontrol.png
		${ORBITER_BINARY_TEXTURES_DIR}/MenuInfoBar/Rcontrol.png
	VERBATIM
)

orbiter_module_info(Rcontrol
	CATEGORY "Input devices"
	DESCRIPTION "REMOTE CONTROL:\n\nRemote vessel control panel — manipulate the focus vessel (RCS, main/hover/retro thrusters, attitude programs) from a separate floating window. Useful when flying with a HOTAS while keeping the cockpit view free of overlays."
)
