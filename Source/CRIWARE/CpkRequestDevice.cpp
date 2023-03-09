#include "Criware.h"
#include "CriwareFileLoader.h"

struct CriFsIoCpkReqHandle
{
	std::string name{};
	uint64_t size{};
	FileLoadRequest* request{};

	~CriFsIoCpkReqHandle()
	{
		if (request)
		{
			CriFileLoader_DeleteRequest(request);
		}
	}
};

FileLoadRequest* CpkRequestFromString(const char* path)
{
	if (path == nullptr || strstr(path, "REQ\\") != path)
	{
		return nullptr;
	}

	FileLoadRequest* handle{};
	if (sscanf(path, "REQ\\%p", &handle) == 0 || !FileLoadRequest::IsValid(handle))
	{
		return nullptr;
	}

	return handle;
}

CriFsIoError CRIAPI CriFsIoCpkReq_Exists(const CriChar8* path, CriBool* result)
{
	if (!path || !result)
	{
		return CRIFS_IO_ERROR_NG;
	}

	*result = CpkRequestFromString(path) != nullptr;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpkReq_Open(const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* filehn)
{
	if (!path || !filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	auto* request = CpkRequestFromString(path);
	if (!request)
	{
		return CRIFS_IO_ERROR_NG;
	}

	auto* handle = new CriFsIoCpkReqHandle();
	handle->name = request->path;
	handle->size = CriFileLoader_GetFileSize(request->path.c_str());
	handle->request = request;

	*filehn = handle;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpkReq_Close(CriFsFileHn filehn)
{
	delete static_cast<CriFsIoCpkReqHandle*>(filehn);
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpkReq_GetFileSize(CriFsFileHn filehn, CriSint64* file_size)
{
	if (!filehn || !file_size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoCpkReqHandle*>(filehn);
	*file_size = handle->request->load_size;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpkReq_Read(CriFsFileHn filehn, CriSint64 offset, CriSint64 read_size, void* buffer, CriSint64 buffer_size)
{
	if (!filehn || !buffer || !read_size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoCpkReqHandle*>(filehn);
	if (handle->request->buffer != buffer)
	{
		memcpy(buffer, handle->request->buffer, read_size);
	}

	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpkReq_IsReadComplete(CriFsFileHn filehn, CriBool* result)
{
	if (!filehn || !result)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoCpkReqHandle*>(filehn);
	bool finished{};
	CriFileLoader_IsLoadComplete(handle->request, &finished);

	*result = finished;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpkReq_GetReadSize(CriFsFileHn filehn, CriSint64* read_size)
{
	if (!filehn || !read_size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoCpkReqHandle*>(filehn);
	*read_size = handle->request->load_size;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpkReq_Flush(CriFsFileHn filehn)
{
	return CRIFS_IO_ERROR_OK;
}

CriFsIoInterfaceTag cpk_req_io_interface
{
	CriFsIoCpkReq_Exists,
	nullptr,
	nullptr,
	CriFsIoCpkReq_Open,
	CriFsIoCpkReq_Close,
	CriFsIoCpkReq_GetFileSize,
	CriFsIoCpkReq_Read,
	CriFsIoCpkReq_IsReadComplete,
	nullptr,
	CriFsIoCpkReq_GetReadSize,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	CriFsIoCpkReq_Flush,
	nullptr,
};