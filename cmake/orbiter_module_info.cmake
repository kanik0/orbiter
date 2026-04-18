# orbiter_module_info(target CATEGORY <category> DESCRIPTION <description>)
#
# Emits a <target>.info text file alongside the plugin module so the
# Orbiter Launchpad (OGLLaunchpad on macOS / Linux, future ImGui port on
# Windows) can show the same metadata that the Win32 build embeds as
# string-table resources (IDS_TYPE / IDS_INFO).
#
# Format (parsed by ogl::ParseModuleInfo in OGLLaunchpad.cpp):
#
#   [Category]
#   <one line, e.g. "MFD modes">
#   [Description]
#   <free-form text, may span multiple lines>
#
# Newline-encoded "\n" sequences in DESCRIPTION are converted to real
# newlines so callers can pass single-line CMake strings.

function(orbiter_module_info target)
	set(options)
	set(oneValueArgs CATEGORY)
	set(multiValueArgs DESCRIPTION)
	cmake_parse_arguments(MI "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	if(NOT MI_CATEGORY)
		set(MI_CATEGORY "Miscellaneous")
	endif()
	if(NOT MI_DESCRIPTION)
		set(MI_DESCRIPTION "(no description provided)")
	endif()

	# join multi-arg DESCRIPTION lines with newlines
	string(REPLACE ";" "\n" _desc "${MI_DESCRIPTION}")
	# allow callers to embed literal \n
	string(REPLACE "\\n" "\n" _desc "${_desc}")

	set(_info_path "${ORBITER_BINARY_PLUGIN_DIR}/${target}.info")

	# Stage the file content via configure_file (preserves newlines that
	# the cmake -E echo dance loses) and copy it next to the dylib at
	# build time so a wiped Modules/Plugin/ is restored by an
	# incremental rebuild.
	set(_staging "${CMAKE_CURRENT_BINARY_DIR}/${target}.info.staging")
	file(MAKE_DIRECTORY "${ORBITER_BINARY_PLUGIN_DIR}")
	file(WRITE "${_staging}"
		"[Category]\n${MI_CATEGORY}\n[Description]\n${_desc}\n")
	file(COPY "${_staging}"
		DESTINATION "${ORBITER_BINARY_PLUGIN_DIR}")
	file(RENAME
		"${ORBITER_BINARY_PLUGIN_DIR}/${target}.info.staging"
		"${_info_path}")

	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${_staging}" "${_info_path}"
		VERBATIM
	)

	# Install alongside the module if installation is enabled.
	if(DEFINED ORBITER_INSTALL_PLUGIN_DIR)
		install(FILES "${_info_path}"
			DESTINATION ${ORBITER_INSTALL_PLUGIN_DIR}
			OPTIONAL)
	endif()
endfunction()
