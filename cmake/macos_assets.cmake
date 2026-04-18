# Copyright (c) Martin Schweiger
# Licensed under the MIT License
#
# macos_assets.cmake — fetch the third-party assets the macOS / Linux
# port needs at run time but cannot ship inline (license clarity,
# upstream pin freshness). All downloads use file(DOWNLOAD) with
# pinned SHA-256 hashes and EXPECTED_HASH validation so the build
# fails closed on a tampered or moved upstream.
#
# Currently provisioned:
#   * FontAwesome 6 Free (Solid)  fa-solid-900.ttf   SIL OFL 1.1
#   * Lekton Bold                 Lekton-Bold.ttf    SIL OFL 1.1
#   * Architects Daughter         architext.regular.ttf
#                                                    SIL OFL 1.1
#     (substituted for the legacy "architext" font referenced by
#      Config.cpp:245 — both are handwritten-style faces; we
#      install Architects Daughter under the historical filename so
#      existing Orbiter.cfg files keep working unchanged.)
#
# All three are written into ${CMAKE_BINARY_DIR}/Fonts/ next to the
# Roboto + Cousine TTFs that imgui's misc/fonts/ already supplies, and
# get install()'d into the .app bundle's Resources/Fonts/ directory.

if(WIN32)
	# Windows ships the legacy fonts inside the SDK installer; nothing
	# to do here.
	return()
endif()

set(_orbiter_macos_fonts_dir ${CMAKE_BINARY_DIR}/Fonts)
file(MAKE_DIRECTORY ${_orbiter_macos_fonts_dir})

# Helper — download `url` to `dst` with SHA-256 verification. Idempotent:
# subsequent configure runs skip the download if the file is already
# present and its hash matches.
function(orbiter_fetch_font name url sha256)
	set(dst "${_orbiter_macos_fonts_dir}/${name}")
	if(EXISTS "${dst}")
		file(SHA256 "${dst}" _have)
		if(_have STREQUAL "${sha256}")
			return()
		endif()
		message(STATUS "Refreshing ${name} (hash mismatch)")
	endif()
	message(STATUS "Fetching font ${name}")
	file(DOWNLOAD "${url}" "${dst}"
		EXPECTED_HASH SHA256=${sha256}
		TLS_VERIFY ON
		STATUS _status
		LOG _log
		SHOW_PROGRESS)
	list(GET _status 0 _code)
	if(NOT _code EQUAL 0)
		file(REMOVE "${dst}")
		message(FATAL_ERROR
			"Failed to download ${name} from ${url}\n${_log}")
	endif()
endfunction()

# Pinned upstream URLs + SHA-256 hashes. When bumping a version,
# rebuild the hash with `shasum -a 256 path/to/file.ttf`.
orbiter_fetch_font(
	"fa-solid-900.ttf"
	"https://github.com/FortAwesome/Font-Awesome/raw/6.7.2/webfonts/fa-solid-900.ttf"
	"af19d135d3a935b3ebfbd80320716ffe1202052c5f68dc2c5f1abc57005ac605"
)
orbiter_fetch_font(
	"Lekton-Bold.ttf"
	"https://github.com/google/fonts/raw/main/ofl/lekton/Lekton-Bold.ttf"
	"9aab5a281f3428d882e67d9a945930822380f5b28ad00f5fbf79e21dadee375f"
)
orbiter_fetch_font(
	"architext.regular.ttf"
	"https://github.com/google/fonts/raw/main/ofl/architectsdaughter/ArchitectsDaughter-Regular.ttf"
	"6159718a08898e34bc1cb7354086141a5f9a70b73e54dbec27ead0d59a697359"
)

install(DIRECTORY ${_orbiter_macos_fonts_dir}/
	DESTINATION ${ORBITER_INSTALL_ROOT_DIR}/Fonts
	FILES_MATCHING PATTERN "*.ttf"
)
