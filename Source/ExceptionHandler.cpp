#include "ExceptionHandler.h"
#include <DbgHelp.h>

LONG WINAPI HandleException(PEXCEPTION_POINTERS pExceptionInfo)
{
	MessageBoxA(nullptr, "Unhandled exception", "Error", MB_OK | MB_ICONERROR);
	if (!CreateDirectoryA("./crashdumps", nullptr))
	{
		if (GetLastError() != ERROR_ALREADY_EXISTS)
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}
	}

	char pathbuf[4096];
	GetModuleFileNameA(GetModuleHandleA(nullptr), pathbuf, sizeof(pathbuf));

	if (snprintf(pathbuf, sizeof(pathbuf), "./crashdumps/%s.%lu.dmp", path_filename(pathbuf), GetCurrentProcessId()) < 0)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	const HANDLE hFile = CreateFileA(pathbuf, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{ GetCurrentThreadId(), pExceptionInfo, false };

	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &exceptionInfo, nullptr, nullptr);

	CloseHandle(hFile);
	return EXCEPTION_CONTINUE_SEARCH;
}

void InitExceptionHandler()
{
	SetUnhandledExceptionFilter(HandleException);
}
