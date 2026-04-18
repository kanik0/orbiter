// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// Cross-platform default implementations for the LaunchpadItem API
// declared in Orbitersdk/include/OrbiterAPI.h. The Win32 build used to
// keep these inside TabExtra.cpp; the macOS / Linux Launchpad lives in
// OGLLaunchpad and links the same default vtable.

#ifdef _WIN32
#include <windows.h>
#else
#include "OrbiterPlatform.h"
#endif

#include "OrbiterAPI.h"

LaunchpadItem::LaunchpadItem()
{
	hItem = 0;
}

LaunchpadItem::~LaunchpadItem()
{
}

char *LaunchpadItem::Name()
{
	return 0;
}

char *LaunchpadItem::Description()
{
	return 0;
}

bool LaunchpadItem::OpenDialog(HINSTANCE hInst, HWND hLaunchpad, int resId, DLGPROC pDlg)
{
#ifdef _WIN32
	DialogBoxParam(hInst, MAKEINTRESOURCE(resId), hLaunchpad, pDlg, (LPARAM)this);
	return true;
#else
	// Resource-based Win32 dialogs are not available on macOS / Linux;
	// addons should override clbkRender() to expose ImGui-based UI on
	// these platforms.
	(void)hInst; (void)hLaunchpad; (void)resId; (void)pDlg;
	return false;
#endif
}

bool LaunchpadItem::clbkOpen(HWND hLaunchpad)
{
	(void)hLaunchpad;
	return false;
}

int LaunchpadItem::clbkWriteConfig()
{
	return 0;
}

bool LaunchpadItem::clbkRender()
{
	return false;
}
