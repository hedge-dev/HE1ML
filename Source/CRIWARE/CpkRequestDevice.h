#pragma once

struct FileLoadRequest;
struct CriFsIoInterfaceTag;
extern CriFsIoInterfaceTag cpk_req_io_interface;
FileLoadRequest* CpkRequestFromString(const char* path);

inline void CpkRequestToString(char* buffer, FileLoadRequest* request)
{
	sprintf(buffer, "REQ:\\%p", request);
}