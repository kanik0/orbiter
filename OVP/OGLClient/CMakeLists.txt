# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# OpenGL Graphics Client for macOS/Linux

if(NOT WIN32)
	FetchContent_MakeAvailable(imgui)

	add_library(OGLClient STATIC
		OGLClient.cpp
		OGLTexture.cpp
		OGLShaderMgr.cpp
		OGLMeshRegistry.cpp
		OGLMaterial.cpp
		OGLEnvMap.cpp
		OGLShadowMap.cpp
		OGLScene.cpp
		OGLvObject.cpp
		OGLvPlanet.cpp
		OGLvVessel.cpp
		OGLCelSphere.cpp
		OGLSurface.cpp
		OGLSketchpad.cpp
		OGLLaunchpad.cpp
		OGLTile.cpp
		OGLParticle.cpp
		OGLBeaconArray.cpp
		OGLAnnotation.cpp
		OGLAtmosphere.cpp
		OGLPostProcess.cpp
		OGLConfig.cpp
		OGLvBase.cpp
		${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
		${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
	)

	# Copy shader files to build directory
	add_custom_command(TARGET OGLClient POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${CMAKE_CURRENT_SOURCE_DIR}/shaders"
			"${CMAKE_BINARY_DIR}/shaders"
		COMMENT "Copying OGLClient shaders to build directory"
	)

	target_include_directories(OGLClient
		PUBLIC ${CMAKE_SOURCE_DIR}/Orbitersdk/include
		PUBLIC ${CMAKE_SOURCE_DIR}/OVP/OGLClient
		PUBLIC ${CMAKE_SOURCE_DIR}/Src/Orbiter
		# Generated headers (about.hpp built from about.hpp.in by Src/Orbiter)
		PUBLIC ${CMAKE_BINARY_DIR}/Src/Orbiter
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
