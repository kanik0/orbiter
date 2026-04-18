// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// User-writable paths for the Orbiter macOS port.
//
// On Windows Orbiter writes its master config and log into the
// install directory (cwd at launch). On macOS the .app bundle lives
// inside /Applications and is read-only for non-admin users, so the
// per-user state has to be stored in a writable location:
//
//   * Orbiter.cfg → ~/Library/Application Support/Orbiter/Orbiter.cfg
//   * Orbiter.log → ~/Library/Logs/Orbiter/Orbiter.log
//
// Linux and Windows fall back to the legacy cwd-relative path so
// existing layouts keep working unchanged.

#ifndef __USERPATHS_H
#define __USERPATHS_H

#include <string>

namespace orbiter {

// macOS: ~/Library/Application Support/Orbiter/ (created if missing)
// Other: empty string — caller falls back to cwd.
std::string GetUserConfigDir();

// macOS: ~/Library/Logs/Orbiter/Orbiter.log (parent dir created)
// Other: empty string — caller falls back to "Orbiter.log".
std::string GetUserLogPath();

// Resolve `name` (e.g. "Orbiter.cfg") under the user config dir on
// macOS, returning the absolute path. The file may or may not exist
// — callers that want a "prefer user override, fall back to install
// dir" policy should test for existence themselves.
std::string ResolveUserConfig(const char *name);

// mkdir -p equivalent. Returns true on success or "already exists".
bool EnsureDir(const std::string &path);

} // namespace orbiter

#endif // !__USERPATHS_H
