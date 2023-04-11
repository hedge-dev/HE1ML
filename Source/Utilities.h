#pragma once

class Buffer
{
	uint32_t flags{};

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

HMODULE LoadSystemLibrary(const char* name);
Buffer* make_buffer(size_t size);
Buffer* read_file(const char* path, bool text_file = false);
std::string strtrim(const char* str, const char* s = nullptr);
void strsplit(const char* str, const char* sep, std::vector<std::string>&out, bool trim_whitespace = true);

constexpr char tolower_c(char c)
{
	if (c >= 'A' && c <= 'Z')
	{
		return c + ('a' - 'A');
	}
	return c;
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