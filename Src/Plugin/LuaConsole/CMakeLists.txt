# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_library(LuaConsole SHARED
	LuaConsole.cpp
	ConsoleInterpreter.cpp
)

target_include_directories(LuaConsole
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
	PUBLIC ${ORBITER_SOURCE_ROOT_DIR}/Src/Module/LuaScript/LuaInterpreter
)

target_link_libraries(LuaConsole
	${ORBITER_LIB}
	${ORBITER_SDK_LIB}
	lua::lib
	${LUAINTERPRETER_LIB}
	imgui
)

add_dependencies(LuaConsole
	${OrbiterTgt}
	Orbitersdk
	LuaInterpreter
)

set_target_properties(LuaConsole
	PROPERTIES
	FOLDER Modules
)

# Installation
install(TARGETS LuaConsole
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_PLUGIN_DIR}
)

install(FILES LuaConsole.png
	DESTINATION ${ORBITER_INSTALL_TEXTURES_DIR}/MenuInfoBar
)

# Also stage the icon in the build tree — the menu bar looks at
# Textures/MenuInfoBar/ relative to the binary directory (#44).
add_custom_command(TARGET LuaConsole POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory
		${ORBITER_BINARY_TEXTURES_DIR}/MenuInfoBar
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_CURRENT_SOURCE_DIR}/LuaConsole.png
		${ORBITER_BINARY_TEXTURES_DIR}/MenuInfoBar/LuaConsole.png
	VERBATIM
)

orbiter_module_info(LuaConsole
	CATEGORY "Script tools and drivers"
	DESCRIPTION "LUA CONSOLE:\n\nInteractive Lua interpreter window. Execute Lua commands and scripts at runtime to inspect or manipulate the simulation state, vessels, planets and modules."
)
