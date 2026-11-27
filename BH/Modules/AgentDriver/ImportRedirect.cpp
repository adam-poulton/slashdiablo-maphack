#ifdef BH_AGENT

#include "ImportRedirect.h"

namespace Agent {

int RedirectImport(HMODULE module, const char* dll, const char* function, void* replacement) {
	FARPROC original = GetProcAddress(GetModuleHandleA(dll), function);
	BYTE* base = (BYTE*)module;
	IMAGE_NT_HEADERS* headers = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
	IMAGE_DATA_DIRECTORY imports = headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (!original || !imports.VirtualAddress)
		return 0;
	int redirected = 0;
	for (IMAGE_IMPORT_DESCRIPTOR* from = (IMAGE_IMPORT_DESCRIPTOR*)(base + imports.VirtualAddress); from->Name; from++) {
		if (_stricmp((const char*)(base + from->Name), dll) != 0)
			continue;
		for (IMAGE_THUNK_DATA* slot = (IMAGE_THUNK_DATA*)(base + from->FirstThunk); slot->u1.Function; slot++) {
			if (slot->u1.Function != (DWORD)original)
				continue;
			DWORD protection;
			VirtualProtect(&slot->u1.Function, sizeof(DWORD), PAGE_READWRITE, &protection);
			slot->u1.Function = (DWORD)replacement;
			VirtualProtect(&slot->u1.Function, sizeof(DWORD), protection, &protection);
			redirected++;
		}
	}
	return redirected;
}

}

#endif
