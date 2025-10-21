#pragma once
#include <rad/rad_memory_stream.h>
#include <string_view>

namespace gens2024
{
	enum class StreamingDataType
	{
		None = 0,
		Cpk,
		Cpk_redirect_folder,
	};

	bool TryUpgradeCSB(
		rad::memory_stream& acbOutputStream,
		const void* data,
		unsigned long dataSize,
		std::string_view name,
		StreamingDataType streamDataType = StreamingDataType::None,
		const char* streamDataPath = nullptr
	);
}
