#pragma once
#include <set>

template<typename T, typename TCompare = std::greater<T>>
using priority_queue = std::multiset<T, TCompare>;

struct Win32HandleDeleter
{
	void operator()(HANDLE hFile)
	{
		CloseHandle(hFile);
	}
};

typedef std::unique_ptr<void, Win32HandleDeleter> Win32Handle;

class Buffer
{
	uint8_t flags{};

public:
	size_t size{};
	uint8_t* memory{};

	Buffer(size_t in_size)
	{
		memory = new uint8_t[in_size];
		size = in_size;
		flags |= 1;
	}

	Buffer(void* in_memory, size_t in_size, bool owns = false) : flags(owns ? 1 : 0), size(in_size), memory(static_cast<uint8_t*>(in_memory))
	{

	}

	void free();
	void resize(size_t in_size);
	~Buffer();
};

template<typename TChar>
struct path_constants
{
	constexpr static TChar dir_separator = static_cast<TChar>('/');
	constexpr static TChar alt_dir_separator = static_cast<TChar>('\\');
	constexpr static TChar volume_separator = static_cast<TChar>(':');

	constexpr static bool is_valid_drive(TChar c)
	{
		return ((c | 0x20) - static_cast<TChar>('a')) <= static_cast<TChar>('z' - 'a');
	}
};

template<typename TChar>
constexpr bool is_dir_separator(TChar c)
{
	return c == path_constants<TChar>::dir_separator || c == path_constants<TChar>::alt_dir_separator;
}

HMODULE LoadSystemLibrary(const char* name);
const char* make_string_symbol(const std::string_view& str);
Buffer* make_buffer(size_t size);
Buffer* read_file(const char* path, bool text_file = false);
bool file_exists(const char* path);
bool file_exists(const std::filesystem::path& path);
std::string strtrim(const char* str, const char* s = nullptr);
void strsplit(const char* str, const char* sep, std::vector<std::string>& out, bool trim_whitespace = true);
const char* rstrstr(const char* str, const char* substr);
char* rstrstr(char* str, const char* substr);
const char* path_filename(const char* str);
std::string_view path_dirname(const std::string_view& str);
bool path_rmfilename(char* str);
std::string strformat(const std::string_view& text);
std::string tostr(const wchar_t* str);
void RestartIfLargeAddressUnaware();

inline IMAGE_NT_HEADERS* GetNtHeaders(HMODULE module)
{
	return reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<size_t>(module) + reinterpret_cast<IMAGE_DOS_HEADER*>(module)->e_lfanew);
}

inline bool CheckLargeAddressAware(HMODULE module)
{
	return !!(GetNtHeaders(module)->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE);
}

inline HANDLE OpenFileW(const wchar_t* path)
{
	return CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

inline HANDLE CreateFileW(const wchar_t* path)
{
	return CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

template<typename TChar>
constexpr bool path_is_rooted(const TChar* path)
{
	auto size = std::char_traits<TChar>::length(path);
	return (size >= 1 && is_dir_separator(path[0])) || (size >= 2 && path_constants<TChar>::is_valid_drive(path[0]) && path[1] == path_constants<TChar>::volume_separator);
}

template<bool FromStart = true>
std::string_view path_noextension(const std::string_view name)
{
	const auto pos = FromStart ? name.find_last_of('.') : name.find_first_of('.');
	if (pos == std::string_view::npos)
	{
		return name;
	}

	return name.substr(0, pos);
}

constexpr char tolower_c(char c)
{
	if (c >= 'A' && c <= 'Z')
	{
		return c + ('a' - 'A');
	}
	return c;
}

constexpr char fromhex(const char* str)
{
	constexpr char zero = '0';
	constexpr char a = 'a';

	const auto lo = tolower_c(str[1]);
	const auto hi = tolower_c(str[0]);

	char result = 0;

	if (lo >= '0' && lo <= '9')
	{
		result |= lo - zero;
	}
	else if (lo >= 'a' && lo <= 'f')
	{
		result |= lo - a + 10;
	}

	if (hi >= '0' && hi <= '9')
	{
		result |= (hi - zero) << 4;
	}
	else if (hi >= 'a' && hi <= 'f')
	{
		result |= (hi - a + 10) << 4;
	}

	return result;
}

constexpr std::uint16_t tohex(char c)
{
	constexpr char zero = '0';
	constexpr char a = 'A';

	const auto lo = c & 0xF;
	const auto hi = (c >> 4) & 0xF;

	std::uint16_t result = 0;

	// Little endianness
	result |= (lo < 10 ? lo + zero : lo - 10 + a) << 8;
	result |= hi < 10 ? hi + zero : hi - 10 + a;

	return result;
}

constexpr void fromhexstr(const std::string_view& str, void* buffer, size_t buffer_len)
{
	if (buffer_len < str.size() / 2)
	{
		return;
	}

	for (size_t i = 0; i < str.size() / 2; i++)
	{
		static_cast<char*>(buffer)[i] = fromhex(str.data() + i * 2);
	}
}

constexpr size_t hexstr(const void* data, size_t len, char* buffer, size_t buffer_len)
{
	if (buffer_len < len * 2)
	{
		return 0;
	}

	for (size_t i = 0; i < len; i++)
	{
		const auto hex = tohex(static_cast<const char*>(data)[i]);
		reinterpret_cast<uint16_t*>(buffer)[i] = hex;
	}

	if (buffer_len > len * 2)
	{
		buffer[len * 2] = 0;
	}

	return len * 2;
}

constexpr std::string hexstr(const void* data, size_t len)
{
	std::string result;
	result.resize(len * 2);
	hexstr(data, len, result.data(), result.capacity());

	return result;
}

constexpr std::string hexstr(const std::string_view& str)
{
	return hexstr(str.data(), str.size());
}

inline std::unique_ptr<Buffer> fromhexstr(const std::string_view& str)
{
	auto buffer = std::make_unique<Buffer>(str.size() / 2);
	fromhexstr(str, buffer->memory, buffer->size);

	return buffer;
}

constexpr size_t ptrtostr(size_t ptr, char* buffer)
{
	constexpr char zero = '0';
	constexpr char a = 'a';

	for (int i = (sizeof(size_t) * 2) - 1; i >= 0; --i)
	{
		auto digit = ptr & 0xF;
		ptr >>= 4;
		buffer[i] = digit < 10 ? zero + digit : a + (digit - 10);
	}

	buffer[sizeof(size_t) * 2] = 0;
	return sizeof(size_t) * 2;
}

constexpr void* strtoptr(const std::string_view& str)
{
	size_t ptr = 0;

	for (int i = 0; i < std::min(str.size(), sizeof(size_t) * 2); ++i)
	{
		ptr <<= 4;
		auto c = str[i];
		if (c >= '0' && c <= '9')
		{
			ptr |= c - '0';
		}
		else if (c >= 'a' && c <= 'f')
		{
			ptr |= c - 'a' + 10;
		}
		else if (c >= 'A' && c <= 'F')
		{
			ptr |= c - 'A' + 10;
		}
		else
		{
			return nullptr;
		}
	}

	return reinterpret_cast<void*>(ptr);
}

constexpr std::string ptrtostr(size_t ptr)
{
	char buffer[(sizeof(size_t) * 2) + 1]{};
	ptrtostr(ptr, buffer);
	return buffer;
}

// Compatible with HMM
template<bool CaseInsensitive = false>
constexpr size_t strhash(const std::string_view& str)
{
	int hash1 = (5381 << 16) + 5381;
	int hash2 = hash1;

	const size_t length = str.size();
	for (int i = 0; i < length; i += 2)
	{
		hash1 = ((hash1 << 5) + hash1) ^ (CaseInsensitive ? tolower_c(str[i]) : str[i]);
		if (i == length - 1)
		{
			break;
		}

		hash2 = ((hash2 << 5) + hash2) ^ (CaseInsensitive ? tolower_c(str[i + 1]) : str[i + 1]);
	}

	return hash1 + (hash2 * 1566083941);
}

inline std::vector<std::string> strsplit(const char* str, const char* sep, bool trim_whitespace = true)
{
	std::vector<std::string> out;
	strsplit(str, sep, out, trim_whitespace);
	return out;
}