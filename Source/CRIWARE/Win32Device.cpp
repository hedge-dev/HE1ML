#include "Criware.h"

struct CriFsIoWinHandle
{
	std::string name{};
	CriUint64 size{};
	CriUint64 bytes_read{};
	HANDLE file{};
};

CriFsIoError CRIAPI CriFsIoWin_Exists(const CriChar8* path, CriBool* result)
{
	if (!path || !result)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto attributes = GetFileAttributesA(path);
	*result = !!(attributes & INVALID_FILE_ATTRIBUTES);
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_Remove(const CriChar8* path)
{
	if (!path)
	{
		return CRIFS_IO_ERROR_NG;
	}

	if (!DeleteFileA(path))
	{
		return CRIFS_IO_ERROR_NG;
	}

	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_Rename(const CriChar8* old_path, const CriChar8* new_path)
{
	if (!old_path || !new_path)
	{
		return CRIFS_IO_ERROR_NG;
	}

	if (!MoveFileA(old_path, new_path))
	{
		return CRIFS_IO_ERROR_NG;
	}

	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_Open(const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* filehn)
{
	if (!path || !filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	auto* handle = new CriFsIoWinHandle();
	handle->name = path;

	const auto access_flags = (access == CRIFS_FILE_ACCESS_READ) ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE;
	const auto share_flags = (access == CRIFS_FILE_ACCESS_READ) ? FILE_SHARE_READ : 0;
	const auto create_flags = (mode == CRIFS_FILE_MODE_OPEN) ? OPEN_EXISTING : CREATE_ALWAYS;
	handle->file = CreateFileA(path, access_flags, share_flags, nullptr, create_flags, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle->file == INVALID_HANDLE_VALUE)
	{
		delete handle;
		return CRIFS_IO_ERROR_NG;
	}

	LARGE_INTEGER size;
	if (!GetFileSizeEx(handle->file, &size))
	{
		delete handle;
		return CRIFS_IO_ERROR_NG;
	}

	handle->size = size.QuadPart;
	*filehn = handle;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_Close(CriFsFileHn filehn)
{
	if (!filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoWinHandle*>(filehn);
	if (!CloseHandle(handle->file))
	{
		return CRIFS_IO_ERROR_NG;
	}

	delete handle;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_GetFileSize(CriFsFileHn filehn, CriSint64* file_size)
{
	if (!filehn || !file_size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoWinHandle*>(filehn);
	*file_size = handle->size;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_Read(CriFsFileHn filehn, CriSint64 offset, CriSint64 read_size, void* buffer, CriSint64 buffer_size)
{
	if (!filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoWinHandle*>(filehn);
	if (offset != handle->bytes_read)
	{
		LARGE_INTEGER offset_li;
		offset_li.QuadPart = offset;
		if (!SetFilePointerEx(handle->file, offset_li, nullptr, FILE_BEGIN))
		{
			return CRIFS_IO_ERROR_NG;
		}
	}
}


CriFsIoInterfaceTag win_io_interface
{
	CriFsIoWin_Exists,
	CriFsIoWin_Remove,
	CriFsIoWin_Rename,
	CriFsIoWin_Open,
	CriFsIoWin_Close,
	CriFsIoWin_GetFileSize,

};