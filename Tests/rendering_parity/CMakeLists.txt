# Copyright (c) Martin Schweiger
# Licensed under the MIT License
#
# Rendering parity test harness (M30). Registers a single CTest
# target that drives Tests/rendering_parity/run_parity.py against
# the bundled scenario manifest. Skipped on Windows builds — the
# harness consumes the macOS / Linux --capture-frame CLI plumbing
# added in M30.a.

if(WIN32)
	return()
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(NOT Python3_FOUND)
	message(STATUS
		"rendering_parity: skipping — no python3 interpreter found")
	return()
endif()

# CTest test. Picks up the freshly-built Orbiter from the same
# binary directory so the harness exercises the same code path as
# an interactive run.
add_test(
	NAME rendering_parity
	COMMAND ${Python3_EXECUTABLE}
		${CMAKE_CURRENT_SOURCE_DIR}/run_parity.py
		--orbiter ${CMAKE_BINARY_DIR}/Orbiter
		--manifest ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
		--out-dir ${CMAKE_BINARY_DIR}/Testing/parity
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(rendering_parity PROPERTIES
	TIMEOUT 600                 # 10 min wall-clock budget
	LABELS "macos;rendering"
)
