# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_subdirectory(samples)
add_subdirectory(sample_build)

file(GLOB_RECURSE includes
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	LIST_DIRECTORIES false
	include/*
)
# gcCoreAPI.h is generated from gcCore.h by D3D9Client's
# InterfaceBuilder.exe (OVP/D3D9Client/CMakeLists.txt:221) and lands
# in the same ${ORBITER_BINARY_SDK_DIR}/include/ destination — having
# both rules emit the same output trips ninja with
#   "multiple rules generate Orbitersdk/include/gcCoreAPI.h".
# Drop it from this GLOB so D3D9Client's rule is the single producer
# on Windows; non-Windows builds (where D3D9Client and InterfaceBuilder
# are not built) consume the committed source-tree gcCoreAPI.h directly
# via the ${ORBITER_SOURCE_SDK_INCLUDE_DIR} include path that all
# non-D3D9 consumers (LuaInterpreter, OGLClient, ...) already use.
list(REMOVE_ITEM includes "include/gcCoreAPI.h")
foreach(fname ${includes})
	list(APPEND input
		${CMAKE_CURRENT_SOURCE_DIR}/${fname}
	)
	list(APPEND output
		${ORBITER_BINARY_SDK_DIR}/${fname}
	)
	list(APPEND commands
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${fname} ${ORBITER_BINARY_SDK_DIR}/${fname}
	)
endforeach()

add_custom_command(
	OUTPUT ${output}
	DEPENDS ${input}
	${commands}
)
add_custom_target(copy_sdk_include
	DEPENDS ${output}
)
add_dependencies(Orbitersdk
	copy_sdk_include
)

if(ORBITER_MAKE_DOC)
	if(Doxygen_FOUND)
		add_subdirectory(doxygen)
	endif()
endif()

install(DIRECTORY html include
	DESTINATION ${ORBITER_INSTALL_SDK_DIR}
)

# Copy Prebuild PDFs ------------------------------------------------------

install(FILES doc/BuildingSamples.pdf DESTINATION ${ORBITER_INSTALL_SDK_DIR}/doc)

