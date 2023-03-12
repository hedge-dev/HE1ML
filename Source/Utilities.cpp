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