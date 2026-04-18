// Copyright (c) Martin Schweiger
// Licensed under the MIT License
//
// XRString — CString compatibility shim for non-Windows builds.
//
// On Windows this header is a thin pass-through onto MFC's <atlstr.h>
// so the production build keeps using the same CString type that
// every existing XRSound source file refers to.
//
// On macOS/Linux the header ships a minimal CString-compatible class
// under the name XRString and aliases CString to it via typedef, so
// the ~160 existing CString callsites across Sound/XRSound/src compile
// unchanged. Only the methods XRSound actually invokes are provided
// (Format, IsEmpty, GetLength, GetAt/SetAt, Find, Left, Empty,
// CompareNoCase + implicit const char* conversion).

#ifndef __XRSTRING_H
#define __XRSTRING_H

#ifdef _WIN32

#include <atlstr.h>  // real MFC CString

#else

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

class XRString {
public:
	XRString()                            : m_s()              {}
	XRString(const char *s)               : m_s(s ? s : "")   {}
	XRString(const char *s, int len)      : m_s(s ? s : "", len > 0 ? (size_t)len : 0) {}
	XRString(const XRString &o)           : m_s(o.m_s)        {}
	XRString(const std::string &s)        : m_s(s)            {}

	XRString &operator=(const char *s)    { m_s = (s ? s : ""); return *this; }
	XRString &operator=(const XRString &o){ m_s = o.m_s;        return *this; }

	// Implicit const char* conversion — callers frequently pass a
	// CString straight into printf / Orbiter log APIs that expect a
	// C string; preserving the implicit cast keeps those untouched.
	operator const char *() const { return m_s.c_str(); }

	// Format / FormatV mirror CString::Format (printf-style).
	void Format(const char *fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		FormatV(fmt, ap);
		va_end(ap);
	}
	void FormatV(const char *fmt, va_list ap)
	{
		va_list ap2;
		va_copy(ap2, ap);
		const int needed = std::vsnprintf(nullptr, 0, fmt, ap2);
		va_end(ap2);
		if (needed < 0) { m_s.clear(); return; }
		m_s.assign((size_t)needed, '\0');
		std::vsnprintf(m_s.data(), (size_t)needed + 1, fmt, ap);
	}

	int  GetLength() const  { return (int)m_s.size(); }
	bool IsEmpty()  const   { return m_s.empty(); }
	void Empty()            { m_s.clear(); }

	char GetAt(int i)       const { return (i >= 0 && (size_t)i < m_s.size()) ? m_s[(size_t)i] : 0; }
	void SetAt(int i, char c)     { if (i >= 0 && (size_t)i < m_s.size()) m_s[(size_t)i] = c; }

	// Find returns the first occurrence or -1, matching CString::Find.
	int Find(char c) const {
		size_t p = m_s.find(c);
		return p == std::string::npos ? -1 : (int)p;
	}
	int Find(const char *sub) const {
		if (!sub) return -1;
		size_t p = m_s.find(sub);
		return p == std::string::npos ? -1 : (int)p;
	}

	XRString Left(int n) const {
		if (n <= 0) return XRString();
		if ((size_t)n >= m_s.size()) return *this;
		return XRString(m_s.substr(0, (size_t)n));
	}

	// CompareNoCase returns <0 / 0 / >0 like strcasecmp.
	int CompareNoCase(const char *other) const {
		if (!other) return m_s.empty() ? 0 : 1;
		return ::strcasecmp(m_s.c_str(), other);
	}

	bool operator==(const char *s)    const { return s && m_s == s; }
	bool operator==(const XRString &o) const { return m_s == o.m_s; }
	bool operator!=(const char *s)    const { return !(*this == s); }
	bool operator!=(const XRString &o) const { return m_s != o.m_s; }
	bool operator<(const XRString &o)  const { return m_s < o.m_s;  }

	XRString &operator+=(const char *s)    { if (s) m_s += s; return *this; }
	XRString &operator+=(const XRString &o){ m_s += o.m_s;    return *this; }

	friend XRString operator+(const XRString &a, const XRString &b) { XRString r(a); r += b; return r; }
	friend XRString operator+(const XRString &a, const char *b)     { XRString r(a); r += b; return r; }

private:
	std::string m_s;
};

// Orbiter / XRSound callers uniformly write `CString foo` — alias so
// every existing declaration continues to compile without edits.
typedef XRString CString;

#endif // !_WIN32

#endif // __XRSTRING_H
