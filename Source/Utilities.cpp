#include "Pch.h"
#include "Utilities.h"

std::unordered_set<std::string> g_string_symbols{};
void Buffer::free()
{
	if (flags & 1)
	{
		delete[] memory;
		memory = nullptr;
		size = 0;
		flags &= ~1;
	}
}

void Buffer::resize(size_t in_size)
{
	if (in_size < size)
	{
		return;
	}

	auto* new_memory = new uint8_t[in_size];
	memcpy(new_memory, memory, size);
	free();

	memory = new_memory;
	size = in_size;
	flags |= 1;
}

Buffer::~Buffer()
{
	free();
}

HMODULE LoadSystemLibrary(const char* name)
{
	std::string windir{};
	DWORD windirSize = GetSystemDirectoryA(nullptr, 0);

	windir.reserve(windirSize + strlen(name) + 1);
	windir.resize(windirSize + strlen(name));

	GetSystemDirectoryA(windir.data(), windirSize);

	windir[windirSize - 1] = '\\';
	strcpy(windir.data() + windirSize, name);

	return LoadLibraryA(windir.c_str());
}

const char* make_string_symbol(const std::string_view& str)
{
	return g_string_symbols.insert(std::string(str)).first->c_str();
}

Buffer* make_buffer(size_t size)
{
	return new Buffer(size);
}

Buffer* read_file(const char* path, bool text_file)
{
	const HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return nullptr;
	}

	auto size = GetFileSize(hFile, nullptr);
	auto* buffer = make_buffer(text_file ? size + 1 : size);
	ReadFile(hFile, buffer->memory, buffer->size, nullptr, nullptr);
	CloseHandle(hFile);

	if (text_file)
	{
		buffer->memory[buffer->size - 1] = 0;
	}

	return buffer;
}

bool file_exists(const char* path)
{
	const auto attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool file_exists(const std::filesystem::path& path)
{
	const auto attributes = GetFileAttributesW(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::string strtrim(const char* str, const char* s)
{
	if (!str)
	{
		return "";
	}

	if (!s)
	{
		s = " ";
	}

	const auto* begin = str;
	const auto* end = str + strlen(str);

	while (begin < end && strchr(s, *begin))
	{
		++begin;
	}

	while (end > begin && strchr(s, *(end - 1)))
	{
		--end;
	}

	return { begin, end };
}

void strsplit(const char* str, const char* sep, std::vector<std::string>& out, bool trim_whitespace)
{
	if (!str)
	{
		return;
	}

	if (!sep)
	{
		sep = " ";
	}

	const auto* begin = str;
	const auto* end = str + strlen(str);

	while (begin < end)
	{
		const auto* pos = strstr(begin, sep);
		if (!pos)
		{
			pos = end;
		}

		if (trim_whitespace)
		{
			out.push_back(strtrim(std::string(begin, pos).c_str()));
		}
		else
		{
			out.emplace_back(begin, pos);
		}

		begin = pos + strlen(sep);
	}
}

const char* rstrstr(const char* str, const char* substr)
{
	if (!str || !substr)
	{
		return nullptr;
	}

	const auto* pos = str + strlen(str) - strlen(substr);
	while (pos >= str)
	{
		if (!strncmp(pos, substr, strlen(substr)))
		{
			return pos;
		}

		--pos;
	}

	return nullptr;
}

char* rstrstr(char* str, const char* substr)
{
	return const_cast<char*>(rstrstr(static_cast<const char*>(str), substr));
}

std::string_view path_dirname(const std::string_view& str)
{
	const auto* pos = rstrstr(str.data(), "\\") + 1;
	if (!pos)
	{
		pos = rstrstr(str.data(), "/") + 1;
	}

	return pos ? std::string_view(str.data(), pos - str.data()) : str;
}

const char* path_filename(const char* str)
{
	const auto* pos = rstrstr(str, "\\");
	if (!pos)
	{
		pos = rstrstr(str, "/");
	}

	return pos ? pos + 1 : str;
}

bool path_rmfilename(char* str)
{
	char* pos = rstrstr(str, "\\");
	if (!pos)
	{
		pos = rstrstr(str, "/");
	}

	if (!pos)
	{
		return false;
	}

	*pos = 0;
	return true;
}

std::string strformat(const std::string_view& text)
{
	std::stringstream ss;

	const char* start = text.data();
	struct
	{
		const char* data;
		bool in;
		char type;

		size_t start;
		size_t length;

		[[nodiscard]] std::string_view string() const
		{
			return { data + start, length };
		}
	} block{ text.data() };

	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] == '{' && i != 0)
		{
			block.in = true;
			block.start = i + 1;
			block.type = text[i - 1];

			if (block.type != 'M' && block.type != 'X')
			{
				block = {};
				ss << text[i];
			}
			else
			{
				ss.seekp(-1, std::ios_base::end);
			}
		}
		else if (text[i] == '}' && block.in)
		{
			block.in = false;
			if (block.type == 'M')
			{
				ss << static_cast<const char*>(strtoptr(block.string()));
			}
			else if (block.type == 'X')
			{
				const auto data = fromhexstr(block.string());
				ss << std::string_view{reinterpret_cast<const char*>(data->memory), data->size};
			}
			else
			{
				ss << text[i];
			}
		}
		else
		{
			if (!block.in)
			{
				ss << text[i];
			}
			else
			{
				block.length++;
			}
		}
	}

	auto s = block.string();

	return ss.str();
}

std::string tostr(const wchar_t* str)
{
	const int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);

	if (len == 0)
	{
		return "";
	}

	std::string utf8(len, 0);
	WideCharToMultiByte(CP_UTF8, 0, str, -1, utf8.data(), len, nullptr, nullptr);

	return utf8;
}

void HandoverProcess(const wchar_t* path)
{
	// These variables get inherited
	SetEnvironmentVariableA(ML_ENVAR_PROCESS_RESTARTED, "1");
	if (IsDebuggerPresent())
	{
		SetEnvironmentVariableA(ML_ENVAR_PROCESS_HAD_DEBUGGER, "1");
	}

	STARTUPINFOW info{};
	PROCESS_INFORMATION procInfo{};
	CreateProcessW(path, GetCommandLineW(), nullptr, nullptr, false, GetPriorityClass(GetCurrentProcess()),
		nullptr, nullptr, &info, &procInfo);

	ExitProcess(1);
}

void RestartIfLargeAddressUnaware()
{
	wchar_t buffer[4096];
	const auto host = GetModuleHandle(nullptr);
	if (CheckLargeAddressAware(host))
	{
		return;
	}

	GetModuleFileNameW(host, buffer, sizeof(buffer));
	std::filesystem::path hostPath{ buffer };
	
	const Win32Handle handle{ OpenFileW(hostPath.c_str()) };
	hostPath.replace_extension(L".HE1ML_LAA.exe");

	if (GetFileAttributesW(hostPath.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		HandoverProcess(hostPath.c_str());
		return;
	}

	if (handle == nullptr)
	{
		return;
	}

	const size_t size{ GetFileSize(handle.get(), nullptr) };
	auto fileBuffer = std::make_unique<uint8_t[]>(size);
	ReadFile(handle.get(), fileBuffer.get(), size, nullptr, nullptr);
	auto* headers = GetNtHeaders(reinterpret_cast<HMODULE>(fileBuffer.get()));

	headers->FileHeader.Characteristics |= IMAGE_FILE_LARGE_ADDRESS_AWARE;
	headers->OptionalHeader.DllCharacteristics &= ~IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;

	{
		const Win32Handle laaHandle{ CreateFileW(hostPath.c_str()) };
		WriteFile(laaHandle.get(), fileBuffer.get(), size, nullptr, nullptr);
	}

	HandoverProcess(hostPath.c_str());
}
