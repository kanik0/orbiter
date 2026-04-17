// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLAnnotation - On-screen text annotation overlay

#ifndef __OGLANNOTATION_H
#define __OGLANNOTATION_H

#ifndef _WIN32
#include "GraphicsAPI.h"

namespace ogl {

class OGLAnnotation : public oapi::ScreenAnnotation {
public:
	OGLAnnotation(oapi::GraphicsClient *gc);
	~OGLAnnotation() override;

	void Render() override;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLANNOTATION_H
