#include "Pch.h"
#include "Utilities.h"

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

Buffer* make_buffer(size_t size)
{
	return new Buffer(size);
}

Buffer* read_file(const char* path)
{
	const HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
	if (!hFile)
	{
		return nullptr;
	}

	auto* buffer = make_buffer(GetFileSize(hFile, nullptr));
	ReadFile(hFile, buffer->memory, buffer->size, nullptr, nullptr);
	CloseHandle(hFile);
	return buffer;
}

std::string string_trim(const char* str, const char* s)
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

void string_split(const char* str, const char* sep, std::vector<std::string>& out, bool trim_whitespace)
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
			out.push_back(string_trim(std::string(begin, pos).c_str()));
		}
		else
		{
			out.emplace_back(begin, pos);
		}

		begin = pos + strlen(sep);
	}
}

int32_t get_deterministic_hash_code(const void* memory, size_t length)
{
	int hash1 = (5381 << 16) + 5381;
	int hash2 = hash1;

	for (int i = 0; i < length; i += 2)
	{
		hash1 = ((hash1 << 5) + hash1) ^ static_cast<const char*>(memory)[i];
		if (i == length - 1)
		{
			break;
		}

		hash2 = ((hash2 << 5) + hash2) ^ static_cast<const char*>(memory)[i + 1];
	}

	return hash1 + (hash2 * 1566083941);
}