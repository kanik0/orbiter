// Stubs that satisfy the Orbiter-executable symbols libLuaInterpreter.dylib
// would normally resolve against the host process. The test harness runs
// without Orbiter, so dyld would otherwise abort on the first eager RTTI
// lookup (typeinfo/vtable for MFD2). None of these definitions are called
// at runtime — they exist purely so the test binary is self-contained.

#define OAPI_IMPLEMENTATION
#include "MFDAPI.h"

// MFD key methods — emit the base typeinfo that MFD2's typeinfo chains to.
MFD::MFD(DWORD w, DWORD h, VESSEL *vessel)
	: W(w), H(h), cw(0), ch(0), pV(vessel), instr(nullptr) {}
MFD::~MFD() {}

// MFD2 key methods — emit typeinfo + vtable for MFD2 in the test binary.
bool        MFD2::Update         (oapi::Sketchpad *)                       { return false; }
void        MFD2::Title          (oapi::Sketchpad *, const char *) const   {}
oapi::Pen  *MFD2::GetDefaultPen  (DWORD, DWORD, DWORD)              const  { return nullptr; }
oapi::Font *MFD2::GetDefaultFont (DWORD)                            const  { return nullptr; }
DWORD       MFD2::GetDefaultColour(DWORD, DWORD)                    const  { return 0; }
