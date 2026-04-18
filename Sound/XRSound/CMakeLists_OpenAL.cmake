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
	${_XRSOUND_SRC}/external/decoders_impl.cpp
)

# Suppress the long list of benign warnings that ship with single-header
# audio decoders. These are vendored-in third-party code we audit via
# pinned URLs but don't actively maintain.
set_source_files_properties(
	${_XRSOUND_SRC}/external/decoders_impl.cpp
	PROPERTIES COMPILE_OPTIONS
	"-Wno-unused-function;-Wno-unused-variable;-Wno-unused-parameter;-Wno-sign-compare;-Wno-implicit-fallthrough;-Wno-missing-braces;-Wno-shift-negative-value"
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

# Smoke test — a tiny executable that boots the OpenAL backend, loads
# three shipping default sounds and verifies the irrKlang-parity API
# round-trips. CTest picks it up automatically when ORBITER_BUILD_XRSOUND
# is ON.
add_executable(xrsound_openal_smoke
	${_XRSOUND_SRC}/xrsound_openal_smoke.cpp
)
target_link_libraries(xrsound_openal_smoke
	PRIVATE XRSound_backend_openal
)
set_target_properties(xrsound_openal_smoke PROPERTIES
	FOLDER Sound
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Sound
)

include(CTest)
if(BUILD_TESTING)
	add_test(
		NAME xrsound_openal_smoke
		COMMAND xrsound_openal_smoke
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	set_tests_properties(xrsound_openal_smoke PROPERTIES
		ENVIRONMENT "XRSOUND_SMOKE_ASSET_DIR=${CMAKE_BINARY_DIR}/XRSound/Default"
	)
endif()
