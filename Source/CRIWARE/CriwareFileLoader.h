#pragma once
enum CriError;
struct CriFunctionTable;
typedef uint32_t CriUint32;
typedef uint64_t CriUint64;
typedef CriUint32 CriFsBindId;

#define CRI_FILE_LOADER_FLAG_NONE 0
#define CRI_FILE_LOADER_FLAG_NO_DIR 1
#define CRI_FILE_LOADER_FLAG_NO_CPK 2

#define FILE_LOAD_REQUEST_SIGNATURE 0x52495243
struct FileLoadRequest
{
	int32_t signature{ FILE_LOAD_REQUEST_SIGNATURE };
	std::string path{};
	int64_t offset{};
	int64_t load_size{};
	void* buffer{};
	int64_t buffer_size{};
	bool ready{};
	bool loading{};
	bool finished{};

	static bool IsValid(void* memory)
	{
		return memory != nullptr && static_cast<FileLoadRequest*>(memory)->signature == FILE_LOAD_REQUEST_SIGNATURE;
	}

	~FileLoadRequest()
	{
		signature = 0;
	}
};

CriError CriFileLoader_Init(const CriFunctionTable& table, size_t flags = CRI_FILE_LOADER_FLAG_NONE);
bool CriFileLoader_IsInit();
CriError CriFileLoader_BindCpk(const char* path);
CriError CriFileLoader_BindCpk(const char* path, CriFsBindId* out_id);
CriError CriFileLoader_BindDirectory(const char* path);
CriError CriFileLoader_BindDirectory(const char* path, CriFsBindId* out_id);
CriError CriFileLoader_Unbind(CriFsBindId id);
CriError CriFileLoader_Load(const char* path, int64_t offset, int64_t load_size, void* buffer, int64_t buffer_size);
CriError CriFileLoader_LoadAsync(const char* path, int64_t offset, int64_t load_size, void* buffer, int64_t buffer_size, FileLoadRequest** out_request);
CriError CriFileLoader_DeleteRequest(FileLoadRequest* request);
CriError CriFileLoader_IsLoadComplete(const FileLoadRequest* request, bool* out_complete);
bool CriFileLoader_FileExists(const char* path);
CriUint64 CriFileLoader_GetFileSize(const char* path);