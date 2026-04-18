# OpenAL-backed XRSound backend for macOS/Linux.
#
# Built as a standalone static library so M18.a can land + CI-verify
# before the full XRSound DLL is unlocked on macOS (happens in M20).
# The library has a single translation unit (OpenALBackend.cpp) and
# exposes xrsound::IAudioBackend via IAudioBackend.h.

if(WIN32)
	return()
endif()

# Prefer Homebrew openal-soft (portable, actively maintained, EFX) over
# Apple's deprecated <OpenAL/al.h> framework.
set(_HB_OPENAL_SOFT /opt/homebrew/opt/openal-soft)
if(APPLE AND EXISTS ${_HB_OPENAL_SOFT}/lib/libopenal.dylib)
	set(OPENAL_INCLUDE_DIR ${_HB_OPENAL_SOFT}/include)
	set(OPENAL_LIBRARY     ${_HB_OPENAL_SOFT}/lib/libopenal.dylib)
	message(STATUS "XRSound: using Homebrew openal-soft at ${_HB_OPENAL_SOFT}")
else()
	find_package(OpenAL REQUIRED)
endif()

set(_XRSOUND_SRC ${CMAKE_SOURCE_DIR}/Sound/XRSound/src)
add_library(XRSound_backend_openal STATIC
	${_XRSOUND_SRC}/OpenALBackend.cpp
)

target_include_directories(XRSound_backend_openal
	PUBLIC  ${_XRSOUND_SRC}
	PRIVATE ${OPENAL_INCLUDE_DIR}
)

target_link_libraries(XRSound_backend_openal
	PRIVATE ${OPENAL_LIBRARY}
)

set_target_properties(XRSound_backend_openal PROPERTIES
	FOLDER Sound
	POSITION_INDEPENDENT_CODE ON
)
