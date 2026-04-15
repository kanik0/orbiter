// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLShaderMgr - Shader compilation, caching, and management for OGLClient
// Loads GLSL shaders from external files with #include support

#ifndef __OGLSHADERMGR_H
#define __OGLSHADERMGR_H

#ifndef _WIN32
#include <OpenGL/gl3.h>
#include <string>
#include <map>

namespace ogl {

class ShaderMgr {
public:
	ShaderMgr();
	~ShaderMgr();

	// Set the base directory for shader files (e.g., "shaders/")
	void SetShaderPath(const std::string &path);

	// Load and link a shader program from vertex + fragment shader files.
	// Returns a GL program ID, or 0 on failure.
	// Results are cached by name — subsequent calls with the same name return
	// the cached program without recompilation.
	GLuint LoadProgram(const char *name, const char *vertFile, const char *fragFile);

	// Get uniform location (thin wrapper with caching)
	GLint GetUniformLoc(GLuint program, const char *name);

	// Release all cached programs
	void ReleaseAll();

private:
	std::string m_shaderPath;

	// Cached programs: key = program name
	std::map<std::string, GLuint> m_programs;

	// Cached uniform locations: key = (program << 16 | hash(name))
	std::map<uint64_t, GLint> m_uniforms;

	// Read file contents, processing #include "file" directives
	bool ReadShaderSource(const char *filename, std::string &outSource, int depth = 0);

	// Compile a single shader stage
	GLuint CompileShader(GLenum type, const char *source, const char *label);

	// Link a program from vertex + fragment shaders
	GLuint LinkProgram(GLuint vert, GLuint frag, const char *label);
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLSHADERMGR_H
