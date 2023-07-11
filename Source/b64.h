#pragma once

static const unsigned char base64_table[65] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr size_t b64_len(size_t len)
{
	return 4 * ((len + 2) / 3);
}

constexpr size_t b64_encode(const void* data, size_t size, char* buffer, size_t buffer_size)
{
	if (buffer == nullptr || buffer_size == 0)
	{
		return 0;
	}

	const auto encoded_len = b64_len(size);
	if (buffer_size < encoded_len + 1)
	{
		return 0;
	}

	const auto* in = static_cast<const unsigned char*>(data);
	const auto out = reinterpret_cast<unsigned char*>(buffer);
	unsigned char* pos = out;
	auto* end = in + size;

	pos = out;
	while (end - in >= 3) {
		*pos++ = base64_table[in[0] >> 2];
		*pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
		*pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
		*pos++ = base64_table[in[2] & 0x3f];
		in += 3;
	}

	if (end - in) {
		*pos++ = base64_table[in[0] >> 2];
		if (end - in == 1) {
			*pos++ = base64_table[(in[0] & 0x03) << 4];
			*pos++ = '=';
		}
		else {
			*pos++ = base64_table[((in[0] & 0x03) << 4) |
				(in[1] >> 4)];
			*pos++ = base64_table[(in[1] & 0x0f) << 2];
		}
		*pos++ = '=';
	}

	return encoded_len;
}

constexpr std::string b64_encode(const void* data, size_t size)
{
	std::string buffer{};
	buffer.resize(b64_len(size));
	b64_encode(data, size, buffer.data(), sizeof(buffer));
	return buffer;
}