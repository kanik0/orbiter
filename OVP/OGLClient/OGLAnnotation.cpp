// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLAnnotation.h"
#include "imgui.h"

namespace ogl {

OGLAnnotation::OGLAnnotation(oapi::GraphicsClient *gc)
	: oapi::ScreenAnnotation(gc)
{
}

OGLAnnotation::~OGLAnnotation() {}

void OGLAnnotation::Render()
{
	// The base class Render() handles text drawing via Sketchpad.
	// Call it directly — it uses the gc's sketchpad to draw.
	oapi::ScreenAnnotation::Render();
}

} // namespace ogl

#endif // !_WIN32
