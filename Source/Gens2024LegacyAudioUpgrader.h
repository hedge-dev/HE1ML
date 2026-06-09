#pragma once
#include <hedgelib/cri/hl_cri_atom_cue_sheet.h>
#include "Gens2024LegacyAudioParser.h"

namespace gens2024
{
	bool TryUpgradeCSB(
		hl::cri::atom::cue_sheet& outAcb,
		const void* csbData,
		unsigned long csbDataSize,
		const SoundElementStreamingInfo* streamingInfo = nullptr,
		rad::allocator& tmpAllocator = rad::default_allocator,
		rad::allocator& csbAllocator = rad::default_allocator
	);
}
