# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# Core library to go to the Orbiter main directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR})
# NOTE: on macOS / Linux we deliberately do NOT set LIBRARY_OUTPUT_DIRECTORY
# to the root. LuaInline.cpp still uses Win32 threading (_beginthreadex,
# WaitForSingleObject, TerminateThread) which is stubbed on POSIX. Loading
# the dylib successfully — rather than silently failing to locate it —
# exposes a SIGSEGV when a vessel (e.g. DeltaGlider::AAPSubsystem) calls
# oapiExecScriptCmd. Until LuaInline is ported to std::thread, leave the
# dylib in Modules/ where the loader won't find it, keeping the current
# "silent-broken scripting, no crash" behaviour.
# See GitHub issue tracked under label `platform:macos`, `scope:lua`.

add_library(LuaInline SHARED
	LuaInline.cpp
)

target_include_directories(LuaInline
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
	PUBLIC ${ORBITER_SOURCE_ROOT_DIR}/Src/Module/LuaScript/LuaInterpreter
)

target_link_libraries(LuaInline
	${ORBITER_LIB}
	${ORBITER_SDK_LIB}
	LuaInterpreter
	lua::lib
)

add_dependencies(LuaInline
	${OrbiterTgt}
	Orbitersdk
	LuaInterpreter
)

set_target_properties(LuaInline
	PROPERTIES
	FOLDER Modules/Lua
)


# Installation
install(TARGETS LuaInline
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
)
