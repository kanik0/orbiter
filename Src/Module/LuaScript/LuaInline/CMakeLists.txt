# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# Core library to go to the Orbiter main directory. Both the Win32
# .dll and the POSIX .dylib / .so need to land in the Orbiter root so
# the loader (`Script.cpp` path=".") can find them — LuaInline.cpp
# now uses std::thread / std::condition_variable (#22) so there's no
# platform-specific quarantine any more.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR})

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

# LuaInline lives in the Orbiter root, but its transitive dep
# libLuaInterpreter.dylib sits in Modules/ alongside the other plugins.
# Without an explicit rpath hint dyld only checks siblings of LuaInline and
# the link fails. Keep the same block on Linux (RUNPATH uses $ORIGIN).
if(APPLE)
	set_target_properties(LuaInline PROPERTIES
		BUILD_RPATH   "@loader_path/Modules"
		INSTALL_RPATH "@loader_path/Modules")
elseif(UNIX)
	set_target_properties(LuaInline PROPERTIES
		BUILD_RPATH   "\$ORIGIN/Modules"
		INSTALL_RPATH "\$ORIGIN/Modules")
endif()


# Installation
install(TARGETS LuaInline
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
)
