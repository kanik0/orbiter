// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
// Help window
//
// On Windows the inline HtmlHelp control opens the .chm file directly
// in the embedded HTMLHelp viewer. On macOS / Linux we resolve the
// (helpfile, topic) tuple to a local .htm file under the Html/
// directory and hand it off to the platform browser via the system
// "open" / "xdg-open" command.
// ======================================================================

#include "DlgHelp.h"
#include "OrbiterAPI.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <unistd.h>

#ifdef _WIN32
#  include <windows.h>
#  include <htmlhelp.h>
#  include <io.h>
#endif

DlgHelp::DlgHelp() : ImGuiDialog("Orbiter: Help") {}
void DlgHelp::Display() {}
void DlgHelp::OnDraw() {}

#ifndef _WIN32
namespace {

// Strip the legacy ".chm" container portion of the helpfile path, keep
// only the leading directory ("html/orbiter.chm" → "html").
std::string ChmDir(const std::string &chm)
{
	auto slash = chm.find_last_of("/\\");
	if (slash == std::string::npos) return "html";
	std::string dir = chm.substr(0, slash);
	for (auto &c : dir) if (c == '\\') c = '/';
	// canonical case the build uses for the directory
	if (dir == "html") dir = "Html";
	return dir;
}

// Resolve a HELPCONTEXT (helpfile, topic) pair to an absolute path
// inside the build/install tree. Returns an empty string if the file
// does not exist.
std::string ResolveHelpFile(const HELPCONTEXT *hc)
{
	if (!hc || !hc->helpfile) return "";

	std::string topic = (hc->topic ? hc->topic : "");
	while (!topic.empty() && (topic.front() == '/' || topic.front() == '\\'))
		topic.erase(topic.begin());
	if (topic.empty()) return "";

	std::string dir = ChmDir(hc->helpfile);

	// Most dialog topics live under Html/Main/<topic>; scenario topics
	// live under Html/Scenarios. Try both.
	const char *roots[] = { "Main", "Scenarios", "" };
	char cwd[1024];
	if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';

	for (const char *root : roots) {
		std::string path = dir;
		if (*root) { path += '/'; path += root; }
		path += '/'; path += topic;
		std::string abs = (cwd[0] ? std::string(cwd) + "/" + path : path);
		if (access(abs.c_str(), R_OK) == 0)
			return abs;
	}
	return "";
}

void OpenInBrowser(const std::string &absPath)
{
	if (absPath.empty()) return;
#if defined(__APPLE__)
	const char *opener = "open";
#else
	const char *opener = "xdg-open";
#endif
	char cmd[2048];
	snprintf(cmd, sizeof(cmd), "%s \"%s\" >/dev/null 2>&1 &",
		opener, absPath.c_str());
	if (std::system(cmd) != 0) {
		oapiAddNotification(OAPINOTIF_ERROR, "Failed to open help",
			absPath.c_str());
	}
}

} // namespace
#endif // !_WIN32

void DlgHelp::OpenHelp(const HELPCONTEXT *hc)
{
#ifdef _WIN32
	char buf[256];
	HWND hWnd = (HWND)(ImGui::GetMainViewport()->PlatformHandle);
	if (hc->topic)
		snprintf(buf, 256, "%s::%s", hc->helpfile, hc->topic);
	else
		snprintf(buf, 256, "%s", hc->helpfile);
	buf[255] = '\0';

	if (!HtmlHelp(hWnd, buf, HH_DISPLAY_TOPIC, NULL)) {
		oapiAddNotification(OAPINOTIF_ERROR, "Failed to open help", buf);
	}
#else
	std::string abs = ResolveHelpFile(hc);
	if (abs.empty()) {
		char info[512];
		snprintf(info, sizeof(info), "Help topic not found: %s::%s",
			hc->helpfile ? hc->helpfile : "(null)",
			hc->topic    ? hc->topic    : "(null)");
		oapiAddNotification(OAPINOTIF_WARNING, "Help unavailable", info);
		return;
	}
	OpenInBrowser(abs);
#endif
}
