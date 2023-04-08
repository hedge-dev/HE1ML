#pragma once
struct CriFsIoInterfaceTag;
class VirtualFileSystem;

void CriFsIoML_AddDevice(const CriFsIoInterfaceTag& device);
size_t CriFsIoML_ResolveDevice(const char* path);
void CriFsIoML_SetVFS(VirtualFileSystem* vfs);

extern CriFsIoInterfaceTag ml_io_interface;