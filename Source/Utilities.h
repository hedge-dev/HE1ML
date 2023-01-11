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

Buffer* make_buffer(size_t size);
Buffer* read_file(const char* path);
std::string string_trim(const char* str, const char* s = nullptr);
void string_split(const char* str, const char* sep, std::vector<std::string>&out, bool trim_whitespace = true);
int32_t get_deterministic_hash_code(const void* memory, size_t length);