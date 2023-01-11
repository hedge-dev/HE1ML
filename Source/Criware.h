#pragma once
#define CRIAPI __cdecl
typedef void* CriFsBinderHn;
typedef void* CriFsLoaderHn;
typedef char CriChar8;
typedef int32_t CriSint32;
typedef int64_t CriSint64;
typedef uint32_t CriUint32;
typedef uint64_t CriUint64;
typedef CriUint32 CriFsBindId;
typedef bool CriBool;

enum CriError {
	CRIERR_OK = 0,
	CRIERR_NG = -1,		
	CRIERR_INVALID_PARAMETER = -2,
	CRIERR_FAILED_TO_ALLOCATE_MEMORY = -3,
	CRIERR_UNSAFE_FUNCTION_CALL = -4,
	CRIERR_FUNCTION_NOT_IMPLEMENTED = -5,
	CRIERR_LIBRARY_NOT_INITIALIZED = -6,
	/* enum be 4bytes */
	CRIERR_ENUM_BE_SINT32 = 0x7FFFFFFF
};

enum CriFsBinderStatus {
	CRIFSBINDER_STATUS_NONE = 0,
	CRIFSBINDER_STATUS_ANALYZE,
	CRIFSBINDER_STATUS_COMPLETE,
	CRIFSBINDER_STATUS_UNBIND,
	CRIFSBINDER_STATUS_REMOVED,
	CRIFSBINDER_STATUS_INVALID,
	CRIFSBINDER_STATUS_ERROR,

	/* enum be 4bytes */
	CRIFSBINDER_STATUS_ENUM_BE_SINT32 = 0x7FFFFFFF
};

#ifdef MODLOADER_IMPLEMENTATION
#include <unordered_set>
constexpr char c_dir_stub[] = "HML\\";
extern ModLoader* g_loader;
extern CriFsBindId g_dir_bind;
extern std::unordered_set<std::string> g_cpk_binds;
extern CriFsBinderHn g_override_binder;

typedef void (CRIAPI* CriErrCbFunc)(const CriChar8* errid, CriUint32 p1, CriUint32 p2, CriUint32* parray);
inline FUNCTION_PTR(void, CRIAPI, criErr_SetCallback, 0x007C92C5, CriErrCbFunc cbf);
inline FUNCTION_PTR(CriError, CRIAPI, criFsBinder_BindDirectory, 0x007D2AB8, CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8* path, void* work, CriSint32 worksize, CriFsBindId* bndrid);
inline FUNCTION_PTR(CriError, CRIAPI, criFsBinder_SetPriority, 0x007D2512, CriFsBindId bndrid, CriSint32 priority);
inline FUNCTION_PTR(CriError, CRIAPI, criFsBinder_GetStatus, 0x007D3300, CriFsBindId bndrid, CriFsBinderStatus* status);

void InitCri(ModLoader* loader);
#endif