# Copyright (c) Martin Schweiger
# Licensed under the MIT License

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_STARTUP_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ORBITER_BINARY_STARTUP_DIR})

# Pull in imgui_SOURCE_DIR for the macOS / Linux clbkRender path.
if(NOT WIN32)
	FetchContent_MakeAvailable(imgui)
endif()

add_library(AtlantisConfig SHARED
	AtlantisConfig.cpp
	AConfigurator.rc
)

target_include_directories(AtlantisConfig
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
	# imgui headers are needed for the macOS / Linux clbkRender path
	# (the editor body is drawn with ImGui calls). On Windows the
	# resource-script-driven dialog flow keeps using HWND.
	PUBLIC ${imgui_SOURCE_DIR}
)

target_link_libraries(AtlantisConfig
	${ORBITER_LIB}
	${ORBITER_SDK_LIB}
)

add_dependencies(AtlantisConfig
	${OrbiterTgt}
	Orbitersdk
)

set_target_properties(AtlantisConfig
	PROPERTIES
	FOLDER Vessels/Atlantis
)

# Installation
install(TARGETS AtlantisConfig
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_MODULE_DIR}/Startup
)
