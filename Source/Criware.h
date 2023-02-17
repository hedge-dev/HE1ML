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

typedef uint32_t CriFsFileMode;
typedef uint32_t CriFsFileAccess;
typedef void* CriFsFileHn;

/// The layout of this structure usually varies with the version of CRIWARE
/// but the first few fields are usually the same.
struct CriFsConfig
{
	CriUint32 thread_model;
	CriSint32 num_binders;
	CriSint32 num_loaders;
	CriSint32 num_group_loaders;
	CriSint32 num_stdio_handles;
	CriSint32 num_installers;
	CriSint32 max_binds;
	CriSint32 max_files;
};

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

typedef enum {
	CRIFSLOADER_STATUS_STOP,
	CRIFSLOADER_STATUS_LOADING,
	CRIFSLOADER_STATUS_COMPLETE,
	CRIFSLOADER_STATUS_ERROR,

	/* enum be 4bytes */
	CRIFSLOADER_STATUS_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsLoaderStatus;

#ifdef MODLOADER_IMPLEMENTATION
#include <unordered_set>

typedef CriError (*CriFsBindCpkHook_t)(CriFsBinderHn& bndrhn, CriFsBinderHn& srcbndrhn, const CriChar8*& path, void*& work, CriSint32& worksize, CriFsBindId*& bndrid);
typedef CriError (*CriFsUnbindHook_t)(CriFsBindId& bndrid);

#define ML_CRIWARE_HOOK_PRE_LOAD 0
#define ML_CRIWARE_HOOK_POST_LOAD 1
#define ML_CRIWARE_HOOK_PRE_BINDCPK 2
#define ML_CRIWARE_HOOK_POST_BINDCPK 3
#define ML_CRIWARE_HOOK_PRE_UNBIND 4
#define ML_CRIWARE_HOOK_POST_UNBIND 5

#define ML_CRIWARE_HOOK_MAX 6

#define ML_SET_CRIWARE_HOOK(ID, FUNC) g_cri_hooks[ID] = (void*)FUNC;

constexpr char c_dir_stub[] = "HML\\";
extern ModLoader* g_loader;
extern CriFsBindId g_dir_bind;
extern std::unordered_set<std::string> g_cpk_binds;
extern void* g_cri_hooks[];

typedef void (CRIAPI* CriErrCbFunc)(const CriChar8* errid, CriUint32 p1, CriUint32 p2, CriUint32* parray);

struct CriFunctionTable
{
	FUNCTION_PTR(CriError, CRIAPI, criFs_CalculateWorkSizeForLibrary, 0x007D0413, const CriFsConfig* config, CriSint32* worksize);
	FUNCTION_PTR(void, CRIAPI, criErr_SetCallback, 0x007C92C5, CriErrCbFunc cbf);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_BindDirectory, 0x007D2AB8, CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8* path, void* work, CriSint32 worksize, CriFsBindId* bndrid);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_SetPriority, 0x007D2512, CriFsBindId bndrid, CriSint32 priority);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_GetStatus, 0x007D3300, CriFsBindId bndrid, CriFsBinderStatus* status);
	FUNCTION_PTR(CriError, CRIAPI, criFsLoader_GetStatus, 0x007D42F1, CriFsLoaderHn loader, CriFsLoaderStatus* status);

	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_Unbind, 0x007D2CCD, CriFsBindId bndrid);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_BindCpk, 0x007D35F4, CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8* path, void* work, CriSint32 worksize, CriFsBindId* bndrid);
	FUNCTION_PTR(CriError, CRIAPI, criFsiowin_Open, 0x007D6B1E, const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* filehn);
	FUNCTION_PTR(CriError, CRIAPI, criFsIoWin_Exists, 0x007D66DB, const CriChar8* path, CriBool* exists);
	FUNCTION_PTR(CriError, CRIAPI, criFsLoader_Create, 0x007D52E2, CriFsLoaderHn* loader);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_Create, 0x007D297D, CriFsBinderHn* binder);
	FUNCTION_PTR(CriError, CRIAPI, criFsLoader_Load, 0x007D5026, CriFsLoaderHn loader,
		CriFsBinderHn binder, const CriChar8* path, CriSint64 offset,
		CriSint64 load_size, void* buffer, CriSint64 buffer_size);
};

void InitCri(ModLoader* loader);
#endif