add_subdirectory(Scenarios)

if(ORBITER_MAKE_DOC)
	add_subdirectory(Main)
endif()

# Non-Windows builds don't have the HTML Help Compiler (HHC) needed to roll
# Html/Main into orbiter.chm; add_subdirectory(Main) above is skipped unless
# ORBITER_MAKE_DOC is on, and even then it only produces the .chm.
# For the POSIX help viewer (DlgHelp::OpenHelp hands the topic to `open` /
# `xdg-open` — see Src/Orbiter/DlgHelp.cpp:OpenInBrowser) we need the raw
# .htm / .css / media alongside the binary so the browser can resolve
# relative URLs. Mirror the source tree to the build output's Html/Main
# on every configure, no CHM involvement.
if(NOT WIN32)
	file(GLOB_RECURSE HTML_MAIN_FILES
		RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/Main
		Main/*.htm Main/*.html Main/*.css Main/*.js
		Main/*.jpg Main/*.jpeg Main/*.png Main/*.gif
		Main/*.hhc Main/*.hhk Main/*.hhp)
	set(HTML_MAIN_STAMPS)
	foreach(rel IN LISTS HTML_MAIN_FILES)
		set(src ${CMAKE_CURRENT_SOURCE_DIR}/Main/${rel})
		set(dst ${ORBITER_BINARY_HTML_DIR}/Main/${rel})
		add_custom_command(OUTPUT ${dst}
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${src} ${dst}
			DEPENDS ${src})
		list(APPEND HTML_MAIN_STAMPS ${dst})
	endforeach()
	add_custom_target(orbiter_html_main ALL DEPENDS ${HTML_MAIN_STAMPS})
	install(DIRECTORY Main/
		DESTINATION ${ORBITER_INSTALL_HTML_DIR}/Main
		FILES_MATCHING
			PATTERN "*.htm" PATTERN "*.html" PATTERN "*.css" PATTERN "*.js"
			PATTERN "*.jpg" PATTERN "*.jpeg" PATTERN "*.png" PATTERN "*.gif"
			PATTERN "*.hhc" PATTERN "*.hhk" PATTERN "*.hhp"
			PATTERN "CMakeLists.txt" EXCLUDE)
endif()
