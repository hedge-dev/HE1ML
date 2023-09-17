#include "ExceptionHandler.h"
#include <DbgHelp.h>

LPTOP_LEVEL_EXCEPTION_FILTER LastExceptionHandler{};

LONG ContinueExceptionHandling(PEXCEPTION_POINTERS pExceptionInfo)
{
	if (LastExceptionHandler != nullptr)
	{
		return LastExceptionHandler(pExceptionInfo);
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI HandleException(PEXCEPTION_POINTERS pExceptionInfo)
{
	char exeName[1024];
	char pathbuf[4096];

	GetModuleFileNameA(GetModuleHandleA(nullptr), pathbuf, sizeof(pathbuf));

	strcpy(exeName, path_filename(pathbuf));
	auto root = path_dirname(pathbuf);
	pathbuf[root.length()] = 0;

	strcpy(pathbuf + root.length(), "crashdumps\\");

	if (!CreateDirectoryA(pathbuf, nullptr))
	{
		if (GetLastError() != ERROR_ALREADY_EXISTS)
		{
			return ContinueExceptionHandling(pExceptionInfo);
		}
	}

	snprintf(pathbuf + strlen(pathbuf), sizeof(pathbuf), "%s.%lu.dmp", exeName, GetCurrentProcessId());

	const HANDLE hFile = CreateFileA(pathbuf, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return ContinueExceptionHandling(pExceptionInfo);
	}

	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{ GetCurrentThreadId(), pExceptionInfo, false };

	if (!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &exceptionInfo, nullptr, nullptr))
	{
		LOG("Failed to write crash dump");
	}

	CloseHandle(hFile);
	return ContinueExceptionHandling(pExceptionInfo);
}

HOOK(LPTOP_LEVEL_EXCEPTION_FILTER, WINAPI, MySetUnhandledExceptionFilter, PROC_ADDRESS("kernel32.dll", "SetUnhandledExceptionFilter"), LPTOP_LEVEL_EXCEPTION_FILTER handler)
{
	const LPTOP_LEVEL_EXCEPTION_FILTER filter = LastExceptionHandler;
	LastExceptionHandler = handler;
	return filter;
}

void InitExceptionHandler()
{
	SetUnhandledExceptionFilter(HandleException);
	INSTALL_HOOK(MySetUnhandledExceptionFilter);
}
