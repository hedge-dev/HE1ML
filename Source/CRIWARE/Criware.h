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
typedef CriSint32 CriBool;

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

typedef struct CriFsBinderFileInfoTag {
	CriFsFileHn filehn;
	CriChar8* path;
	CriSint64 offset;
	CriSint32 read_size; // Compressed file size
	CriSint32 extract_size; // Uncompressed file size
	CriFsBindId binderid;
	CriUint32 reserved[1];
} CriFsBinderFileInfo;

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

typedef enum CriFsDeviceIdTag {
	CRIFS_DEVICE_00 = 0, // Default device
	CRIFS_DEVICE_01,
	CRIFS_DEVICE_02,
	CRIFS_DEVICE_03,
	CRIFS_DEVICE_04,
	CRIFS_DEVICE_05,
	CRIFS_DEVICE_06,
	CRIFS_DEVICE_07, // Memory
	CRIFS_DEVICE_MAX,

	CRIFS_DEVICE_INVALID = -1,

	/* enum be 4bytes */
	CRIFS_DEVICE_ENUM_BE_SINT32 = 0x7fffffff
} CriFsDeviceId;

typedef enum {
	CRIFS_IO_ERROR_OK = 0,
	CRIFS_IO_ERROR_NG = -1,
	CRIFS_IO_ERROR_TRY_AGAIN = -2,

	CRIFS_IO_ERROR_NG_NO_ENTRY = -11,
	CRIFS_IO_ERROR_NG_INVALID_DATA = -12,

	/* enum be 4bytes */
	CRIFS_IO_ERROR_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsIoError;

typedef enum {
	CRIFS_FILE_ACCESS_READ = 0,
	CRIFS_FILE_ACCESS_WRITE = 1,
	CRIFS_FILE_ACCESS_READ_WRITE = 2,

	/* enum be 4bytes */
	CRIFS_FILE_ACCESS_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsFileAccess;

typedef enum {
	CRIFS_FILE_MODE_APPEND = 0,
	CRIFS_FILE_MODE_CREATE = 1,
	CRIFS_FILE_MODE_CREATE_NEW = 2,
	CRIFS_FILE_MODE_OPEN = 3,
	CRIFS_FILE_MODE_OPEN_OR_CREATE = 4,
	CRIFS_FILE_MODE_TRUNCATE = 5,

	/* Special case*/
	CRIFS_FILE_MODE_OPEN_WITHOUT_DECRYPTING = 10,	/*EN< Opens a file (Without decrypting)	*/

	/* enum be 4bytes */
	CRIFS_FILE_MODE_ENUM_BE_SINT32 = 0x7FFFFFFF
} CriFsFileMode;

// Only including the common functions
typedef struct CriFsIoInterfaceTag {
	CriFsIoError(CRIAPI* Exists)(const CriChar8* path, CriBool* result);
	CriFsIoError(CRIAPI* Remove)(const CriChar8* path);
	CriFsIoError(CRIAPI* Rename)(const CriChar8* old_path, const CriChar8* new_path);
	CriFsIoError(CRIAPI* Open)(
		const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* filehn);

	CriFsIoError(CRIAPI* Close)(CriFsFileHn filehn);
	CriFsIoError(CRIAPI* GetFileSize)(CriFsFileHn filehn, CriSint64* file_size);
	CriFsIoError(CRIAPI* Read)(CriFsFileHn filehn, CriSint64 offset, CriSint64 read_size, void* buffer, CriSint64 buffer_size);
	CriFsIoError(CRIAPI* IsReadComplete)(CriFsFileHn filehn, CriBool* result);
	CriFsIoError(CRIAPI* CancelRead)(CriFsFileHn filehn);
	CriFsIoError(CRIAPI* GetReadSize)(CriFsFileHn filehn, CriSint64* read_size);
	CriFsIoError(CRIAPI* Write)(CriFsFileHn filehn, CriSint64 offset, CriSint64 write_size, void* buffer, CriSint64 buffer_size);
	CriFsIoError(CRIAPI* IsWriteComplete)(CriFsFileHn filehn, CriBool* result);
	CriFsIoError(CRIAPI* CancelWrite)(CriFsFileHn filehn);
	CriFsIoError(CRIAPI* GetWriteSize)(CriFsFileHn filehn, CriSint64* write_size);
	CriFsIoError(CRIAPI* Flush)(CriFsFileHn filehn);
	CriFsIoError(CRIAPI* Resize)(CriFsFileHn filehn, CriSint64 size);
	//CriFsIoError(CRIAPI* GetNativeFileHandle)(CriFsFileHn filehn, void** native_filehn);
	//CriFsIoError(CRIAPI* SetAddReadProgressCallback)(CriFsFileHn filehn, void(*callback)(void*, CriSint32), void* obj);
} CriFsIoInterface, *CriFsIoInterfacePtr;

typedef CriError(CRIAPI* CriFsSelectIoCbFunc)(
	const CriChar8* path, CriFsDeviceId* device_id, CriFsIoInterfacePtr* ioif);

inline int crifs_uint_ptr_to_string(unsigned int a1, char* a2)
{
	// Disassembled code
	int i; // r9
	char v3; // r11
	bool v4; // cr32

	for (i = 7; i >= 0; --i)
	{
		v3 = a1 & 0xF;
		v4 = (a1 & 0xF) < 0xA;
		a1 >>= 4;
		if (v4)
			*(a2 + i) = v3 + 48;
		else
			*(a2 + i) = v3 + 55;
	}
	return 8;
}

inline int criFs_AddressToPath(const void* buffer, CriUint32 buffer_size, CriChar8* path, CriSint32 length)
{
	int result; // r3

	if (buffer_size >= 0 && length)
	{
		if (length >= 28)
		{
			strcpy_s(path, length, "CRIFSMEM:/");
			crifs_uint_ptr_to_string(reinterpret_cast<unsigned int>(buffer), (path + 10));
			path[18] = 46;
			crifs_uint_ptr_to_string(buffer_size, (path + 19));
			result = 0;
			path[27] = 0;
		}
		else
		{
			// criErr_Notify(0, "E2010111602:Length of path is insufficient.");
			result = -2;
		}
	}
	else
	{
		// criErr_NotifyGeneric(0, "E2010111691", -2);
		result = -2;
	}
	return result;
}

#ifdef _WINDOWS_
inline static long criAtomic_TestAndSet(long* target, long value)
{
	return InterlockedExchange(target, value);
}
#endif

#ifdef MODLOADER_IMPLEMENTATION
#include <unordered_set>

typedef CriError (*CriFsBindCpkHook_t)(CriFsBinderHn& bndrhn, CriFsBinderHn& srcbndrhn, const CriChar8*& path, void*& work, CriSint32& worksize, CriFsBindId*& bndrid);
typedef CriError (*CriFsLoadHook_t)(CriFsLoaderHn& loader,
	CriFsBinderHn& binder, const CriChar8*& path, CriSint64& offset,
	CriSint64& load_size, void*& buffer, CriSint64& buffer_size);

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
#endif

typedef void (CRIAPI* CriErrCbFunc)(const CriChar8* errid, CriUint32 p1, CriUint32 p2, CriUint32* parray);

struct CriFunctionTable
{
	FUNCTION_PTR(CriError, CRIAPI, criFs_CalculateWorkSizeForLibrary, 0x007D0413, const CriFsConfig* config, CriSint32* worksize);
	FUNCTION_PTR(void, CRIAPI, criErr_SetCallback, 0x007C92C5, CriErrCbFunc cbf);
	FUNCTION_PTR(void, CRIAPI, criFs_SetSelectIoCallback, nullptr, CriFsSelectIoCbFunc func);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_BindDirectory, 0x007D2AB8, CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8* path, void* work, CriSint32 worksize, CriFsBindId* bndrid);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_SetPriority, 0x007D2512, CriFsBindId bndrid, CriSint32 priority);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_GetStatus, 0x007D3300, CriFsBindId bndrid, CriFsBinderStatus* status);
	FUNCTION_PTR(CriError, CRIAPI, criFsLoader_GetStatus, 0x007D42F1, CriFsLoaderHn loader, CriFsLoaderStatus* status);

	FUNCTION_PTR(CriError, CRIAPI, crifsbinder_findWithNameEx, 0x007D335A, CriFsBinderHn bndrhn, const CriChar8* path, void* a3, CriFsBinderFileInfo* finfo, void* a5, CriBool* exists);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_Find, 0x007D38A3, CriFsBinderHn bndrhn, const CriChar8* filepath, CriFsBinderFileInfo* finfo, CriBool* exist);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_Unbind, 0x007D2CCD, CriFsBindId bndrid);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_BindCpk, 0x007D35F4, CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8* path, void* work, CriSint32 worksize, CriFsBindId* bndrid);
	FUNCTION_PTR(CriFsIoError, CRIAPI, criFsiowin_Open, 0x007D6B1E, const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* filehn);
	FUNCTION_PTR(CriFsIoError, CRIAPI, criFsIoWin_Exists, 0x007D66DB, const CriChar8* path, CriBool* exists);
	FUNCTION_PTR(CriError, CRIAPI, criFsLoader_Create, 0x007D52E2, CriFsLoaderHn* loader);
	FUNCTION_PTR(CriError, CRIAPI, criFsBinder_Create, 0x007D297D, CriFsBinderHn* binder);
	FUNCTION_PTR(CriError, CRIAPI, criFsLoader_Load, 0x007D5026, CriFsLoaderHn loader,
		CriFsBinderHn binder, const CriChar8* path, CriSint64 offset,
		CriSint64 load_size, void* buffer, CriSint64 buffer_size);
};

#ifdef MODLOADER_IMPLEMENTATION
void InitCri(ModLoader* loader);
#endif