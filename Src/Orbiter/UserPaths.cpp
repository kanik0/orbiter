// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "UserPaths.h"
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#endif

namespace orbiter {

#ifndef _WIN32
// Resolve the current user's home directory. Prefers $HOME, falls back
// to getpwuid_r so we still work in environments where $HOME is unset
// (cron, system services). Returns "/tmp" as a last resort so we never
// emit an empty prefix.
static std::string HomeDir()
{
	if (const char *h = std::getenv("HOME"); h && *h) return h;
	if (struct passwd *pw = getpwuid(getuid()); pw && pw->pw_dir)
		return pw->pw_dir;
	return "/tmp";
}
#endif

bool EnsureDir(const std::string &path)
{
#ifdef _WIN32
	(void)path;
	return false;
#else
	if (path.empty()) return false;
	// Walk parents and mkdir each. Tolerates EEXIST.
	for (size_t i = 1; i <= path.size(); ++i) {
		if (i < path.size() && path[i] != '/') continue;
		std::string seg = path.substr(0, i);
		if (seg.empty() || seg == "/") continue;
		struct stat st;
		if (stat(seg.c_str(), &st) == 0) {
			if (!S_ISDIR(st.st_mode)) return false;
			continue;
		}
		if (mkdir(seg.c_str(), 0755) != 0) return false;
	}
	return true;
#endif
}

std::string GetUserConfigDir()
{
#if defined(__APPLE__)
	std::string d = HomeDir() + "/Library/Application Support/Orbiter";
	EnsureDir(d);
	return d + "/";
#else
	return ""; // Windows / Linux → caller uses cwd
#endif
}

std::string GetUserLogPath()
{
#if defined(__APPLE__)
	std::string d = HomeDir() + "/Library/Logs/Orbiter";
	EnsureDir(d);
	return d + "/Orbiter.log";
#else
	return "";
#endif
}

std::string ResolveUserConfig(const char *name)
{
	std::string d = GetUserConfigDir();
	if (d.empty()) return "";
	if (!name) return d;
	return d + name;
}

} // namespace orbiter
