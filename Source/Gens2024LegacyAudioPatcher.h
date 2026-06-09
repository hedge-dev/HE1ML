#pragma once
#include <string_view>
#include <rad/rad_span.h>

namespace hl::cri::atom
{
	struct cue_sheet;
}

namespace gens2024
{
	struct SoundElementStreamingInfo;

	struct ACBPatchInfo
	{
		unsigned short trackIndex;
		unsigned short waveformIndex;
		unsigned short synthCommandIndex;
		std::string_view localStreamingPath;
	};

	void InitLegacyAudioPatcher();

	void PatchACB(
		hl::cri::atom::cue_sheet& acb,
		const SoundElementStreamingInfo& streamingInfo
	);
}
