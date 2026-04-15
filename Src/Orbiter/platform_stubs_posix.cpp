// Platform stubs for non-Windows builds
// Provides minimal implementations of classes whose full implementation
// is Windows-specific (DirectInput, Win32 Launchpad, ConsoleNG, etc.)

#ifndef _WIN32

#include "OrbiterPlatform.h"
#include "d3d_compat.h"
#include "resource_stub.h"
#include "Orbiter.h"
#include "Input.h"
#include "Launchpad.h"
#include "Script.h"
#include "Memstat.h"
#include "console_ng.h"

// ======================================================================
// DInput stubs - SDL2 input will replace these
// ======================================================================

DInput::DInput(Orbiter *pOrbiter) : orbiter(pOrbiter), diframe(nullptr), m_hWnd(nullptr) {
	diframe = new CDIFramework7();
}
DInput::~DInput() { delete diframe; }
HRESULT DInput::Create(HINSTANCE) { return S_OK; }
void DInput::Destroy() {}
void DInput::SetRenderWindow(HWND hWnd) { m_hWnd = hWnd; }
bool DInput::CreateKbdDevice() { return true; }
bool DInput::CreateJoyDevice() { return false; }
void DInput::DestroyDevices() {}
void DInput::OptionChanged(DWORD, DWORD) {}
HRESULT DInput::SetJoystickProperties() { return S_OK; }
bool DInput::PollJoystick(void*) { return false; }

// ======================================================================
// LaunchpadDialog stubs - ImGui launchpad will replace these
// ======================================================================

namespace orbiter {

LaunchpadDialog::LaunchpadDialog(Orbiter* app)
	: pApp(app), pCfg(app->Cfg()), hInst(nullptr), hDlg(nullptr),
	  m_bVisible(false), pExtra(nullptr), mem_wait(0), mem0(0) {}
LaunchpadDialog::~LaunchpadDialog() {}
bool LaunchpadDialog::Create(bool) { return true; }
void LaunchpadDialog::Show() { m_bVisible = true; }
void LaunchpadDialog::Hide() { m_bVisible = false; }
void LaunchpadDialog::WriteExtraParams() {}
void LaunchpadDialog::UpdateConfig() {}
void LaunchpadDialog::ShowWaitPage(bool, long) {}
void LaunchpadDialog::UpdateWaitProgress() {}

} // namespace orbiter

// ======================================================================
// ConsoleNG stubs
// ======================================================================

namespace orbiter {
ConsoleNG::ConsoleNG(Orbiter* p) : m_pOrbiter(p), m_hWnd(nullptr) {}
ConsoleNG::~ConsoleNG() {}
bool ConsoleNG::ParseCmd() { return false; }
void ConsoleNG::Echo(const char*) const {}
void ConsoleNG::EchoIntro() const {}
}

// ======================================================================
// Orbiter methods that reference Win32-only features
// ======================================================================

// MouseEvent, BroadcastMouseEvent, BroadcastImmediateKeyboardEvent,
// BroadcastBufferedKeyboardEvent, KbdInputBuffered_System/OnRunning are now
// compiled from Orbiter.cpp for all platforms (no longer Windows-only).
bool Orbiter::SendKbdBuffered(DWORD, DWORD*, DWORD, bool) { return false; }
bool Orbiter::SendKbdImmediate(char[256], bool) { return false; }

// MemStat is compiled from Memstat.cpp - no stub needed

// ======================================================================
// ImGui Win32 backend stubs (SDL2 backend will replace)
// ======================================================================

bool ImGui_ImplWin32_Init(void*) { return false; }
void ImGui_ImplWin32_NewFrame() {}
void ImGui_ImplWin32_Shutdown() {}

// ======================================================================
// DialogWin stubs
// ======================================================================

#include "DialogWin.h"

DialogWin::DialogWin(HWND, HWND, HWND, DWORD) {}
DialogWin::DialogWin(HINSTANCE, HWND, int, DLGPROC, DWORD, void*) {}
DWORD DialogWin::GetTitleButtonState(DWORD) { return 0; }
bool DialogWin::SetTitleButtonState(DWORD, DWORD) { return false; }
bool DialogWin::Create_AddTitleButton(DWORD, HBITMAP, DWORD) { return false; }
bool DialogWin::Create_SetTitleButtonState(DWORD, DWORD) { return false; }
DialogWin::~DialogWin() {}
HWND DialogWin::OpenWindow() { return nullptr; }
void DialogWin::Message(DWORD, void*) {}
void DialogWin::ToggleShrink() {}
void DialogWin::Update() {}
BOOL DialogWin::OnCommand(HWND, WORD, WORD, HWND) { return FALSE; }
BOOL DialogWin::OnSize(HWND, WPARAM, int, int) { return FALSE; }
BOOL DialogWin::OnMove(HWND, int, int) { return FALSE; }

// ======================================================================
// GraphicsClient methods stubbed in the #ifndef _WIN32 block of
// GraphicsAPI.cpp need additional stubs for referenced-but-not-defined
// ======================================================================

namespace oapi {
LRESULT GraphicsClient::RenderWndProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
INT_PTR GraphicsClient::LaunchpadVideoWndProc(HWND, UINT, WPARAM, LPARAM) { return FALSE; }
int GraphicsClient::clbkBeginBltGroup(SURFHANDLE) { return -2; }
int GraphicsClient::clbkEndBltGroup() { return -2; }
SURFHANDLE GraphicsClient::clbkCreateSurface(HBITMAP) { return nullptr; }
void GraphicsClient::clbkRender2DPanel(SURFHANDLE*, MESHHANDLE, MATRIX3*, bool) {}
void GraphicsClient::clbkRender2DPanel(SURFHANDLE*, MESHHANDLE, MATRIX3*, float, bool) {}
}

// ======================================================================
// oapiDebugString
// ======================================================================

char oapiDebugString_buf[256] = "";

#endif // !_WIN32
