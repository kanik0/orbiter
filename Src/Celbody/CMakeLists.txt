# Copyright (c) Martin Schweiger
# Licensed under the MIT License

# Helper for the 8 celestial bodies whose runtime is shipped only as a
# prebuilt Win32 PE32 .dll (Ariel, Deimos, Miranda, Oberon, Phobos,
# Titania, Triton, Umbriel). On Windows we copy the .dll into
# Modules/Celbody/ and install it alongside the .cfg. On non-Windows
# the PE32 binary cannot be loaded, so we patch the .cfg to disable
# `Module = <Name>` and enable the Keplerian element fallback via
# `cmake/macos_celbody_cfg_patch.cmake`. See Orbiter issue #8.
function(orbiter_celbody_prebuilt CELBODY)
    if(WIN32)
        add_custom_command(
            OUTPUT ${ORBITER_BINARY_CELBODY_DIR}/${CELBODY}.dll
            COMMAND ${CMAKE_COMMAND} -E make_directory ${ORBITER_BINARY_CELBODY_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/${CELBODY}.dll
                ${ORBITER_BINARY_CELBODY_DIR}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${CELBODY}.dll
        )

        srcdir_to_tgtlist("${CMAKE_CURRENT_SOURCE_DIR}/Config"
                          "${ORBITER_BINARY_CONFIG_DIR}" srclist tgtlist)
        add_custom_command(
            OUTPUT ${tgtlist}
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/Config/
                ${ORBITER_BINARY_CONFIG_DIR}
            DEPENDS ${srclist}
        )

        add_custom_target(${CELBODY}
            DEPENDS ${tgtlist} ${ORBITER_BINARY_CELBODY_DIR}/${CELBODY}.dll
        )

        install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/${CELBODY}.dll
                DESTINATION ${ORBITER_INSTALL_CELBODY_DIR})
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Config
                DESTINATION ${ORBITER_INSTALL_ROOT_DIR})
    else()
        set(_patched_cfg ${ORBITER_BINARY_CONFIG_DIR}/${CELBODY}.cfg)
        add_custom_command(
            OUTPUT ${_patched_cfg}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${ORBITER_BINARY_CONFIG_DIR}
            COMMAND ${CMAKE_COMMAND}
                -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/Config/${CELBODY}.cfg
                -DOUTPUT=${_patched_cfg}
                -DCELBODY=${CELBODY}
                -P ${CMAKE_SOURCE_DIR}/cmake/macos_celbody_cfg_patch.cmake
            DEPENDS
                ${CMAKE_CURRENT_SOURCE_DIR}/Config/${CELBODY}.cfg
                ${CMAKE_SOURCE_DIR}/cmake/macos_celbody_cfg_patch.cmake
            COMMENT "Patching ${CELBODY}.cfg for non-Windows (Keplerian fallback)"
        )
        add_custom_target(${CELBODY}
            DEPENDS ${_patched_cfg}
        )
        install(FILES ${_patched_cfg}
                DESTINATION ${ORBITER_INSTALL_ROOT_DIR}/Config)
    endif()

    add_dependencies(Sol ${CELBODY})
    set_target_properties(${CELBODY} PROPERTIES FOLDER Celbody)
endfunction()

add_subdirectory(Sol)
add_subdirectory(Vsop87)
add_subdirectory(Moon)
add_subdirectory(Phobos)
add_subdirectory(Deimos)
add_subdirectory(Vesta)
add_subdirectory(Galsat)
add_subdirectory(Satsat)
add_subdirectory(Miranda)
add_subdirectory(Ariel)
add_subdirectory(Umbriel)
add_subdirectory(Titania)
add_subdirectory(Oberon)
add_subdirectory(Triton)
add_subdirectory(Proteus)
add_subdirectory(Nereid)

# macOS-only: bridge the celbody dylibs from Modules/ into the
# Modules/Celbody/ layout Orbiter dlopens at runtime, plus the per-planet
# Modules/Celbody/<Planet>/Atmosphere/ subfolders CELBODY2 expects. Declared
# here (not in Src/Orbiter/CMakeLists.txt where it used to live as a POST_BUILD
# of the Orbiter executable) because celbody dylibs link *against* Orbiter and
# are therefore built *after* it — the old hook fired before any symlink
# source existed and left the directory empty until rebuilt manually.
if(APPLE)
	set(_symlink_script "${CMAKE_BINARY_DIR}/create_celbody_symlinks.sh")
	file(WRITE ${_symlink_script}
		"#!/bin/sh\n"
		"mkdir -p \"${CMAKE_BINARY_DIR}/Modules/Celbody\"\n"
		"# Bridge the planet / Galsat / Satsat / Vsop87 dylibs that get built\n"
		"# into Modules/ (SHARED libraries use LIBRARY_OUTPUT_DIRECTORY — the\n"
		"# flat Modules/ — instead of the Modules/Celbody/ value set as RUNTIME).\n"
		"# Orbiter dlopen's Modules/Celbody/lib<Name>.dylib, so we symlink only\n"
		"# the known celbody libs to avoid shadowing plugin libs that also live\n"
		"# in Modules/Plugin/. The 8 prebuilt Win32 PE32 bodies (Ariel, Deimos,\n"
		"# Miranda, Oberon, Phobos, Titania, Triton, Umbriel) are handled by\n"
		"# cmake/macos_celbody_cfg_patch.cmake which rewrites their .cfg to use\n"
		"# the Keplerian fallback — no dylib exists for them on macOS.\n"
		"for name in Sun Mercury Venus Earth Mars Jupiter Saturn Uranus Neptune \\\n"
		"            Moon Vsop87 Galsat Io Europa Ganymede Callisto \\\n"
		"            Satsat Mimas Enceladus Tethys Dione Rhea Titan \\\n"
		"            Hyperion Iapetus Phoebe Vesta Nereid Proteus; do\n"
		"  src=\"${CMAKE_BINARY_DIR}/Modules/lib$name.dylib\"\n"
		"  dst=\"${CMAKE_BINARY_DIR}/Modules/Celbody/lib$name.dylib\"\n"
		"  [ -f \"$src\" ] && ln -sf \"../lib$name.dylib\" \"$dst\" 2>/dev/null\n"
		"done\n"
		"# Bridge atmosphere modules to Modules/Celbody/<Planet>/Atmosphere/.\n"
		"bridge_atm() {\n"
		"  planet=$1; name=$2\n"
		"  src=\"${CMAKE_BINARY_DIR}/Modules/lib$name.dylib\"\n"
		"  atm_dir=\"${CMAKE_BINARY_DIR}/Modules/Celbody/$planet/Atmosphere\"\n"
		"  if [ -f \"$src\" ]; then\n"
		"    mkdir -p \"$atm_dir\"\n"
		"    ln -sf \"../../../lib$name.dylib\" \"$atm_dir/lib$name.dylib\" 2>/dev/null\n"
		"  fi\n"
		"}\n"
		"bridge_atm Venus VenusAtm2006\n"
		"bridge_atm Earth EarthAtm2006\n"
		"bridge_atm Earth EarthAtmJ71G\n"
		"bridge_atm Earth EarthAtmNRLMSISE00\n"
		"bridge_atm Mars MarsAtm2006\n"
		"exit 0\n"
	)

	# Targets the script actually symlinks, in the same order as the shell
	# loop. Keeping the two lists in the same file makes drift easy to catch
	# in review. Atmosphere modules are added only when the corresponding
	# subdirectory was included (Earth's Vsop87 subtree wires them up).
	set(_celbody_link_targets
		# Vsop87 major bodies
		Sun Mercury Venus Earth Mars Jupiter Saturn Uranus Neptune Vsop87
		# Other bodies
		Moon Vesta
		# Galilean moons (Jupiter)
		Galsat Io Europa Ganymede Callisto
		# Saturnian moons
		Satsat Mimas Enceladus Tethys Dione Rhea Titan Hyperion Iapetus
		# Atmospheres
		VenusAtm2006 EarthAtm2006 EarthAtmJ71G EarthAtmNRLMSISE00 MarsAtm2006
	)

	add_custom_target(celbody_symlinks ALL
		COMMAND sh ${_symlink_script}
		COMMENT "Linking celbody dylibs into Modules/Celbody/..."
		VERBATIM
	)
	foreach(_tgt IN LISTS _celbody_link_targets)
		if(TARGET ${_tgt})
			add_dependencies(celbody_symlinks ${_tgt})
		endif()
	endforeach()
	set_target_properties(celbody_symlinks PROPERTIES FOLDER Celbody)
endif()
