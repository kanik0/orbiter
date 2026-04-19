# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_library(ExtMFD SHARED
	ExtMFD.cpp
	MFDWindow.cpp
)

target_include_directories(ExtMFD
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
	PUBLIC ${MODULE_COMMON_DIR}
)

target_link_libraries(ExtMFD
	${ORBITER_LIB}
	${ORBITER_SDK_LIB}
	imgui
)

add_dependencies(ExtMFD
	${OrbiterTgt}
	Orbitersdk
	imgui
)

set_target_properties(ExtMFD
	PROPERTIES
	FOLDER Modules
)

# Installation
install(TARGETS ExtMFD
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_PLUGIN_DIR}
)

install(FILES ExtMFD.png
	DESTINATION ${ORBITER_INSTALL_TEXTURES_DIR}/MenuInfoBar
)

# Also stage the icon in the build tree so the menu bar finds it
# without requiring a full install (#44).
add_custom_command(TARGET ExtMFD POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
		${ORBITER_BINARY_TEXTURES_DIR}/MenuInfoBar
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_CURRENT_SOURCE_DIR}/ExtMFD.png
		${ORBITER_BINARY_TEXTURES_DIR}/MenuInfoBar/ExtMFD.png
	VERBATIM
)

orbiter_module_info(ExtMFD
	CATEGORY "Tools and dialogs"
	DESCRIPTION "EXTERNAL MFD:\n\nAttach an MFD instrument as a movable, resizable floating window outside the main cockpit. Useful for users with multiple monitors or for keeping an MFD visible while panning the camera away from the cockpit."
)
