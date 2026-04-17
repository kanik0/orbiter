// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLShaderMgr.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <functional>

namespace ogl {

ShaderMgr::ShaderMgr() {}

ShaderMgr::~ShaderMgr()
{
	ReleaseAll();
}

void ShaderMgr::SetShaderPath(const std::string &path)
{
	m_shaderPath = path;
	// Ensure trailing slash
	if (!m_shaderPath.empty() && m_shaderPath.back() != '/')
		m_shaderPath += '/';
}

GLuint ShaderMgr::LoadProgram(const char *name, const char *vertFile, const char *fragFile)
{
	auto it = m_programs.find(name);
	if (it != m_programs.end())
		return it->second;

	std::string vertSrc, fragSrc;
	if (!ReadShaderSource(vertFile, vertSrc)) {
		fprintf(stderr, "[ShaderMgr] Failed to read vertex shader: %s\n", vertFile);
		return 0;
	}
	if (!ReadShaderSource(fragFile, fragSrc)) {
		fprintf(stderr, "[ShaderMgr] Failed to read fragment shader: %s\n", fragFile);
		return 0;
	}

	GLuint vert = CompileShader(GL_VERTEX_SHADER, vertSrc.c_str(), vertFile);
	GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc.c_str(), fragFile);
	if (!vert || !frag) {
		if (vert) glDeleteShader(vert);
		if (frag) glDeleteShader(frag);
		return 0;
	}

	GLuint prog = LinkProgram(vert, frag, name);
	glDeleteShader(vert);
	glDeleteShader(frag);

	if (prog)
		m_programs[name] = prog;

	return prog;
}

GLint ShaderMgr::GetUniformLoc(GLuint program, const char *name)
{
	uint64_t key = ((uint64_t)program << 32) | (uint64_t)std::hash<std::string>{}(name);
	auto it = m_uniforms.find(key);
	if (it != m_uniforms.end())
		return it->second;

	GLint loc = glGetUniformLocation(program, name);
	m_uniforms[key] = loc;
	return loc;
}

void ShaderMgr::ReleaseAll()
{
	for (auto &kv : m_programs)
		glDeleteProgram(kv.second);
	m_programs.clear();
	m_uniforms.clear();
}

bool ShaderMgr::ReadShaderSource(const char *filename, std::string &outSource, int depth)
{
	if (depth > 10) {
		fprintf(stderr, "[ShaderMgr] #include depth exceeded for '%s'\n", filename);
		return false;
	}

	std::string fullPath = m_shaderPath + filename;
	std::ifstream file(fullPath);
	if (!file.is_open()) {
		fprintf(stderr, "[ShaderMgr] Cannot open shader file: %s\n", fullPath.c_str());
		return false;
	}

	std::string line;
	while (std::getline(file, line)) {
		// Process #include "filename" directives
		size_t pos = line.find("#include");
		if (pos != std::string::npos) {
			size_t q1 = line.find('"', pos);
			size_t q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
			if (q1 != std::string::npos && q2 != std::string::npos) {
				std::string incFile = line.substr(q1 + 1, q2 - q1 - 1);
				std::string incSrc;
				if (!ReadShaderSource(incFile.c_str(), incSrc, depth + 1))
					return false;
				outSource += incSrc;
				outSource += '\n';
				continue;
			}
		}
		outSource += line;
		outSource += '\n';
	}
	return true;
}

GLuint ShaderMgr::CompileShader(GLenum type, const char *source, const char *label)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char log[1024];
		glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
		fprintf(stderr, "[ShaderMgr] Compile error in '%s': %s\n", label, log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

GLuint ShaderMgr::LinkProgram(GLuint vert, GLuint frag, const char *label)
{
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);

	GLint success;
	glGetProgramiv(prog, GL_LINK_STATUS, &success);
	if (!success) {
		char log[1024];
		glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
		fprintf(stderr, "[ShaderMgr] Link error in '%s': %s\n", label, log);
		glDeleteProgram(prog);
		return 0;
	}

	fprintf(stderr, "[ShaderMgr] Loaded program '%s'\n", label);
	return prog;
}

} // namespace ogl

#endif // !_WIN32
