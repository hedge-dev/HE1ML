#pragma once
struct CriFsIoInterfaceTag;
class VirtualFileSystem;

void CriFsIoML_AddDevice(const CriFsIoInterfaceTag& device);
size_t CriFsIoML_ResolveDevice(const char* path);

extern CriFsIoInterfaceTag ml_io_interface;