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

class Orbiter;

namespace orbiter {

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

private:
	void HandleKeyEvent(const SDL_KeyboardEvent &key);
	void HandleMouseMotion(const SDL_MouseMotionEvent &motion);
	void HandleMouseButton(const SDL_MouseButtonEvent &button);
	void HandleMouseWheel(const SDL_MouseWheelEvent &wheel);
	void HandleWindowEvent(const SDL_WindowEvent &window);

	Orbiter *m_orbiter;
	SDL_Window *m_window;
	SDL_GLContext m_glContext;
	int m_width, m_height;
	bool m_initialized;
};

} // namespace orbiter

#endif // !_WIN32
#endif // __SDLPLATFORM_H
