// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// SDL2 Platform Layer implementation

#ifndef _WIN32

#include "SDLPlatform.h"
#include "Orbiter.h"
#include "Log.h"
#include <OpenGL/gl.h>
#include <cstdio>

namespace orbiter {

SDLPlatform::SDLPlatform(Orbiter *orbiter)
	: m_orbiter(orbiter), m_window(nullptr), m_glContext(nullptr),
	  m_width(1280), m_height(800), m_initialized(false)
{
}

SDLPlatform::~SDLPlatform()
{
	Shutdown();
}

bool SDLPlatform::Initialize(int width, int height)
{
	m_width = width;
	m_height = height;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
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

	m_initialized = true;
	return true;
}

void SDLPlatform::Shutdown()
{
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
	return true;
}

void SDLPlatform::SwapBuffers()
{
	if (m_window) {
		SDL_GL_SwapWindow(m_window);
	}
}

void SDLPlatform::HandleKeyEvent(const SDL_KeyboardEvent &key)
{
	// TODO: Map SDL keycodes to Orbiter key codes and dispatch
}

void SDLPlatform::HandleMouseMotion(const SDL_MouseMotionEvent &motion)
{
	// TODO: Forward to Orbiter camera/panel input
}

void SDLPlatform::HandleMouseButton(const SDL_MouseButtonEvent &button)
{
	// TODO: Forward to Orbiter input
}

void SDLPlatform::HandleMouseWheel(const SDL_MouseWheelEvent &wheel)
{
	// TODO: Forward to Orbiter camera zoom
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
