#pragma once
enum CriError;
struct CriFunctionTable;
typedef uint32_t CriUint32;
typedef CriUint32 CriFsBindId;

#define CRI_FILE_LOADER_FLAG_NONE 0
#define CRI_FILE_LOADER_FLAG_NO_DIR 1
#define CRI_FILE_LOADER_FLAG_NO_CPK 2

CriError CriFileLoader_Init(const CriFunctionTable& table, size_t flags = CRI_FILE_LOADER_FLAG_NONE);
bool CriFileLoader_IsInit();
CriError CriFileLoader_BindCpk(const char* path);
CriError CriFileLoader_BindCpk(const char* path, CriFsBindId* out_id);
CriError CriFileLoader_BindDirectory(const char* path);
CriError CriFileLoader_BindDirectory(const char* path, CriFsBindId* out_id);
CriError CriFileLoader_Unbind(CriFsBindId id);
CriError CriFileLoader_Load(const char* path, int64_t offset, int64_t load_size, void* buffer, int64_t buffer_size);
bool CriFileLoader_FileExists(const char* path);