# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# OpenGL Graphics Client for macOS/Linux

if(NOT WIN32)
	FetchContent_MakeAvailable(imgui)

	add_library(OGLClient STATIC
		OGLClient.cpp
		${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
		${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
	)

	target_include_directories(OGLClient
		PUBLIC ${CMAKE_SOURCE_DIR}/Orbitersdk/include
		PUBLIC ${CMAKE_SOURCE_DIR}/OVP/OGLClient
		PUBLIC ${SDL2_INCLUDE_DIRS}
		PUBLIC ${imgui_SOURCE_DIR}
		PUBLIC ${imgui_SOURCE_DIR}/backends
	)

	target_link_libraries(OGLClient
		${SDL2_LIBRARIES}
		${OPENGL_LIBRARIES}
	)

	set_target_properties(OGLClient
		PROPERTIES
		FOLDER Core
	)
endif()
