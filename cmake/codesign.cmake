# Copyright (c) Martin Schweiger
# Licensed under the MIT License
#
# codesign.cmake — macOS code-signing + DMG packaging helpers.
#
# Two CMake variables steer the policy:
#
#   APPLE_CODESIGN_IDENTITY
#     The "Developer ID Application: …" identity returned by
#     `security find-identity -v -p codesigning`. Empty (default)
#     produces an unsigned ad-hoc bundle that runs locally after a
#     manual `xattr -dr com.apple.quarantine` but cannot pass
#     Gatekeeper for distribution.
#
#   APPLE_NOTARIZATION_KEYCHAIN_PROFILE
#     A profile name previously stored via
#     `xcrun notarytool store-credentials …`. Empty (default) skips
#     notarization. Required for the .dmg to launch from Finder
#     without warnings on a fresh macOS install.
#
# The companion `macos-dmg` target wraps the .app from
# `macos-bundle-distributable` into a UDZO-compressed disk image
# at ${CMAKE_BINARY_DIR}/Orbiter-macos-arm64.dmg.

if(NOT APPLE)
	return()
endif()

set(APPLE_CODESIGN_IDENTITY ""
	CACHE STRING "macOS code-signing identity (empty = unsigned ad-hoc)")
set(APPLE_NOTARIZATION_KEYCHAIN_PROFILE ""
	CACHE STRING "notarytool keychain profile (empty = skip notarization)")

set(_app  "${CMAKE_BINARY_DIR}/Orbiter.app")
set(_dist "${CMAKE_BINARY_DIR}/Orbiter-dist.app")
set(_dmg  "${CMAKE_BINARY_DIR}/Orbiter-macos-arm64.dmg")
set(_entitlements "${CMAKE_SOURCE_DIR}/cmake/Orbiter.entitlements.plist")

# Distributable bundle: same layout as macos-bundle but data dirs are
# *copied* (not symlinked) so the .app is self-contained and survives
# the round-trip through hdiutil.
add_custom_target(macos-bundle-distributable
	COMMENT "Creating distributable Orbiter.app (copy data dirs)..."
	DEPENDS Orbiter
	COMMAND ${CMAKE_COMMAND} -E rm -rf "${_dist}"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${_dist}/Contents/MacOS"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${_dist}/Contents/Resources"
	COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_BINARY_DIR}/Orbiter"   "${_dist}/Contents/MacOS/"
	COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_BINARY_DIR}/Info.plist" "${_dist}/Contents/"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/Config"        "${_dist}/Contents/Resources/Config"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/Meshes"        "${_dist}/Contents/Resources/Meshes"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/Textures"      "${_dist}/Contents/Resources/Textures"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/Scenarios"     "${_dist}/Contents/Resources/Scenarios"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/Modules"       "${_dist}/Contents/Resources/Modules"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/Fonts"         "${_dist}/Contents/Resources/Fonts"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/GravityModels" "${_dist}/Contents/Resources/GravityModels"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/Script"        "${_dist}/Contents/Resources/Script"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/Flights"       "${_dist}/Contents/Resources/Flights"
	# XRSound runtime assets: configs + Default/*.wav pack (produced by
	# Sound/XRSound/CMakeLists.txt in the build tree, not in the source tree).
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/XRSound"       "${_dist}/Contents/Resources/XRSound"
	# Doc / Html / Localdoc: copy source trees first, then overlay any
	# PDFs produced during the build (e.g. XRSound User Manual, which
	# lands in ${CMAKE_BINARY_DIR}/Doc/). make_directory guarantees the
	# overlay source exists even when no build step has populated it,
	# and copy_directory onto an existing destination merges/overwrites
	# so generated PDFs win over their LaTeX/ODT sources.
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_SOURCE_DIR}/Doc"           "${_dist}/Contents/Resources/Doc"
	COMMAND ${CMAKE_COMMAND} -E make_directory
		"${CMAKE_BINARY_DIR}/Doc"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_BINARY_DIR}/Doc"           "${_dist}/Contents/Resources/Doc"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_SOURCE_DIR}/Html"          "${_dist}/Contents/Resources/Html"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_SOURCE_DIR}/Localdoc"      "${_dist}/Contents/Resources/Localdoc"
)

# codesign step. Handles three cases:
#   1. APPLE_CODESIGN_IDENTITY non-empty → real signature
#   2. otherwise → ad-hoc signature (`-`) so dyld is happy with
#      relative @executable_path / @rpath linkages on Apple Silicon
add_custom_target(macos-codesign
	COMMENT "Code-signing Orbiter.app..."
	DEPENDS macos-bundle-distributable
	COMMAND /bin/sh -c "if [ -n \"$APPLE_CODESIGN_IDENTITY\" ]; then \
		codesign --force --options runtime --timestamp \
			$([ -f \"${_entitlements}\" ] && echo \"--entitlements ${_entitlements}\") \
			--deep --sign \"$APPLE_CODESIGN_IDENTITY\" \"${_dist}\"; \
	else \
		echo '[codesign] APPLE_CODESIGN_IDENTITY not set — applying ad-hoc signature'; \
		codesign --force --deep --sign - \"${_dist}\"; \
	fi"
	VERBATIM)

# DMG packaging. hdiutil consumes the .app as the source folder.
add_custom_target(macos-dmg
	COMMENT "Building Orbiter-macos-arm64.dmg..."
	DEPENDS macos-codesign
	COMMAND ${CMAKE_COMMAND} -E rm -f "${_dmg}"
	COMMAND hdiutil create
		-volname "Orbiter"
		-srcfolder "${_dist}"
		-ov -format UDZO
		"${_dmg}"
	# Optional notarization step. The notarytool call needs a valid
	# Apple ID + app-specific password stored under the profile name
	# referenced by APPLE_NOTARIZATION_KEYCHAIN_PROFILE.
	COMMAND /bin/sh -c "if [ -n \"$APPLE_NOTARIZATION_KEYCHAIN_PROFILE\" ]; then \
		echo '[notarize] submitting ${_dmg}'; \
		xcrun notarytool submit \"${_dmg}\" \
			--keychain-profile \"$APPLE_NOTARIZATION_KEYCHAIN_PROFILE\" \
			--wait; \
		xcrun stapler staple \"${_dmg}\"; \
	else \
		echo '[notarize] APPLE_NOTARIZATION_KEYCHAIN_PROFILE not set — DMG is unsigned for distribution; users must xattr -dr com.apple.quarantine after download'; \
	fi"
	VERBATIM)
