// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef __LAUNCHPAD_H
#define __LAUNCHPAD_H

#include "OrbiterAPI.h"
#include "Config.h"

#ifdef _WIN32
#include <CommCtrl.h>

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class LaunchpadTab;
class ExtraTab;
class BuiltinLaunchpadItem;

//-----------------------------------------------------------------------------
// Nonmember functions
//-----------------------------------------------------------------------------
RECT GetClientPos (HWND hWnd, HWND hChild);
void SetClientPos (HWND hWnd, HWND hChild, RECT &r);

INT_PTR CALLBACK AppDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK WaitPageProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif // _WIN32

namespace orbiter {

	class LaunchpadTab;
	class ExtraTab;

	//-----------------------------------------------------------------------------
	// Name: class LaunchpadDialog
	// Desc: Handles the startup dialog ("Launchpad")
	//-----------------------------------------------------------------------------
	class LaunchpadDialog {
		friend class Orbiter;
#ifdef _WIN32
		friend class LaunchpadTab;
#endif

	public:
		LaunchpadDialog(Orbiter* app);
		~LaunchpadDialog();

		bool Create(bool startvideotab = false);
		void Show();
		void Hide();

		inline bool Visible() const { return m_bVisible; }
		inline Orbiter* App() const { return pApp; }
		inline Config* Cfg() const { return pCfg; }

#ifdef _WIN32
		bool ConsumeMessage(LPMSG msg);
		const HWND GetWaitWindow() const { return hWait; }
		LaunchpadTab* GetTab(UINT i) const;
		HWND HTabContainer() const { return hTabContainer; }

		void AddTab(LaunchpadTab* tab);
		void EnableLaunchButton(bool enable) const;

		HTREEITEM RegisterExtraParam(LaunchpadItem* item, HTREEITEM parent = 0);
		bool UnregisterExtraParam(LaunchpadItem* item);
		HTREEITEM FindExtraParam(const char* name, const HTREEITEM parent = 0);
#endif

		void WriteExtraParams();

		ExtraTab* GetExtraTab() const { return pExtra; }

		void UpdateConfig();

		void ShowWaitPage(bool show, long mem_committed = 0);
		void UpdateWaitProgress();
		long mem_wait;
		long mem0;

	private:
		HINSTANCE hInst;
		HWND hDlg;
		bool m_bVisible;
		Orbiter* pApp;
		Config* pCfg;
		orbiter::ExtraTab* pExtra;

#ifdef _WIN32
		std::vector<LaunchpadTab*> TabList;
		LaunchpadTab* CTab;
		HWND hTabContainer;
		HWND hWait;
		HBRUSH hDlgBrush;
		HANDLE hShadowImg;

		void SetDemoMode();
		int SelectDemoScenario();
		void InitSize(HWND hWnd);
		BOOL Resize(HWND hWnd, DWORD w, DWORD h, DWORD mode);
		void InitTabControl(HWND hWnd);
		void SwitchTabPage(HWND hWnd, int pg);

		INT_PTR DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		INT_PTR WaitProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		static INT_PTR CALLBACK s_DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		friend INT_PTR CALLBACK ::WaitPageProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		friend LONG_PTR FAR PASCAL MsgProc_CopyrightFrame(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		RECT client0, copyr0;
		RECT r_launch0, r_help0, r_exit0, r_data0, r_wait0, r_version0;
		DWORD shadowh;
		int dy_bt;
#endif
	};

}

#endif // !__LAUNCHPAD_H
