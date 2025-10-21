// TODO: Remove this file; we're going to replace this with clean acb patching
#include "Gens2024LegacyAudioGenerator.h"
//#include "Gens2024LegacyAudioUpgrader.h"
#include "Gens2024LegacyAudioParser.h"
#include <hedgelib/cri/hl_cri_atom_cue_sheet.h>
//#include <rad/rad_stack_or_heap_array.h>
#include <ankerl/unordered_dense.h>

using namespace hl::cri_new;

namespace gens2024
{
	struct ACBTrackTemplate
	{
		const char* soundElementPath;
	};

	struct ACBCueTemplate
	{
		const char* name;
		unsigned long id;
		ACBTrackTemplate tracks[3];
		unsigned short trackCount = [](const ACBTrackTemplate(&arr)[3])
		{
			unsigned short count = 0;
			for (const auto& track : arr)
			{
				if (!track.soundElementPath) break;
				++count;
			}

			return count;
		}
		(tracks);
	};

	struct ACBTemplate
	{
		ACBCueTemplate cues[3];
		unsigned short cueCount = [](const ACBCueTemplate(&arr)[3])
		{
			unsigned short count = 0;
			for (const auto& cue : arr)
			{
				if (!cue.name) break;
				++count;
			}

			return count;
		}
		(cues);
	};

	static const char* AcbTemplateNames[] =
	{
#define START_ACB(name) name,
#include "Gens2024LegacyAudioStreamTemplates.h"
	};

	static const ACBTemplate AcbTemplates[] =
	{
#define START_ACB(name) {{
#define START_CUE(id, name) { name, id, {
#define TRACK(soundElementPath) { soundElementPath },
#define END_CUE }},
#define END_ACB }},

#include "Gens2024LegacyAudioStreamTemplates.h"
	};

	static constexpr std::size_t AcbTemplateCount = sizeof(AcbTemplates) / sizeof(*AcbTemplates);

	static ankerl::unordered_dense::map<std::string_view, const ACBTemplate*> AcbTemplateMap;

	void InitializeLegacyACBTemplates()
	{
		AcbTemplateMap.reserve(AcbTemplateCount);

		for (std::size_t i = 0; i < AcbTemplateCount; ++i)
		{
			AcbTemplateMap.try_emplace(AcbTemplateNames[i], AcbTemplates + i);
		}
	}

	static void GenerateACB(
		rad::memory_stream& acbOutputStream,
		const ACBTemplate& acbTmpl,
		std::string_view name)
		//StreamingDataType streamDataType = StreamingDataType::None,
		//const char* streamDataPath = nullptr
	{
		atom::cue_sheet acb;

		//// Get waveform info.
		//using WaveformInfoArr_t = rad::stack_or_heap_array<WaveformInfo, 32>;

		//const auto waveformCount = 0;
		//WaveformInfoArr_t waveformsInfo(
			//csb.soundElements.size()
		//);

		// Generate cues.
		acb.cues.reserve(acbTmpl.cueCount);

		for (unsigned short i = 0; i < acbTmpl.cueCount; ++i)
		{
			const auto& cueTmpl = acbTmpl.cues[i];
			auto& acbCue = acb.cues.emplace_back();

			acbCue.name = rad::string(cueTmpl.name);
			acbCue.id = cueTmpl.id;
			acbCue.refItem.type = atom::reference_type::sequence;
			acbCue.refItem.index = i;
		}

		// Generate sequences and tracks.
		acb.sequences.reserve(acbTmpl.cueCount);

		for (unsigned short i = 0; i < acbTmpl.cueCount; ++i)
		{
			const auto& cueTmpl = acbTmpl.cues[i];
			auto& acbSequence = acb.sequences.emplace_back();

			for (unsigned short i2 = 0; i2 < cueTmpl.trackCount; ++i2)
			{
				const auto& trackTmpl = cueTmpl.tracks[i2];
				const auto acbTrackIndex = static_cast<unsigned short>(acb.tracks.size());
				auto& acbTrack = acb.tracks.emplace_back();

				acbSequence.trackIndices.emplace_back(acbTrackIndex);

				// TODO
			}
		}
		
		// TODO
	}

	bool TryGenerateACB(
		rad::memory_stream& acbOutputStream,
		std::string_view name)
		//StreamingDataType streamDataType = StreamingDataType::None,
		//const char* streamDataPath = nullptr
	{
		const auto it = AcbTemplateMap.find(name);

		if (it == AcbTemplateMap.end())
		{
			LOG("Failed to generate acb for %s - no template for acb with this name", name.data());
			return false;
		}

		try
		{
			GenerateACB(
				acbOutputStream,
				*it->second,
				name
			);
		}
		catch (const std::exception& ex)
		{
			LOG("Failed to generate acb for %s - \"%s\"", name.data(), ex.what());
			return false;
		}
		catch (...)
		{
			LOG("Failed to generate acb for %s", name.data());
			return false;
		}

		return true;
	}
}