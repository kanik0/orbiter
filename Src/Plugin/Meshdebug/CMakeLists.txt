# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# Pull in imgui_SOURCE_DIR for the macOS / Linux ImGui dialog path.
if(NOT WIN32)
	FetchContent_MakeAvailable(imgui)
endif()

add_library(Meshdebug SHARED
	Meshdebug.cpp
	Meshdebug.rc
)

target_include_directories(Meshdebug
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
	PUBLIC ${MODULE_COMMON_DIR}
	PUBLIC ${imgui_SOURCE_DIR}
)

target_link_libraries(Meshdebug
	${ORBITER_LIB}
	${ORBITER_SDK_LIB}
)

add_dependencies(Meshdebug
	${OrbiterTgt}
	Orbitersdk
)

set_target_properties(Meshdebug
	PROPERTIES
	FOLDER Modules
)

# Installation
install(TARGETS Meshdebug
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_PLUGIN_DIR}
)

orbiter_module_info(Meshdebug
	CATEGORY "Developer resources and samples"
	DESCRIPTION "MESH DEBUG:\n\nA small utility for developers. Meshdebug can be used to visually identify a mesh group in a vessel mesh. Set the camera focus on the vessel whose meshes you want to inspect, then open the mesh debugger dialog from the Custom functions list (Ctrl-F4). The vessel meshes will become semi-transparent, and the selected mesh group will blink."
)
