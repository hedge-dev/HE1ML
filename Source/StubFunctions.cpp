#include <cassert>
#define MAKE_STUB(NAME) void __declspec(dllexport) NAME() { assert(false && "Attempted to call unloaded function: " #NAME); }

extern "C"
{
	// d3d9.dll
	MAKE_STUB(D3DPERF_BeginEvent);
	MAKE_STUB(D3DPERF_EndEvent);
	MAKE_STUB(D3DPERF_GetStatus);
	MAKE_STUB(D3DPERF_QueryRepeatFrame);
	MAKE_STUB(D3DPERF_SetMarker);
	MAKE_STUB(D3DPERF_SetOptions);
	MAKE_STUB(D3DPERF_SetRegion);
	MAKE_STUB(DebugSetLevel);
	MAKE_STUB(DebugSetMute);
	MAKE_STUB(Direct3D9EnableMaximizedWindowedModeShim);
	MAKE_STUB(Direct3DCreate9);
	MAKE_STUB(Direct3DCreate9Ex);
	MAKE_STUB(Direct3DCreate9On12);
	MAKE_STUB(Direct3DCreate9On12Ex);
	MAKE_STUB(Direct3DShaderValidatorCreate9);
	MAKE_STUB(PSGPError);
	MAKE_STUB(PSGPSampleTexture);
}

void ResolveStubMethods(void* module)
{
#define MAP(MODULE, ADDRESS) ((char*)(MODULE) + (unsigned)(ADDRESS))

	HMODULE executingModule{};
	assert(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCSTR>(ResolveStubMethods), &executingModule));

	const auto* header = reinterpret_cast<IMAGE_DOS_HEADER*>(executingModule);
	const auto* ntHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(MAP(executingModule, header->e_lfanew));
	
	const auto* exportDir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(MAP(executingModule, ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));

	const DWORD* names = reinterpret_cast<DWORD*>(MAP(executingModule, exportDir->AddressOfNames));
	for (size_t i = 0; i < exportDir->NumberOfNames; i++)
	{
		const char* name = MAP(executingModule, names[i]);
		void* newProc = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), name));

		if (newProc)
		{
			const DWORD* functions = reinterpret_cast<DWORD*>(MAP(executingModule, exportDir->AddressOfFunctions));
			const WORD* ordinals = reinterpret_cast<WORD*>(MAP(executingModule, exportDir->AddressOfNameOrdinals));

			const DWORD function = functions[ordinals[i]];
			void* oldProc = MAP(executingModule, function);
			
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&oldProc, newProc);
			DetourTransactionCommit();
		}
	}
	
#undef MAP
}