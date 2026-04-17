// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLShaderMgr.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <functional>
#include <sys/stat.h>

namespace ogl {

ShaderMgr::ShaderMgr()
	: m_hotReloadEnabled(
#ifdef NDEBUG
		false
#else
		true
#endif
	)
	, m_reloadCounter(0)
	, m_statusLogged(false)
{
	SeedDefaultUBOBindings();
}

ShaderMgr::~ShaderMgr()
{
	ReleaseAll();
}

void ShaderMgr::SeedDefaultUBOBindings()
{
	m_uboBindings["Camera"]   = UBO::Camera;
	m_uboBindings["Light"]    = UBO::Light;
	m_uboBindings["Material"] = UBO::Material;
	m_uboBindings["Scatter"]  = UBO::Scatter;
	m_uboBindings["Time"]     = UBO::Time;
}

void ShaderMgr::SetShaderPath(const std::string &path)
{
	m_shaderPath = path;
	if (!m_shaderPath.empty() && m_shaderPath.back() != '/')
		m_shaderPath += '/';
}

void ShaderMgr::SetHotReloadEnabled(bool enable)
{
	m_hotReloadEnabled = enable;
}

GLuint ShaderMgr::LoadProgram(const char *name, const char *vertFile, const char *fragFile)
{
	auto it = m_programs.find(name);
	if (it != m_programs.end())
		return it->second.program;

	ProgramEntry entry;
	entry.name     = name;
	entry.vertFile = vertFile;
	entry.fragFile = fragFile;
	entry.program  = 0;

	std::string vertSrc, fragSrc;
	std::unordered_set<std::string> includedV, includedF;

	if (!ReadShaderSource(vertFile, vertSrc, entry.deps, includedV, 0)) {
		fprintf(stderr, "[ShaderMgr] Failed to read vertex shader: %s\n", vertFile);
		return 0;
	}
	if (!ReadShaderSource(fragFile, fragSrc, entry.deps, includedF, 0)) {
		fprintf(stderr, "[ShaderMgr] Failed to read fragment shader: %s\n", fragFile);
		return 0;
	}

	GLuint vert = CompileShader(GL_VERTEX_SHADER,   vertSrc.c_str(), vertFile);
	GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc.c_str(), fragFile);
	if (!vert || !frag) {
		if (vert) glDeleteShader(vert);
		if (frag) glDeleteShader(frag);
		return 0;
	}

	GLuint prog = LinkProgram(vert, frag, name);
	glDeleteShader(vert);
	glDeleteShader(frag);
	if (!prog)
		return 0;

	entry.program = prog;
	IntrospectUniformBlocks(entry);

	// Bind any introspected block whose name matches the registry.
	for (const auto &blockName : entry.uniformBlocks) {
		auto bi = m_uboBindings.find(blockName);
		if (bi == m_uboBindings.end())
			continue;
		GLuint idx = glGetUniformBlockIndex(prog, blockName.c_str());
		if (idx != GL_INVALID_INDEX)
			glUniformBlockBinding(prog, idx, bi->second);
	}

	m_programs[name] = std::move(entry);
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

void ShaderMgr::RegisterUBOBinding(const std::string &blockName, GLuint bindingPoint)
{
	m_uboBindings[blockName] = bindingPoint;

	// Re-bind the block on every program that already advertised it.
	for (auto &kv : m_programs) {
		ProgramEntry &entry = kv.second;
		for (const auto &bn : entry.uniformBlocks) {
			if (bn == blockName) {
				GLuint idx = glGetUniformBlockIndex(entry.program, blockName.c_str());
				if (idx != GL_INVALID_INDEX)
					glUniformBlockBinding(entry.program, idx, bindingPoint);
			}
		}
	}
}

int ShaderMgr::BindUBOBlocks(GLuint program, const char *programName)
{
	auto it = m_programs.find(programName ? programName : "");
	if (it == m_programs.end())
		return 0;

	int bound = 0;
	for (const auto &blockName : it->second.uniformBlocks) {
		auto bi = m_uboBindings.find(blockName);
		if (bi == m_uboBindings.end()) {
			fprintf(stderr, "[ShaderMgr] Program '%s' uses unregistered UBO '%s'\n",
			        programName, blockName.c_str());
			continue;
		}
		GLuint idx = glGetUniformBlockIndex(program, blockName.c_str());
		if (idx != GL_INVALID_INDEX) {
			glUniformBlockBinding(program, idx, bi->second);
			++bound;
		}
	}
	return bound;
}

GLuint ShaderMgr::CreateUBO(GLuint bindingPoint, GLsizeiptr sizeBytes)
{
	GLuint ubo = 0;
	glGenBuffers(1, &ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeBytes, nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	m_ubos[ubo] = bindingPoint;
	return ubo;
}

void ShaderMgr::UpdateUBO(GLuint ubo, GLsizeiptr sizeBytes, const void *data)
{
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeBytes, data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void ShaderMgr::ReleaseUBO(GLuint ubo)
{
	auto it = m_ubos.find(ubo);
	if (it == m_ubos.end())
		return;
	glDeleteBuffers(1, &ubo);
	m_ubos.erase(it);
}

void ShaderMgr::CheckReload()
{
	if (!m_hotReloadEnabled)
		return;
	if (++m_reloadCounter < kReloadPollFrames)
		return;
	m_reloadCounter = 0;

	for (auto &kv : m_programs) {
		ProgramEntry &entry = kv.second;
		bool dirty = false;
		for (const auto &dep : entry.deps) {
			time_t now = GetMTime(dep.path);
			if (now != 0 && now != dep.mtime) {
				dirty = true;
				break;
			}
		}
		if (!dirty)
			continue;

		fprintf(stderr, "[ShaderMgr] Hot-reload '%s'...\n", entry.name.c_str());
		if (ReloadProgram(entry))
			fprintf(stderr, "[ShaderMgr] Hot-reload '%s' OK (program=%u)\n",
			        entry.name.c_str(), entry.program);
		else
			fprintf(stderr, "[ShaderMgr] Hot-reload '%s' FAILED, keeping previous program\n",
			        entry.name.c_str());
	}
}

bool ShaderMgr::ReloadProgram(ProgramEntry &entry)
{
	std::string vertSrc, fragSrc;
	std::vector<SourceDep> newDeps;
	std::unordered_set<std::string> includedV, includedF;

	if (!ReadShaderSource(entry.vertFile.c_str(), vertSrc, newDeps, includedV, 0))
		return false;
	if (!ReadShaderSource(entry.fragFile.c_str(), fragSrc, newDeps, includedF, 0))
		return false;

	GLuint vert = CompileShader(GL_VERTEX_SHADER,   vertSrc.c_str(), entry.vertFile.c_str());
	GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc.c_str(), entry.fragFile.c_str());
	if (!vert || !frag) {
		if (vert) glDeleteShader(vert);
		if (frag) glDeleteShader(frag);
		return false;
	}

	GLuint prog = LinkProgram(vert, frag, entry.name.c_str());
	glDeleteShader(vert);
	glDeleteShader(frag);
	if (!prog)
		return false;

	// Success: swap in the new program, drop cached uniform locations
	// (locations are program-specific so they must be re-queried), and
	// re-apply UBO bindings.
	GLuint old = entry.program;

	// Remove uniform cache entries that belonged to the old program.
	for (auto it = m_uniforms.begin(); it != m_uniforms.end(); ) {
		if ((GLuint)(it->first >> 32) == old)
			it = m_uniforms.erase(it);
		else
			++it;
	}

	glDeleteProgram(old);
	entry.program = prog;
	entry.deps    = std::move(newDeps);
	entry.uniformBlocks.clear();
	IntrospectUniformBlocks(entry);

	for (const auto &blockName : entry.uniformBlocks) {
		auto bi = m_uboBindings.find(blockName);
		if (bi == m_uboBindings.end())
			continue;
		GLuint idx = glGetUniformBlockIndex(prog, blockName.c_str());
		if (idx != GL_INVALID_INDEX)
			glUniformBlockBinding(prog, idx, bi->second);
	}
	return true;
}

void ShaderMgr::LogStatus()
{
	if (m_statusLogged)
		return;
	m_statusLogged = true;

	fprintf(stderr, "[ShaderMgr] ===== Loaded %zu shader programs =====\n",
	        m_programs.size());
	for (const auto &kv : m_programs) {
		const ProgramEntry &e = kv.second;
		fprintf(stderr, "  '%s' (program=%u)\n", e.name.c_str(), e.program);
		fprintf(stderr, "    vert: %s\n", e.vertFile.c_str());
		fprintf(stderr, "    frag: %s\n", e.fragFile.c_str());
		for (const auto &d : e.deps)
			fprintf(stderr, "    dep : %s (mtime=%ld)\n",
			        d.path.c_str(), (long)d.mtime);
		if (!e.uniformBlocks.empty()) {
			fprintf(stderr, "    UBOs:");
			for (const auto &bn : e.uniformBlocks) {
				auto bi = m_uboBindings.find(bn);
				if (bi != m_uboBindings.end())
					fprintf(stderr, " %s@%u", bn.c_str(), bi->second);
				else
					fprintf(stderr, " %s@?", bn.c_str());
			}
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "[ShaderMgr] Hot-reload: %s (%d frame poll)\n",
	        m_hotReloadEnabled ? "ENABLED" : "disabled", kReloadPollFrames);
	fprintf(stderr, "[ShaderMgr] ==========================================\n");
}

void ShaderMgr::ReleaseAll()
{
	for (auto &kv : m_programs)
		if (kv.second.program)
			glDeleteProgram(kv.second.program);
	m_programs.clear();
	m_uniforms.clear();

	for (auto &kv : m_ubos) {
		GLuint buf = kv.first;
		glDeleteBuffers(1, &buf);
	}
	m_ubos.clear();
}

std::string ShaderMgr::ResolveInclude(const std::string &file) const
{
	// 1. Verbatim path (handles "include/foo.glsl.inc" and "common.glsl").
	std::string direct = m_shaderPath + file;
	struct stat st;
	if (stat(direct.c_str(), &st) == 0)
		return direct;

	// 2. Bareword fallback → look in the include/ sub-directory.
	if (file.find('/') == std::string::npos) {
		std::string incdir = m_shaderPath + "include/" + file;
		if (stat(incdir.c_str(), &st) == 0)
			return incdir;
	}
	return {};
}

bool ShaderMgr::ReadShaderSource(const std::string &filename,
                                 std::string &outSource,
                                 std::vector<SourceDep> &deps,
                                 std::unordered_set<std::string> &included,
                                 int depth)
{
	if (depth > kMaxIncludeDepth) {
		fprintf(stderr, "[ShaderMgr] #include depth exceeded for '%s'\n", filename.c_str());
		return false;
	}

	std::string fullPath;
	if (depth == 0) {
		// Top-level shader file (vert/frag): always relative to shaderPath root.
		fullPath = m_shaderPath + filename;
		struct stat st;
		if (stat(fullPath.c_str(), &st) != 0) {
			fprintf(stderr, "[ShaderMgr] Cannot open shader file: %s\n", fullPath.c_str());
			return false;
		}
	} else {
		fullPath = ResolveInclude(filename);
		if (fullPath.empty()) {
			fprintf(stderr, "[ShaderMgr] Cannot resolve #include \"%s\"\n", filename.c_str());
			return false;
		}
	}

	// #pragma once across the whole include chain of this compilation unit.
	if (!included.insert(fullPath).second) {
		// Already included — emit a marker but no content.
		outSource += "// @include \"" + filename + "\" skipped (already included)\n";
		return true;
	}

	// Record dependency for hot-reload.
	deps.push_back({ fullPath, GetMTime(fullPath) });

	std::ifstream file(fullPath);
	if (!file.is_open()) {
		fprintf(stderr, "[ShaderMgr] Cannot open shader file: %s\n", fullPath.c_str());
		return false;
	}

	const int  sourceId = (int)deps.size();  // 1-based, unique per file in this unit
	bool       sawPragmaOnce = false;
	int        srcLine       = 0;
	bool       afterVersion  = (depth > 0);  // includes never emit #version themselves
	bool       emittedLeadingLine = false;

	// Stable marker so errors reference the actual source file.
	outSource += "// @file \"" + filename + "\"\n";

	std::string line;
	while (std::getline(file, line)) {
		++srcLine;

		// Strip leading whitespace for directive matching.
		size_t firstNonWs = line.find_first_not_of(" \t");
		std::string trimmed = (firstNonWs == std::string::npos)
		                       ? std::string() : line.substr(firstNonWs);

		// --- #version handling ---
		if (trimmed.rfind("#version", 0) == 0) {
			if (depth == 0) {
				// Keep the version line verbatim; it must stay first.
				outSource += line;
				outSource += '\n';
				afterVersion = true;
				// Inject a #line so diagnostics after this map to the source file.
				char buf[64];
				std::snprintf(buf, sizeof(buf), "#line %d %d\n", srcLine + 1, sourceId);
				outSource += buf;
				emittedLeadingLine = true;
				continue;
			} else {
				// Includes must not carry their own #version.
				fprintf(stderr, "[ShaderMgr] stripped #version in include '%s'\n",
				        filename.c_str());
				continue;
			}
		}

		// --- #pragma once ---
		if (trimmed.rfind("#pragma", 0) == 0 && trimmed.find("once") != std::string::npos) {
			// Already deduped via `included` set; this line is a safe no-op marker.
			sawPragmaOnce = true;
			continue;
		}

		// --- #include "file" ---
		if (trimmed.rfind("#include", 0) == 0) {
			size_t q1 = trimmed.find('"');
			size_t q2 = (q1 != std::string::npos) ? trimmed.find('"', q1 + 1) : std::string::npos;
			if (q1 == std::string::npos || q2 == std::string::npos) {
				fprintf(stderr, "[ShaderMgr] Malformed #include in %s line %d: %s\n",
				        filename.c_str(), srcLine, trimmed.c_str());
				return false;
			}
			std::string incFile = trimmed.substr(q1 + 1, q2 - q1 - 1);

			outSource += "// @include \"" + incFile + "\" begin\n";
			if (!ReadShaderSource(incFile, outSource, deps, included, depth + 1))
				return false;
			outSource += "// @include \"" + incFile + "\" end\n";

			// Restore line numbering for the enclosing file.
			char buf[64];
			std::snprintf(buf, sizeof(buf), "#line %d %d\n", srcLine + 1, sourceId);
			outSource += buf;
			continue;
		}

		// --- regular content ---
		if (!emittedLeadingLine && afterVersion) {
			// First content line of an include or of a pre-version-free top file.
			char buf[64];
			std::snprintf(buf, sizeof(buf), "#line %d %d\n", srcLine, sourceId);
			outSource += buf;
			emittedLeadingLine = true;
		}

		outSource += line;
		outSource += '\n';
	}

	(void)sawPragmaOnce; // acknowledged for future use; not required for correctness
	return true;
}

GLuint ShaderMgr::CompileShader(GLenum type, const char *source, const char *label)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLint logLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
		std::string log(logLen > 0 ? logLen : 1, '\0');
		glGetShaderInfoLog(shader, (GLsizei)log.size(), nullptr, &log[0]);
		fprintf(stderr, "[ShaderMgr] Compile error in '%s':\n%s\n", label, log.c_str());
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

	GLint success = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &success);
	if (!success) {
		GLint logLen = 0;
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
		std::string log(logLen > 0 ? logLen : 1, '\0');
		glGetProgramInfoLog(prog, (GLsizei)log.size(), nullptr, &log[0]);
		fprintf(stderr, "[ShaderMgr] Link error in '%s':\n%s\n", label, log.c_str());
		glDeleteProgram(prog);
		return 0;
	}

	fprintf(stderr, "[ShaderMgr] Loaded program '%s' (GL=%u)\n", label, prog);
	return prog;
}

void ShaderMgr::IntrospectUniformBlocks(ProgramEntry &entry)
{
	GLint blockCount = 0;
	glGetProgramiv(entry.program, GL_ACTIVE_UNIFORM_BLOCKS, &blockCount);

	GLint maxNameLen = 0;
	glGetProgramiv(entry.program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &maxNameLen);
	if (maxNameLen <= 0) maxNameLen = 64;

	std::string name(maxNameLen, '\0');
	for (GLint i = 0; i < blockCount; ++i) {
		GLsizei nameLen = 0;
		glGetActiveUniformBlockName(entry.program, (GLuint)i,
		                            (GLsizei)name.size(), &nameLen, &name[0]);
		if (nameLen > 0)
			entry.uniformBlocks.emplace_back(name.c_str(), (size_t)nameLen);
	}
}

time_t ShaderMgr::GetMTime(const std::string &path)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return 0;
	return st.st_mtime;
}

} // namespace ogl

#endif // !_WIN32
