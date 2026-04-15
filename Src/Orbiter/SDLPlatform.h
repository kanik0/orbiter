// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// SDL2 Platform Layer for Orbiter on non-Windows platforms
// Provides window creation, event handling, and OpenGL context management

#ifndef __SDLPLATFORM_H
#define __SDLPLATFORM_H

#ifndef _WIN32

// SDL defines KMOD_* enum values that conflict with Orbiter's KMOD_* macros
// Temporarily undefine the Orbiter macros before including SDL
#ifdef KMOD_LSHIFT
#undef KMOD_LSHIFT
#undef KMOD_RSHIFT
#undef KMOD_SHIFT
#undef KMOD_LCTRL
#undef KMOD_RCTRL
#undef KMOD_CTRL
#undef KMOD_LALT
#undef KMOD_RALT
#undef KMOD_ALT
#endif

#include <SDL.h>
#include <vector>

class Orbiter;

namespace orbiter {

// Queued mouse event for consumption by Orbiter::UserInput
struct SDLMouseEvent {
	unsigned int event; // WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP
	unsigned long state; // MK_LBUTTON | MK_RBUTTON | MK_MBUTTON flags
	int x, y;
};

class SDLPlatform {
public:
	SDLPlatform(Orbiter *orbiter);
	~SDLPlatform();

	bool Initialize(int width = 1280, int height = 800);
	void Shutdown();

	// Event processing - returns false when quit is requested
	bool ProcessEvents();

	// Window management
	SDL_Window* GetWindow() const { return m_window; }
	SDL_GLContext GetGLContext() const { return m_glContext; }
	int GetWidth() const { return m_width; }
	int GetHeight() const { return m_height; }

	void SwapBuffers();

	// Input state - returns DirectInput-compatible key state array (256 bytes)
	// Non-zero (0x80) means key is pressed
	const char* GetKeyState() const { return m_kstate; }

	// Mouse state
	int GetMouseX() const { return m_mouseX; }
	int GetMouseY() const { return m_mouseY; }
	bool IsMouseButtonDown(int button) const { return m_mouseButtons[button]; }

	// Queued mouse events - consumed by Orbiter::UserInput each frame
	const std::vector<SDLMouseEvent>& GetMouseEvents() const { return m_mouseEvents; }
	void ClearMouseEvents() { m_mouseEvents.clear(); }

	// Mouse wheel accumulator - consumed each frame
	int GetWheelAccum() const { return m_wheelAccum; }
	void ClearWheelAccum() { m_wheelAccum = 0; }

	// Static SDL scancode to DirectInput keycode mapping
	static unsigned char SDLScancodeToDirectInput(SDL_Scancode sc);

private:
	void HandleKeyEvent(const SDL_KeyboardEvent &key);
	void HandleMouseMotion(const SDL_MouseMotionEvent &motion);
	void HandleMouseButton(const SDL_MouseButtonEvent &button);
	void HandleMouseWheel(const SDL_MouseWheelEvent &wheel);
	void HandleWindowEvent(const SDL_WindowEvent &window);
	void UpdateKeyStateFromSDL();

	Orbiter *m_orbiter;
	SDL_Window *m_window;
	SDL_GLContext m_glContext;
	int m_width, m_height;
	bool m_initialized;

	// Keyboard state in DirectInput format (index = DInput scancode, value = 0x80 if pressed)
	char m_kstate[256];

	// Mouse state
	int m_mouseX, m_mouseY;
	bool m_mouseButtons[3]; // left, right, middle
	int m_wheelAccum;        // accumulated scroll ticks since last frame

	// Queued mouse click events (consumed by Orbiter each frame)
	std::vector<SDLMouseEvent> m_mouseEvents;
};

} // namespace orbiter

#endif // !_WIN32
#endif // __SDLPLATFORM_H
