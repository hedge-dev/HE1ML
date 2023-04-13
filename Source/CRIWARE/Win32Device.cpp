#include "Criware.h"
#include "Win32Device.h"
#include <FileBinder.h>

struct CriFsIoWinHandle
{
	HANDLE file{};
	std::string name{};
	CriUint64 size{};
	DWORD bytes_read{};
	DWORD bytes_written{};
};

CriFsIoError CRIAPI CriFsIoWin_Exists(const CriChar8* path, CriBool* result)
{
	if (!path || !result)
	{
		return CRIFS_IO_ERROR_NG;
	}

	if (g_binder && g_binder->FileExists(path) == eBindError_None)
	{
		*result = true;
		return CRIFS_IO_ERROR_OK;
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

	std::string replacePath{};
	if (g_binder && g_binder->ResolvePath(path, &replacePath))
	{
		path = replacePath.c_str();
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

	auto* handle = static_cast<CriFsIoWinHandle*>(filehn);
	LARGE_INTEGER offset_li;
	offset_li.QuadPart = offset;
	if (!SetFilePointerEx(handle->file, offset_li, nullptr, FILE_BEGIN))
	{
		return CRIFS_IO_ERROR_NG;
	}

	if (!ReadFile(handle->file, buffer, static_cast<DWORD>(read_size), &handle->bytes_read, nullptr))
	{
		return CRIFS_IO_ERROR_NG;
	}

	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_IsReadComplete(CriFsFileHn filehn, CriBool* complete)
{
	if (!filehn || !complete)
	{
		return CRIFS_IO_ERROR_NG;
	}

	*complete = true;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_GetReadSize(CriFsFileHn filehn, CriSint64* size)
{
	if (!filehn || !size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoWinHandle*>(filehn);
	*size = handle->bytes_read;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_Write(CriFsFileHn filehn, CriSint64 offset, CriSint64 write_size, void* buffer, CriSint64 buffer_size)
{
	auto* handle = static_cast<CriFsIoWinHandle*>(filehn);
	LARGE_INTEGER offset_li;
	offset_li.QuadPart = offset;
	if (!SetFilePointerEx(handle->file, offset_li, nullptr, FILE_BEGIN))
	{
		return CRIFS_IO_ERROR_NG;
	}

	if (!WriteFile(handle->file, buffer, static_cast<DWORD>(write_size), &handle->bytes_written, nullptr))
	{
		return CRIFS_IO_ERROR_NG;
	}

	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_GetWriteSize(CriFsFileHn filehn, CriSint64* size)
{
	if (!filehn || !size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoWinHandle*>(filehn);
	*size = handle->bytes_written;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_Flush(CriFsFileHn filehn)
{
	const auto* handle = static_cast<CriFsIoWinHandle*>(filehn);
	if (!FlushFileBuffers(handle->file))
	{
		return CRIFS_IO_ERROR_NG;
	}

	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_Resize(CriFsFileHn filehn, CriSint64 size)
{
	const auto* handle = static_cast<CriFsIoWinHandle*>(filehn);
	LARGE_INTEGER size_li;
	size_li.QuadPart = size;
	if (!SetFilePointerEx(handle->file, size_li, nullptr, FILE_BEGIN))
	{
		return CRIFS_IO_ERROR_NG;
	}

	if (!SetEndOfFile(handle->file))
	{
		return CRIFS_IO_ERROR_NG;
	}

	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoWin_GetNativeFileHandle(CriFsFileHn filehn, void** native_filehn)
{
	if (!filehn || !native_filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoWinHandle*>(filehn);
	*native_filehn = handle->file;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoInterfaceTag win_io_interface
{
	CriFsIoWin_Exists,
	CriFsIoWin_Remove,
	CriFsIoWin_Rename,
	CriFsIoWin_Open,
	CriFsIoWin_Close,
	CriFsIoWin_GetFileSize,
	CriFsIoWin_Read,
	CriFsIoWin_IsReadComplete,
	nullptr,
	CriFsIoWin_GetReadSize,
	CriFsIoWin_Write,
	CriFsIoWin_IsReadComplete,
	nullptr,
	CriFsIoWin_GetWriteSize,
	CriFsIoWin_Flush,
};
