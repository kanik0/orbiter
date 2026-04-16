# OpenAL-based XRSound backend for macOS/Linux
# This is a standalone build that doesn't require irrKlang

if(APPLE)
	add_library(XRSound_OpenAL STATIC
		src/OpenALBackend.cpp
	)

	target_include_directories(XRSound_OpenAL
		PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
		PUBLIC ${CMAKE_SOURCE_DIR}/Orbitersdk/include
	)

	# macOS ships with OpenAL framework
	find_library(OPENAL_FRAMEWORK OpenAL)
	if(OPENAL_FRAMEWORK)
		target_link_libraries(XRSound_OpenAL ${OPENAL_FRAMEWORK})
		message(STATUS "Found OpenAL: ${OPENAL_FRAMEWORK}")
	else()
		message(WARNING "OpenAL framework not found — audio will be disabled")
	endif()

	set_target_properties(XRSound_OpenAL PROPERTIES FOLDER Sound)
endif()
