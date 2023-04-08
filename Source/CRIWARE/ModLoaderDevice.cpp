#include "Criware.h"
#include "ModLoaderDevice.h"
#include <VirtualFileSystem.h>

struct CriFsIoMLHandle
{
	std::string name{};
	CriFsFileHn handle{};
	const CriFsIoInterfaceTag* interface {};
};

namespace
{
	std::vector<const CriFsIoInterfaceTag*> g_devices{};
	VirtualFileSystem* g_vfs{};
	std::unordered_set<CriFsIoMLHandle*> g_handles{};
}

CriFsIoError CRIAPI CriFsIoML_Exists(const CriChar8* path, CriBool* result)
{
	if (!path || !result)
	{
		return CRIFS_IO_ERROR_NG;
	}

	*result = CriFsIoML_ResolveDevice(path) != -1;
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoML_Remove(const CriChar8* path)
{
	if (!path)
	{
		return CRIFS_IO_ERROR_NG;
	}

	for (const auto& device : g_devices)
	{
		if (device->Remove && device->Remove(path) == CRIFS_IO_ERROR_OK)
		{
			return CRIFS_IO_ERROR_OK;
		}
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_Rename(const CriChar8* old_path, const CriChar8* new_path)
{
	if (!old_path || !new_path)
	{
		return CRIFS_IO_ERROR_NG;
	}

	for (const auto& device : g_devices)
	{
		if (device->Rename && device->Rename(old_path, new_path) == CRIFS_IO_ERROR_OK)
		{
			return CRIFS_IO_ERROR_OK;
		}
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_Open(const CriChar8* path, CriFsFileMode mode, CriFsFileAccess access, CriFsFileHn* filehn)
{
	if (!path || !filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	if (strstr(path, "HML\\"))
	{
		path += 4;
	}

	std::string newPath{};
	if (g_vfs)
	{
		const auto* entry = g_vfs->get_entry(path);
		if (entry)
		{
			newPath = entry->full_path();
		}
	}

	if (!newPath.empty())
	{
		path = newPath.c_str();
	}

	for (const auto& device : g_devices)
	{
		if (device->Open && device->Open(path, mode, access, filehn) == CRIFS_IO_ERROR_OK)
		{
			auto* handle = new CriFsIoMLHandle();
			handle->name = path;
			handle->handle = *filehn;
			handle->interface = device;
			*filehn = reinterpret_cast<CriFsFileHn>(handle);
			g_handles.insert(handle);
			return CRIFS_IO_ERROR_OK;
		}
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_Close(CriFsFileHn filehn)
{
	if (!filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->Close)
	{
		handle->interface->Close(handle->handle);
	}

	delete handle;
	g_handles.erase(handle);
	return CRIFS_IO_ERROR_OK;
}

CriFsIoError CRIAPI CriFsIoML_GetFileSize(CriFsFileHn filehn, CriSint64* size)
{
	if (!filehn || !size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->GetFileSize)
	{
		return handle->interface->GetFileSize(handle->handle, size);
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_Read(CriFsFileHn filehn, CriSint64 offset, CriSint64 read_size, void* buffer, CriSint64 buffer_size)
{
	if (!filehn || !buffer || !buffer_size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->Read)
	{
		return handle->interface->Read(handle->handle, offset, read_size, buffer, buffer_size);
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_IsReadComplete(CriFsFileHn filehn, CriBool* result)
{
	if (!filehn || !result)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->IsReadComplete)
	{
		return handle->interface->IsReadComplete(handle->handle, result);
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_CancelRead(CriFsFileHn filehn)
{
	if (!filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->CancelRead)
	{
		return handle->interface->CancelRead(handle->handle);
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_GetReadSize(CriFsFileHn filehn, CriSint64* size)
{
	if (!filehn || !size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->GetReadSize)
	{
		return handle->interface->GetReadSize(handle->handle, size);
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_Write(CriFsFileHn filehn, CriSint64 offset, CriSint64 write_size, void* buffer, CriSint64 buffer_size)
{
	if (!filehn || !buffer || !buffer_size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->Write)
	{
		return handle->interface->Write(handle->handle, offset, write_size, buffer, buffer_size);
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_IsWriteComplete(CriFsFileHn filehn, CriBool* result)
{
	if (!filehn || !result)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->IsWriteComplete)
	{
		return handle->interface->IsWriteComplete(handle->handle, result);
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_CancelWrite(CriFsFileHn filehn)
{
	if (!filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->CancelWrite)
	{
		return handle->interface->CancelWrite(handle->handle);
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_GetWriteSize(CriFsFileHn filehn, CriSint64* size)
{
	if (!filehn || !size)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->GetWriteSize)
	{
		return handle->interface->GetWriteSize(handle->handle, size);
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_Flush(CriFsFileHn filehn)
{
	if (!filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->Flush)
	{
		return handle->interface->Flush(handle->handle);
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoError CRIAPI CriFsIoML_Resize(CriFsFileHn filehn, CriSint64 size)
{
	if (!filehn)
	{
		return CRIFS_IO_ERROR_NG;
	}

	const auto* handle = static_cast<CriFsIoMLHandle*>(filehn);
	if (handle->interface->Resize)
	{
		return handle->interface->Resize(handle->handle, size);
	}

	return CRIFS_IO_ERROR_NG;
}

CriFsIoInterfaceTag ml_io_interface
{
	CriFsIoML_Exists,
	CriFsIoML_Remove,
	CriFsIoML_Rename,
	CriFsIoML_Open,
	CriFsIoML_Close,
	CriFsIoML_GetFileSize,
	CriFsIoML_Read,
	CriFsIoML_IsReadComplete,
	CriFsIoML_CancelRead,
	CriFsIoML_GetReadSize,
	CriFsIoML_Write,
	CriFsIoML_IsWriteComplete,
	CriFsIoML_CancelWrite,
	CriFsIoML_GetWriteSize,
	CriFsIoML_Flush,
	CriFsIoML_Resize,
};

void CriFsIoML_AddDevice(const CriFsIoInterfaceTag& device)
{
	g_devices.push_back(&device);
}

size_t CriFsIoML_ResolveDevice(const char* path)
{
	if (strstr(path, "HML\\"))
	{
		path += 4;
	}

	std::string newPath{};
	if (g_vfs)
	{
		const auto* entry = g_vfs->get_entry(path);
		if (entry != nullptr)
		{
			newPath = entry->full_path();
		}
	}

	if (!newPath.empty())
	{
		path = newPath.c_str();
	}

	CriBool exists = false;
	for (const auto& device : g_devices)
	{
		if (device->Exists && device->Exists(path, &exists) == CRIFS_IO_ERROR_OK && exists)
		{
			return &device - g_devices.data();
		}
	}

	return -1;
}

void CriFsIoML_SetVFS(VirtualFileSystem* vfs)
{
	g_vfs = vfs;
}
