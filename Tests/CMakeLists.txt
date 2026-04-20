Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.0-preview3
)

FetchContent_MakeAvailable(Catch2)

# Utility function
function(add_test_file test_name)
	add_executable(${test_name} "${test_name}.cpp")

	set_target_properties( ${test_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${ORBITER_BINARY_ROOT_DIR}" )

	# Test binaries live at the build root next to Orbiter, but the
	# plugin-side dylibs they link against (e.g. libLuaInterpreter) are
	# deployed under Modules/ to match the runtime loader layout. Extend
	# the rpath so dyld can find them without copying or symlinking.
	if(APPLE)
		set_target_properties(${test_name} PROPERTIES
			BUILD_RPATH   "@loader_path;@loader_path/Modules"
			INSTALL_RPATH "@loader_path;@loader_path/Modules"
		)
		# libLuaInterpreter is linked with -undefined dynamic_lookup so it
		# can reach back into the Orbiter executable at runtime. A test
		# harness has no such host, so dyld aborts on the first unresolved
		# RTTI symbol (typeinfo/vtable are bound eagerly). Compile a small
		# stubs TU into the test to provide those key symbols — it's never
		# exercised at runtime, only needed by the loader.
		target_sources(${test_name} PRIVATE Stubs.cpp)
	elseif(UNIX)
		set_target_properties(${test_name} PROPERTIES
			BUILD_RPATH   "\$ORIGIN;\$ORIGIN/Modules"
			INSTALL_RPATH "\$ORIGIN;\$ORIGIN/Modules"
		)
	endif()

	target_include_directories(${test_name}
		PRIVATE ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
		PRIVATE ${MODULE_COMMON_DIR}
		PRIVATE ${ORBITER_SOURCE_ROOT_DIR}/Src/Module/LuaScript/LuaInterpreter
	)

	target_link_libraries(${test_name}
		${ORBITER_LIB}
		${ORBITER_SDK_LIB}
		lua::lib
		${LUAINTERPRETER_LIB}
		Catch2::Catch2WithMain
	)

	add_dependencies(${test_name}
		${OrbiterTgt}
		Orbitersdk
		LuaInterpreter
	)

	add_test( 
		NAME ${test_name} 
		COMMAND $<TARGET_FILE:${test_name}>
		WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
	)
	set_tests_properties(${test_name} PROPERTIES TIMEOUT 30)
endfunction()

# Register unit tests
add_test_file(Lua.Interpreter)

# Rendering parity harness (M30) — driven by a Python script that
# captures a frame from a live scenario and SSIM-compares it with a
# baseline. POSIX-only because it consumes the OGLClient capture path.
if(NOT WIN32)
	add_subdirectory(rendering_parity)
endif()

if (BUILD_ORBITER_SERVER)

	# Sanity check for scenario tests
	add_test(
		NAME "Scenario.SanityCheck"
		COMMAND $<TARGET_FILE:Orbiter_server> "--scenariox=${CMAKE_SOURCE_DIR}/Scenarios/Delta-glider/Smack!.scn" "--maxframes=60"
		WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
	)
	set_tests_properties(Scenario.SanityCheck PROPERTIES TIMEOUT 60)

	# Register scenario tests
	file(GLOB TestScenarios "${CMAKE_SOURCE_DIR}/Scenarios/Tests/*.scn")
	foreach(Scenario ${TestScenarios})
		get_filename_component(test_name ${Scenario} NAME_WE)
		add_test(
			NAME "Scenario.${test_name}"
			COMMAND $<TARGET_FILE:Orbiter_server> "--scenariox=${Scenario}"
			WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
		)
		set_tests_properties(Scenario.${test_name} PROPERTIES TIMEOUT 60)
	endforeach()

endif()
