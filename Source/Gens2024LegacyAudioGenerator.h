#pragma once
#include <rad/rad_memory_stream.h>
#include <string_view>

namespace gens2024
{
	void InitializeLegacyACBTemplates();

	bool TryGenerateACB(
		rad::memory_stream& acbOutputStream,
		std::string_view name
		//StreamingDataType streamDataType = StreamingDataType::None,
		//const char* streamDataPath = nullptr
	);
}
