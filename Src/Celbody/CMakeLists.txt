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
