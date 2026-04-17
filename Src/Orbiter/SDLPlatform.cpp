// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// SDL2 Platform Layer implementation

#ifndef _WIN32

#include "SDLPlatform.h"
#include "Orbiter.h"
#include "Log.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include <OpenGL/gl.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// We need these Windows message constants for Orbiter's MouseEvent interface.
// They are defined in OrbiterPlatform.h but we replicate the values here
// to keep includes clean.
#ifndef WM_LBUTTONDOWN
#define WM_LBUTTONDOWN  0x0201
#define WM_LBUTTONUP    0x0202
#define WM_RBUTTONDOWN  0x0204
#define WM_RBUTTONUP    0x0205
#endif

// Windows MK_ mouse button flags (used by Orbiter's input system)
#ifndef MK_LBUTTON
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_MBUTTON 0x0010
#endif

// ============================================================================
// SDL Scancode -> DirectInput (OAPI_KEY) mapping table
// ============================================================================
// DirectInput scan codes match the original IBM PC keyboard scan codes.
// This table maps SDL_Scancode values to their DirectInput equivalents.

static unsigned char s_sdlToDInput[SDL_NUM_SCANCODES];
static bool s_mappingInitialized = false;

static void InitSDLToDInputMapping()
{
	if (s_mappingInitialized) return;
	memset(s_sdlToDInput, 0, sizeof(s_sdlToDInput));

	// Escape
	s_sdlToDInput[SDL_SCANCODE_ESCAPE]    = 0x01;

	// Number row
	s_sdlToDInput[SDL_SCANCODE_1]         = 0x02;
	s_sdlToDInput[SDL_SCANCODE_2]         = 0x03;
	s_sdlToDInput[SDL_SCANCODE_3]         = 0x04;
	s_sdlToDInput[SDL_SCANCODE_4]         = 0x05;
	s_sdlToDInput[SDL_SCANCODE_5]         = 0x06;
	s_sdlToDInput[SDL_SCANCODE_6]         = 0x07;
	s_sdlToDInput[SDL_SCANCODE_7]         = 0x08;
	s_sdlToDInput[SDL_SCANCODE_8]         = 0x09;
	s_sdlToDInput[SDL_SCANCODE_9]         = 0x0A;
	s_sdlToDInput[SDL_SCANCODE_0]         = 0x0B;
	s_sdlToDInput[SDL_SCANCODE_MINUS]     = 0x0C;
	s_sdlToDInput[SDL_SCANCODE_EQUALS]    = 0x0D;

	// Top row controls
	s_sdlToDInput[SDL_SCANCODE_BACKSPACE] = 0x0E;
	s_sdlToDInput[SDL_SCANCODE_TAB]       = 0x0F;

	// QWERTY row
	s_sdlToDInput[SDL_SCANCODE_Q]         = 0x10;
	s_sdlToDInput[SDL_SCANCODE_W]         = 0x11;
	s_sdlToDInput[SDL_SCANCODE_E]         = 0x12;
	s_sdlToDInput[SDL_SCANCODE_R]         = 0x13;
	s_sdlToDInput[SDL_SCANCODE_T]         = 0x14;
	s_sdlToDInput[SDL_SCANCODE_Y]         = 0x15;
	s_sdlToDInput[SDL_SCANCODE_U]         = 0x16;
	s_sdlToDInput[SDL_SCANCODE_I]         = 0x17;
	s_sdlToDInput[SDL_SCANCODE_O]         = 0x18;
	s_sdlToDInput[SDL_SCANCODE_P]         = 0x19;
	s_sdlToDInput[SDL_SCANCODE_LEFTBRACKET]  = 0x1A;
	s_sdlToDInput[SDL_SCANCODE_RIGHTBRACKET] = 0x1B;
	s_sdlToDInput[SDL_SCANCODE_RETURN]    = 0x1C;

	// Left control
	s_sdlToDInput[SDL_SCANCODE_LCTRL]     = 0x1D;
#ifdef __APPLE__
	// On macOS, Ctrl+arrow conflicts with Spaces/Mission Control.
	// Map Cmd (LGUI/RGUI) to Ctrl scancode so Cmd+arrows work for camera control.
	s_sdlToDInput[SDL_SCANCODE_LGUI]      = 0x1D;
	s_sdlToDInput[SDL_SCANCODE_RGUI]      = 0x9D;
#endif

	// ASDF row
	s_sdlToDInput[SDL_SCANCODE_A]         = 0x1E;
	s_sdlToDInput[SDL_SCANCODE_S]         = 0x1F;
	s_sdlToDInput[SDL_SCANCODE_D]         = 0x20;
	s_sdlToDInput[SDL_SCANCODE_F]         = 0x21;
	s_sdlToDInput[SDL_SCANCODE_G]         = 0x22;
	s_sdlToDInput[SDL_SCANCODE_H]         = 0x23;
	s_sdlToDInput[SDL_SCANCODE_J]         = 0x24;
	s_sdlToDInput[SDL_SCANCODE_K]         = 0x25;
	s_sdlToDInput[SDL_SCANCODE_L]         = 0x26;
	s_sdlToDInput[SDL_SCANCODE_SEMICOLON] = 0x27;
	s_sdlToDInput[SDL_SCANCODE_APOSTROPHE]= 0x28;
	s_sdlToDInput[SDL_SCANCODE_GRAVE]     = 0x29;

	// Left shift + ZXCV row
	s_sdlToDInput[SDL_SCANCODE_LSHIFT]    = 0x2A;
	s_sdlToDInput[SDL_SCANCODE_BACKSLASH] = 0x2B;
	s_sdlToDInput[SDL_SCANCODE_Z]         = 0x2C;
	s_sdlToDInput[SDL_SCANCODE_X]         = 0x2D;
	s_sdlToDInput[SDL_SCANCODE_C]         = 0x2E;
	s_sdlToDInput[SDL_SCANCODE_V]         = 0x2F;
	s_sdlToDInput[SDL_SCANCODE_B]         = 0x30;
	s_sdlToDInput[SDL_SCANCODE_N]         = 0x31;
	s_sdlToDInput[SDL_SCANCODE_M]         = 0x32;
	s_sdlToDInput[SDL_SCANCODE_COMMA]     = 0x33;
	s_sdlToDInput[SDL_SCANCODE_PERIOD]    = 0x34;
	s_sdlToDInput[SDL_SCANCODE_SLASH]     = 0x35;

	// Right shift
	s_sdlToDInput[SDL_SCANCODE_RSHIFT]    = 0x36;

	// Keypad multiply
	s_sdlToDInput[SDL_SCANCODE_KP_MULTIPLY] = 0x37;

	// Left alt, space, capslock
	s_sdlToDInput[SDL_SCANCODE_LALT]      = 0x38;
	s_sdlToDInput[SDL_SCANCODE_SPACE]     = 0x39;
	s_sdlToDInput[SDL_SCANCODE_CAPSLOCK]  = 0x3A;

	// Function keys F1-F10
	s_sdlToDInput[SDL_SCANCODE_F1]        = 0x3B;
	s_sdlToDInput[SDL_SCANCODE_F2]        = 0x3C;
	s_sdlToDInput[SDL_SCANCODE_F3]        = 0x3D;
	s_sdlToDInput[SDL_SCANCODE_F4]        = 0x3E;
	s_sdlToDInput[SDL_SCANCODE_F5]        = 0x3F;
	s_sdlToDInput[SDL_SCANCODE_F6]        = 0x40;
	s_sdlToDInput[SDL_SCANCODE_F7]        = 0x41;
	s_sdlToDInput[SDL_SCANCODE_F8]        = 0x42;
	s_sdlToDInput[SDL_SCANCODE_F9]        = 0x43;
	s_sdlToDInput[SDL_SCANCODE_F10]       = 0x44;

	// Numlock, Scrolllock
	s_sdlToDInput[SDL_SCANCODE_NUMLOCKCLEAR] = 0x45;
	s_sdlToDInput[SDL_SCANCODE_SCROLLLOCK]   = 0x46;

	// Numpad 7-9
	s_sdlToDInput[SDL_SCANCODE_KP_7]      = 0x47;
	s_sdlToDInput[SDL_SCANCODE_KP_8]      = 0x48;
	s_sdlToDInput[SDL_SCANCODE_KP_9]      = 0x49;

	// Numpad subtract
	s_sdlToDInput[SDL_SCANCODE_KP_MINUS]  = 0x4A;

	// Numpad 4-6
	s_sdlToDInput[SDL_SCANCODE_KP_4]      = 0x4B;
	s_sdlToDInput[SDL_SCANCODE_KP_5]      = 0x4C;
	s_sdlToDInput[SDL_SCANCODE_KP_6]      = 0x4D;

	// Numpad add
	s_sdlToDInput[SDL_SCANCODE_KP_PLUS]   = 0x4E;

	// Numpad 1-3
	s_sdlToDInput[SDL_SCANCODE_KP_1]      = 0x4F;
	s_sdlToDInput[SDL_SCANCODE_KP_2]      = 0x50;
	s_sdlToDInput[SDL_SCANCODE_KP_3]      = 0x51;

	// Numpad 0, decimal
	s_sdlToDInput[SDL_SCANCODE_KP_0]      = 0x52;
	s_sdlToDInput[SDL_SCANCODE_KP_PERIOD] = 0x53;

	// OEM_102 (extra key on non-US keyboards)
	s_sdlToDInput[SDL_SCANCODE_NONUSBACKSLASH] = 0x56;

	// F11, F12
	s_sdlToDInput[SDL_SCANCODE_F11]       = 0x57;
	s_sdlToDInput[SDL_SCANCODE_F12]       = 0x58;

	// Numpad enter
	s_sdlToDInput[SDL_SCANCODE_KP_ENTER]  = 0x9C;

	// Right control
	s_sdlToDInput[SDL_SCANCODE_RCTRL]     = 0x9D;

	// Numpad divide
	s_sdlToDInput[SDL_SCANCODE_KP_DIVIDE] = 0xB5;

	// Print screen / SysRq
	s_sdlToDInput[SDL_SCANCODE_PRINTSCREEN] = 0xB7;

	// Right alt
	s_sdlToDInput[SDL_SCANCODE_RALT]      = 0xB8;

	// Pause/Break
	s_sdlToDInput[SDL_SCANCODE_PAUSE]     = 0xC5;

	// Cursor / navigation keys
	s_sdlToDInput[SDL_SCANCODE_HOME]      = 0xC7;
	s_sdlToDInput[SDL_SCANCODE_UP]        = 0xC8;
	s_sdlToDInput[SDL_SCANCODE_PAGEUP]    = 0xC9;
	s_sdlToDInput[SDL_SCANCODE_LEFT]      = 0xCB;
	s_sdlToDInput[SDL_SCANCODE_RIGHT]     = 0xCD;
	s_sdlToDInput[SDL_SCANCODE_END]       = 0xCF;
	s_sdlToDInput[SDL_SCANCODE_DOWN]      = 0xD0;
	s_sdlToDInput[SDL_SCANCODE_PAGEDOWN]  = 0xD1;
	s_sdlToDInput[SDL_SCANCODE_INSERT]    = 0xD2;
	s_sdlToDInput[SDL_SCANCODE_DELETE]    = 0xD3;

	s_mappingInitialized = true;
}

namespace orbiter {

// Static method
unsigned char SDLPlatform::SDLScancodeToDirectInput(SDL_Scancode sc)
{
	InitSDLToDInputMapping();
	if (sc >= 0 && sc < SDL_NUM_SCANCODES)
		return s_sdlToDInput[sc];
	return 0;
}

SDLPlatform::SDLPlatform(Orbiter *orbiter)
	: m_orbiter(orbiter), m_window(nullptr), m_glContext(nullptr),
	  m_width(1280), m_height(800), m_initialized(false),
	  m_mouseX(0), m_mouseY(0), m_wheelAccum(0),
	  m_gameController(nullptr)
{
	memset(m_kstate, 0, sizeof(m_kstate));
	memset(m_mouseButtons, 0, sizeof(m_mouseButtons));
	memset(&m_joy, 0, sizeof(m_joy));
	m_joy.hatAngle = 0xFFFF; // centered
	InitSDLToDInputMapping();
}

SDLPlatform::~SDLPlatform()
{
	Shutdown();
}

bool SDLPlatform::Initialize(int width, int height)
{
	m_width = width;
	m_height = height;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) < 0) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return false;
	}

	// Request OpenGL 4.1 (highest supported on macOS)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	m_window = SDL_CreateWindow(
		"Orbiter Space Flight Simulator",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		m_width, m_height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
	);

	if (!m_window) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_Quit();
		return false;
	}

	m_glContext = SDL_GL_CreateContext(m_window);
	if (!m_glContext) {
		fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
		SDL_Quit();
		return false;
	}

	// Enable vsync
	SDL_GL_SetSwapInterval(1);

	// Get actual drawable size (may differ from window size on Retina)
	int drawW, drawH;
	SDL_GL_GetDrawableSize(m_window, &drawW, &drawH);

	fprintf(stderr, "[SDLPlatform] Window: %dx%d, Drawable: %dx%d\n",
		m_width, m_height, drawW, drawH);
	fprintf(stderr, "[SDLPlatform] OpenGL vendor: %s\n", glGetString(GL_VENDOR));
	fprintf(stderr, "[SDLPlatform] OpenGL renderer: %s\n", glGetString(GL_RENDERER));
	fprintf(stderr, "[SDLPlatform] OpenGL version: %s\n", glGetString(GL_VERSION));

	// Open the first available game controller
	int numJoy = SDL_NumJoysticks();
	for (int i = 0; i < numJoy; i++) {
		if (SDL_IsGameController(i)) {
			m_gameController = SDL_GameControllerOpen(i);
			if (m_gameController) {
				m_joy.connected = true;
				fprintf(stderr, "[SDLPlatform] GameController: %s\n",
					SDL_GameControllerName(m_gameController));
				break;
			}
		}
	}

	m_initialized = true;
	return true;
}

void SDLPlatform::Shutdown()
{
	if (m_gameController) {
		SDL_GameControllerClose(m_gameController);
		m_gameController = nullptr;
		m_joy.connected = false;
	}
	if (m_glContext) {
		SDL_GL_DeleteContext(m_glContext);
		m_glContext = nullptr;
	}
	if (m_window) {
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
	}
	if (m_initialized) {
		SDL_Quit();
		m_initialized = false;
	}
}

bool SDLPlatform::ProcessEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// Forward events to ImGui SDL2 backend (only if ImGui is initialized)
		if (ImGui::GetCurrentContext())
			ImGui_ImplSDL2_ProcessEvent(&event);

		switch (event.type) {
		case SDL_QUIT:
			return false;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			HandleKeyEvent(event.key);
			break;
		case SDL_MOUSEMOTION:
			HandleMouseMotion(event.motion);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			HandleMouseButton(event.button);
			break;
		case SDL_MOUSEWHEEL:
			HandleMouseWheel(event.wheel);
			break;
		case SDL_WINDOWEVENT:
			HandleWindowEvent(event.window);
			break;
		}
	}

	// Update key state from SDL's continuous keyboard state each frame.
	// This catches keys held across frames even if no event fires.
	UpdateKeyStateFromSDL();
	UpdateJoystick();

	return true;
}

void SDLPlatform::SwapBuffers()
{
	if (m_window) {
		SDL_GL_SwapWindow(m_window);
	}
}

void SDLPlatform::UpdateKeyStateFromSDL()
{
	// Merge continuous SDL keyboard state into our DInput-format array.
	// Start fresh each frame (like DirectInput GetDeviceState).
	memset(m_kstate, 0, sizeof(m_kstate));

	int numkeys = 0;
	const Uint8 *sdlState = SDL_GetKeyboardState(&numkeys);
	if (!sdlState) return;

	for (int sc = 0; sc < numkeys && sc < SDL_NUM_SCANCODES; sc++) {
		if (sdlState[sc]) {
			unsigned char dik = s_sdlToDInput[sc];
			if (dik != 0) {
				m_kstate[dik] = (char)0x80;
			}
		}
	}
}

void SDLPlatform::UpdateJoystick()
{
	if (!m_gameController || !m_joy.connected) return;

	// Apply deadzone (SDL axis range is -32768..32767, Orbiter uses -1000..1000)
	auto mapAxis = [](int raw) -> int {
		const int deadzone = 4000;
		if (abs(raw) < deadzone) return 0;
		int sign = (raw > 0) ? 1 : -1;
		return sign * (abs(raw) - deadzone) * 1000 / (32767 - deadzone);
	};

	m_joy.lX  = mapAxis(SDL_GameControllerGetAxis(m_gameController, SDL_CONTROLLER_AXIS_LEFTX));
	m_joy.lY  = mapAxis(SDL_GameControllerGetAxis(m_gameController, SDL_CONTROLLER_AXIS_LEFTY));
	m_joy.lRx = mapAxis(SDL_GameControllerGetAxis(m_gameController, SDL_CONTROLLER_AXIS_RIGHTX));
	m_joy.lRy = mapAxis(SDL_GameControllerGetAxis(m_gameController, SDL_CONTROLLER_AXIS_RIGHTY));
	// Triggers: 0..32767 → map to 0..1000
	m_joy.lZ  = SDL_GameControllerGetAxis(m_gameController, SDL_CONTROLLER_AXIS_TRIGGERLEFT)  * 1000 / 32767;
	m_joy.lRz = SDL_GameControllerGetAxis(m_gameController, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) * 1000 / 32767;

	// D-pad to POV hat angle (0=up, 9000=right, 18000=down, 27000=left, 0xFFFF=centered)
	bool up    = SDL_GameControllerGetButton(m_gameController, SDL_CONTROLLER_BUTTON_DPAD_UP);
	bool down  = SDL_GameControllerGetButton(m_gameController, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	bool left  = SDL_GameControllerGetButton(m_gameController, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
	bool right = SDL_GameControllerGetButton(m_gameController, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
	if      (up && right)    m_joy.hatAngle = 4500;
	else if (right && down)  m_joy.hatAngle = 13500;
	else if (down && left)   m_joy.hatAngle = 22500;
	else if (left && up)     m_joy.hatAngle = 31500;
	else if (up)             m_joy.hatAngle = 0;
	else if (right)          m_joy.hatAngle = 9000;
	else if (down)           m_joy.hatAngle = 18000;
	else if (left)           m_joy.hatAngle = 27000;
	else                     m_joy.hatAngle = 0xFFFF;

	// Buttons
	for (int i = 0; i < 16 && i < SDL_CONTROLLER_BUTTON_MAX; i++)
		m_joy.buttons[i] = SDL_GameControllerGetButton(m_gameController, (SDL_GameControllerButton)i);
}

void SDLPlatform::HandleKeyEvent(const SDL_KeyboardEvent &key)
{
	// Event-based key tracking is handled by UpdateKeyStateFromSDL() each frame.
	// This handler is kept for potential future buffered-event needs.
}

void SDLPlatform::HandleMouseMotion(const SDL_MouseMotionEvent &motion)
{
	m_mouseX = motion.x;
	m_mouseY = motion.y;
}

void SDLPlatform::HandleMouseButton(const SDL_MouseButtonEvent &button)
{
	bool pressed = (button.type == SDL_MOUSEBUTTONDOWN);

	switch (button.button) {
	case SDL_BUTTON_LEFT:
		m_mouseButtons[0] = pressed;
		break;
	case SDL_BUTTON_RIGHT:
		m_mouseButtons[1] = pressed;
		break;
	case SDL_BUTTON_MIDDLE:
		m_mouseButtons[2] = pressed;
		break;
	}

	m_mouseX = button.x;
	m_mouseY = button.y;

	// Skip mouse events if ImGui wants to capture them
	if (ImGui::GetIO().WantCaptureMouse) return;

	// Queue the event for Orbiter::UserInput to process
	unsigned int event = 0;
	if (button.button == SDL_BUTTON_LEFT) {
		event = pressed ? WM_LBUTTONDOWN : WM_LBUTTONUP;
	} else if (button.button == SDL_BUTTON_RIGHT) {
		event = pressed ? WM_RBUTTONDOWN : WM_RBUTTONUP;
	}
	if (event) {
		unsigned long state = 0;
		if (m_mouseButtons[0]) state |= MK_LBUTTON;
		if (m_mouseButtons[1]) state |= MK_RBUTTON;
		if (m_mouseButtons[2]) state |= MK_MBUTTON;
		m_mouseEvents.push_back({event, state, button.x, button.y});
	}
}

void SDLPlatform::HandleMouseWheel(const SDL_MouseWheelEvent &wheel)
{
	if (ImGui::GetIO().WantCaptureMouse) return;

	// Accumulate wheel ticks for consumption by Orbiter::UserInput
	m_wheelAccum += wheel.y;
}

void SDLPlatform::HandleWindowEvent(const SDL_WindowEvent &window)
{
	switch (window.event) {
	case SDL_WINDOWEVENT_RESIZED:
		m_width = window.data1;
		m_height = window.data2;
		break;
	case SDL_WINDOWEVENT_CLOSE:
		// Will trigger SDL_QUIT
		break;
	}
}

} // namespace orbiter

#endif // !_WIN32
