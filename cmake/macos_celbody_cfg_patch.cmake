# Copyright (c) Martin Schweiger
# Licensed under the MIT License
#
# macos_celbody_cfg_patch.cmake — CMake script-mode helper invoked as
#     cmake -DINPUT=... -DOUTPUT=... -DCELBODY=... -P macos_celbody_cfg_patch.cmake
#
# Used by the 8 celbody directories whose runtime is shipped only as a
# Win32 PE32 .dll (Ariel, Deimos, Miranda, Oberon, Phobos, Titania,
# Triton, Umbriel). On non-Windows builds dlopen cannot load those
# binaries, so we patch each body's Config/<Name>.cfg to:
#   * comment out the `Module = <Name>` line (disables module loading)
#   * uncomment `EllipticOrbit = TRUE`, `HasElements = TRUE`, and the
#     six Keplerian orbital element lines (Epoch / SemiMajorAxis /
#     Eccentricity / Inclination / LongAscNode / LongPerihelion /
#     MeanLongitude)
#
# Orbiter's CelestialBody core then propagates the body via the same
# unperturbed-Kepler path already used for Nereid / Proteus (and for
# Triton upstream, where the module is commented out on Windows too
# because of a broken MSVCR90D linkage). Accuracy matches what the
# pre-built 2006-era DLLs produced — those implement the same
# Keplerian propagation internally.

if(NOT INPUT)
    message(FATAL_ERROR "macos_celbody_cfg_patch.cmake: -DINPUT=<source cfg> required")
endif()
if(NOT OUTPUT)
    message(FATAL_ERROR "macos_celbody_cfg_patch.cmake: -DOUTPUT=<target cfg> required")
endif()
if(NOT CELBODY)
    message(FATAL_ERROR "macos_celbody_cfg_patch.cmake: -DCELBODY=<body name> required")
endif()

file(READ "${INPUT}" _cfg)

# Comment the active `Module = <Celbody>` line so Orbiter's core
# skips dlopen and falls back to the Keplerian element path.
# Upstream Triton.cfg already ships with this line commented, so we
# only match active assignments.
string(REGEX REPLACE
    "(^|\n)Module[ \t]*=[ \t]*${CELBODY}([^\n]*)"
    "\\1;Module = ${CELBODY}   ; disabled on non-Windows (Orbiter issue #8)"
    _cfg "${_cfg}")

# Uncomment the ephemeris fallback switches and the six Keplerian
# element lines that every affected cfg ships commented out.
foreach(_key
        "EllipticOrbit"
        "HasElements"
        "Epoch"
        "SemiMajorAxis"
        "Eccentricity"
        "Inclination"
        "LongAscNode"
        "LongPerihelion"
        "MeanLongitude")
    string(REGEX REPLACE
        "(^|\n)[ \t]*;[ \t]*(${_key}[ \t]*=)"
        "\\1\\2"
        _cfg "${_cfg}")
endforeach()

file(WRITE "${OUTPUT}" "${_cfg}")
