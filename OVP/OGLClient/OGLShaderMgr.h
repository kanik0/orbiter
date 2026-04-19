// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLShaderMgr - Shader compilation, caching and management for OGLClient.
//
// Features:
//  - GLSL #include "file" directive with a dedicated sub-directory search
//    (files named *.glsl.inc live under <shaderPath>/include/).
//  - #pragma once deduplication across include chain.
//  - #line directives injected into the expanded source so compiler errors
//    reference the original file/line.
//  - Dependency tracking per program for debug hot-reload (mtime polling).
//  - Uniform-block introspection + centralised UBO binding-point registry.
//  - Diagnostic LogStatus() that lists every program, its deps and its
//    uniform blocks — invoked once at the end of client init.

#ifndef __OGLSHADERMGR_H
#define __OGLSHADERMGR_H

#ifndef _WIN32
#include <OpenGL/gl3.h>
#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include <ctime>
#include <cstdint>

namespace ogl {

// Well-known UBO binding points shared across the OGLClient shader library.
// Keep these in sync with shaders/include/*.glsl.inc block declarations.
namespace UBO {
	constexpr GLuint Camera   = 0;   // view/proj + camera pos
	constexpr GLuint Light    = 1;   // sun direction + colour + ambient
	constexpr GLuint Material = 2;   // PBR material parameters
	constexpr GLuint Scatter  = 3;   // atmospheric scattering constants
	constexpr GLuint Time     = 4;   // time + frame counter
}

class ShaderMgr {
public:
	ShaderMgr();
	~ShaderMgr();

	// Location of *.vert/*.frag files. Trailing slash auto-appended.
	void SetShaderPath(const std::string &path);

	// Enable/disable hot-reload polling. Default: on in debug builds
	// (NDEBUG not defined), off otherwise. Cadence is controlled by
	// CheckReload(): every kReloadPollFrames frames it stats each dep.
	void SetHotReloadEnabled(bool enable);
	bool IsHotReloadEnabled() const { return m_hotReloadEnabled; }

	// Load+link a program from vertex+fragment shader files. Results cached
	// by logical name; a second call with the same name returns the cached
	// program without touching disk (outside hot-reload).
	GLuint LoadProgram(const char *name, const char *vertFile, const char *fragFile);

	// Scalar-uniform location lookup with caching.
	GLint GetUniformLoc(GLuint program, const char *name);

	// --- Uniform block (UBO) registry ---
	// Associate a named uniform block with a fixed binding point. Call before
	// LoadProgram for the binding to be applied automatically, or any time to
	// rebind. Values beyond the constants in namespace UBO are caller-defined.
	void RegisterUBOBinding(const std::string &blockName, GLuint bindingPoint);

	// Inspect a linked program, and for every active uniform block whose name
	// matches a registered binding, call glUniformBlockBinding. Returns the
	// number of blocks bound. Unknown blocks are logged and left untouched.
	int BindUBOBlocks(GLuint program, const char *programName);

	// Create (or re-create) a GL_UNIFORM_BUFFER of the requested size bound to
	// the given binding point via glBindBufferBase. The returned GLuint is the
	// buffer object; caller owns it and must pass it to ReleaseUBO() on shutdown.
	GLuint CreateUBO(GLuint bindingPoint, GLsizeiptr sizeBytes);

	// Push new data into a previously created UBO. `sizeBytes` must match the
	// size passed to CreateUBO (subranges are not supported here).
	void UpdateUBO(GLuint ubo, GLsizeiptr sizeBytes, const void *data);

	// Delete a UBO returned by CreateUBO and clear our tracking of it.
	void ReleaseUBO(GLuint ubo);

	// Poll mtime of every shader dependency and reload affected programs.
	// Cheap: bumps an internal frame counter, stat()s deps only every
	// kReloadPollFrames frames. No-op when hot-reload is disabled.
	void CheckReload();

	// Emit a single-shot summary of every loaded program: logical name, source
	// files (+ mtime), active uniform blocks, scalar uniform count. Safe to
	// call repeatedly; only the first call produces output. Call after the
	// client finished initialising its shader set.
	void LogStatus();

	// Release every cached program, UBO and dependency record.
	void ReleaseAll();

private:
	struct SourceDep {
		std::string path;        // absolute path to source file
		time_t      mtime;       // last-modification stamp when read
	};

	struct ProgramEntry {
		std::string name;
		std::string vertFile;
		std::string fragFile;
		GLuint      program;
		std::vector<SourceDep>   deps;
		std::vector<std::string> uniformBlocks;
	};

	// Frames between two hot-reload scans (kept small: stat is cheap).
	static constexpr int kReloadPollFrames = 30;
	static constexpr int kMaxIncludeDepth  = 10;

	std::string m_shaderPath;
	bool        m_hotReloadEnabled;
	int         m_reloadCounter;
	bool        m_statusLogged;

	// Key = logical program name.
	std::map<std::string, ProgramEntry> m_programs;

	// Uniform-location cache. Key is a (program, name) pair so different
	// programs that share uniform names (e.g. "uAlpha") never alias.
	// Invalidated per-program on hot-reload. The previous uint64-packed
	// key OR-merged the hash into the program slot and returned stale
	// locations across programs — regressed panel2d (#37).
	std::map<std::pair<GLuint, std::string>, GLint> m_uniforms;

	// UBO block-name → binding point. Populated via RegisterUBOBinding()
	// plus defaults seeded in the constructor.
	std::map<std::string, GLuint> m_uboBindings;

	// UBOs created through CreateUBO(), tracked only for LogStatus()
	// and ReleaseAll() cleanup.
	std::map<GLuint, GLuint> m_ubos;  // buffer → binding point

	// Read a shader file, expanding #include "file" directives. `deps` is
	// appended with every file touched (including nested includes), each
	// with its mtime; `included` is a set of already-included files (for
	// #pragma once dedup across the whole chain). `outSource` receives the
	// assembled GLSL, with #line directives injected for diagnostics.
	bool ReadShaderSource(const std::string &filename,
	                      std::string &outSource,
	                      std::vector<SourceDep> &deps,
	                      std::unordered_set<std::string> &included,
	                      int depth);

	// Resolve an include filename: first try <shaderPath>/include/<file>,
	// then <shaderPath>/<file>. Returns empty on miss.
	std::string ResolveInclude(const std::string &file) const;

	GLuint CompileShader(GLenum type, const char *source, const char *label);
	GLuint LinkProgram(GLuint vert, GLuint frag, const char *label);
	void   IntrospectUniformBlocks(ProgramEntry &entry);

	// Rebuild program from disk using entry.vertFile/fragFile. On success the
	// old GL program is deleted and entry is mutated. On failure entry is
	// untouched and the function returns false.
	bool ReloadProgram(ProgramEntry &entry);

	static time_t GetMTime(const std::string &path);

	// Default constructor seeds m_uboBindings with the UBO:: constants above.
	void SeedDefaultUBOBindings();
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLSHADERMGR_H
