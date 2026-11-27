#pragma once
#ifdef BH_AGENT

#include <Windows.h>

namespace Agent {

// Points the module's imports of one function, from the DLL named, at a
// replacement, and returns how many import slots it changed. An import is
// matched by the address it holds rather than by name, because the game's DLLs
// were linked with their imports bound, and a bound import table keeps no names
// in the slots it binds.
int RedirectImport(HMODULE module, const char* dll, const char* function, void* replacement);

}

#endif
