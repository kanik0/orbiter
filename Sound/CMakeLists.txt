if(ORBITER_BUILD_XRSOUND)
	add_subdirectory(XRSound)
elseif(NOT WIN32)
	# XRSound's full DLL build is still gated OFF on macOS/Linux until
	# the CString → XRString swap and the irrKlang → IAudioBackend
	# refactor land (M19 / M20). The OpenAL backend on its own compiles
	# cleanly today, so we include it here to keep the M18.a work on
	# the CI build path before the rest of XRSound is ready.
	include(${CMAKE_CURRENT_SOURCE_DIR}/XRSound/CMakeLists_OpenAL.cmake)
endif()
