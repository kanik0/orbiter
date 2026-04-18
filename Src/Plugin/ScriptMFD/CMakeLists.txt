# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_library(ScriptMFD SHARED
	ScriptMFD.cpp
	ScriptMFD.rc
)

target_include_directories(ScriptMFD
	PUBLIC ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
	PUBLIC ${MODULE_COMMON_DIR}
	PUBLIC ${ORBITER_SOURCE_ROOT_DIR}/Src/Module/LuaScript/LuaInterpreter
)

target_link_libraries(ScriptMFD
	${ORBITER_LIB}
	${ORBITER_SDK_LIB}
	lua::lib
	${LUAINTERPRETER_LIB}
)

add_dependencies(ScriptMFD
	${OrbiterTgt}
	Orbitersdk	
	LuaInterpreter
)

set_target_properties(ScriptMFD
	PROPERTIES
	FOLDER Modules
)

# Installation
install(TARGETS ScriptMFD
	RUNTIME
	DESTINATION ${ORBITER_INSTALL_PLUGIN_DIR}
)

orbiter_module_info(ScriptMFD
	CATEGORY "Script tools and drivers"
	DESCRIPTION "SCRIPT MFD:\n\nThe driver module for script-based MFD (multifunctional display) modes. By activating this module, all script-based MFD modes become available during the simulation."
)
