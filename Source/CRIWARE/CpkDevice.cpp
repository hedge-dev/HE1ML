#include "Criware.h"
#include "CriwareFileLoader.h"

struct CriFsIoCpkHandle
{
	std::string name{};
	CriUint64 size{};
	FileLoadRequest* request{};

	~CriFsIoCpkHandle()
	{
		if (request)
		{
			CriFileLoader_DeleteRequest(request);
		}
	}
};

CriFsIoError CRIAPI CriFsIoCpk_Exists(const CriChar8* path, CriBool* result)
{
	if (!path || !result)
	{
		return CRIFS_IO_ERROR_NG;
	}

	if (strstr(path, "HML\\") == path)
	{
		path += 4;
	}

	*result = CriFileLoader_FileExists(path);
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpk_Open(const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* filehn)
{
	if (!path || !filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	if (strstr(path, "HML\\") == path)
	{
		path += 4;
	}

	if (!CriFileLoader_FileExists(path))
	{
		return CRIFS_IO_ERROR_NG;
	}

	auto* handle = new CriFsIoCpkHandle();
	handle->name = path;
	handle->size = CriFileLoader_GetFileSize(path);

	*filehn = handle;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpk_Close(CriFsFileHn filehn)
{
	delete static_cast<CriFsIoCpkHandle*>(filehn);
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpk_GetFileSize(CriFsFileHn filehn, CriSint64* file_size)
{
	if (!filehn || !file_size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoCpkHandle*>(filehn);
	*file_size = handle->size;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpk_Read(CriFsFileHn filehn, CriSint64 offset, CriSint64 read_size, void* buffer, CriSint64 buffer_size)
{
	if (!filehn || !buffer || !read_size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	auto* handle = static_cast<CriFsIoCpkHandle*>(filehn);
	CriFileLoader_LoadAsync(handle->name.c_str(), offset, read_size, buffer, buffer_size, &handle->request);
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpk_IsReadComplete(CriFsFileHn filehn, CriBool* result)
{
	if (!filehn || !result)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoCpkHandle*>(filehn);
	bool finished{};
	CriFileLoader_IsLoadComplete(handle->request, &finished);

	*result = finished;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpk_GetReadSize(CriFsFileHn filehn, CriSint64* read_size)
{
	if (!filehn || !read_size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	*read_size = 0;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoCpk_Flush(CriFsFileHn filehn)
{
	return CRIFS_IO_ERROR_OK;
}

CriFsIoInterfaceTag cpk_io_interface
{
	CriFsIoCpk_Exists,
	nullptr,
	nullptr,
	CriFsIoCpk_Open,
	CriFsIoCpk_Close,
	CriFsIoCpk_GetFileSize,
	CriFsIoCpk_Read,
	CriFsIoCpk_IsReadComplete,
	nullptr,
	CriFsIoCpk_GetReadSize,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	CriFsIoCpk_Flush,
	nullptr,
};