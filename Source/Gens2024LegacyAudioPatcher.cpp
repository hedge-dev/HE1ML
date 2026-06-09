#include <memory>
#include <ankerl/unordered_dense.h>
#include <rad/rad_memory_stream.h>
#include <hedgelib/cri/hl_cri_atom_cue_sheet.h>
#include <hedgelib/cri/hl_cri_atom_wave_bank.h>
#include "Gens2024LegacyAudioPatcher.h"
#include "Gens2024LegacyAudioParser.h"

using namespace hl::cri;

namespace gens2024
{
	using ACBPatchInfoMap = ankerl::unordered_dense::map<
		std::string_view,
		rad::span<const ACBPatchInfo>
	>;

	static std::unique_ptr<ACBPatchInfoMap> patchInfoMap;

#include "Gens2024LegacyAudioACBPatchInfo.inl"

	/*
	static const ACBPatchInfo patch_info_SNG02_CPZ[] =
	{
		{ 2, 2, "Synth/EMBB011_CPZ_3D_wav.aax" },
		{ 3, 3, "Synth/EMBB011_CPZ_3D_FXd_wav.aax" }
	};

	void InitLegacyAudioPatcher()
	{
		patchInfoMap.reset(new ACBPatchInfoMap{
			{ "SNG02_CPZ", patch_info_SNG02_CPZ }
		});
	}
	*/

	// TODO: This function should probably be part of HedgeLib
	static const atom::raw_wave_bank_entry* GetAWBEntryById(
		const atom::wave_bank_deserializer& dr,
		hl::u16 id) noexcept
	{
		for (const auto& entry : dr.entries())
		{
			if (entry.id == id)
			{
				return &entry;
			}
		}
		
		return nullptr;
	}

	void PatchACB(
		hl::cri::atom::cue_sheet& acb,
		const SoundElementStreamingInfo& streamingInfo)
	{
		assert(patchInfoMap != nullptr &&
			"The LegacyAudioPatcher MUST be initialized before calling "
			"PatchACB !!!"
		);

		const auto it = patchInfoMap->find(acb.name);
		if (it == patchInfoMap->end())
		{
			throw std::runtime_error(
				"HE1ML has no built-in patch info for this cue sheet"
			);
		}

		const auto& patchInfos = it->second;

		auto soundElementInfos = GetSoundElementInfosForPatch(
			patchInfos,
			streamingInfo
			// TODO: Pass in allocator??
		);

		const auto newWaveformsOffset = acb.waveforms.size();

		acb.waveforms.reserve(
			acb.waveforms.size() +
			soundElementInfos.totalSegmentCount
		);

		hl::u16 curAwbId = 0;
		std::size_t totalPatched = 0;

		for (std::size_t i = 0; i < patchInfos.size(); ++i)
		{
			const auto& patchInfo = patchInfos[i];
			auto& soundElementInfo = soundElementInfos.data[i];

			// NOTE: We use a sampleRate of 0 as a marker that no replacement
			// data was found for this waveform, and we shouldn't patch it.
			if (soundElementInfo.sampleRate == 0)
			{
				if (patchInfo.waveformIndex > acb.waveforms.size())
				{
					throw std::runtime_error(
						"HE1ML built-in patch info for this cue sheet was incorrect; "
						"invalid waveform index"
					);
				}

				const auto& waveform = acb.waveforms[patchInfo.waveformIndex];
				if (waveform.streamAwbId != UINT16_MAX)
				{
					// NOTE: Include this waveform in the new streaming AWB
					// even though we're not going to patch it with redirect
					// data, since we have to replace the whole streaming AWB
					// anyway.
					++curAwbId;
				}

				continue;
			}

			const auto firstNewAcbSynthIndex = acb.synths.size();

			for (unsigned char segmentIndex = 0;
				segmentIndex < soundElementInfo.segmentCount;
				++segmentIndex)
			{
				const auto& segment = soundElementInfo.segments[segmentIndex];

				// Generate ACB waveform.
				assert(acb.waveforms.size() < acb.waveforms.capacity() &&
					"Waveform count exceeded capacity; this should never happen!"
				);

				const auto newWaveformIdx = static_cast<hl::u16>(acb.waveforms.size());

				acb.waveforms.emplace_back_unchecked(
					UINT16_MAX,								// memoryAwbId
					curAwbId++,								// streamAwbId
					atom::waveform_encode_type::adx,		// encodeType
					atom::waveform_stream_type::stream,		// streamType
					atom::waveform_loop_type::one_shot,		// loopType
					soundElementInfo.channelCount,			// channelCount
					soundElementInfo.sampleRate,			// sampleRate
					segment.sampleCount,					// sampleCount
					0										// streamAwbPort
				);

				auto& acbSynth = acb.synths.emplace_back(
					atom::synth_type::polyphonic,
					acb.synths.allocator()
				);

				// TODO: Set voiceLimitGroupName !
				acbSynth.commandIndex = patchInfo.synthCommandIndex;

				acbSynth.refItems.emplace_back(
					atom::ref_type::waveform,
					newWaveformIdx
				);
			}

			// Patch ACB track.
			if (patchInfo.trackIndex > acb.tracks.size())
			{
				throw std::runtime_error(
					"HE1ML built-in patch info for this cue sheet was incorrect; "
					"invalid track index"
				);
			}

			const auto& track = acb.tracks[patchInfo.trackIndex];
			LOG("Patching 2024 ACB track %d...", (void*)patchInfo.trackIndex);

			if (track.eventIndex > acb.trackEventCommands.size())
			{
				throw std::runtime_error(
					"ACB track had invalid track event command index"
				);
			}

			auto& evCmd = acb.trackEventCommands[track.eventIndex];

			if (evCmd.rawData.size() != 10)
			{
				LOG("WARNING: Not yet implemented case encountered when patching track event %d...", (void*)evCmd.rawData.size());
			}

			evCmd.rawData.clear(); // TODO: Locate and replace just the one synth play command, and keep the rest of the track events in-tact !!!

			InsertTrackEventPlaySynthCommands(
				evCmd,
				evCmd.end(),
				soundElementInfo,
				firstNewAcbSynthIndex
			);

			evCmd.append_no_op();

			++totalPatched;
		}

		// Return early if we didn't patch anything.
		if (totalPatched == 0) return;

		// Patch any unpatched streaming AWB IDs, since we're about to
		// replace the whole streaming AWB TOC with a fake HE1ML one.
		if (!acb.streamAwbTocData.empty())
		{
			//MessageBoxA(NULL, "wuh oh", "YAY", MB_OK);
			// TODO: Validate acb.streamAwbTocData.size() == 1
			rad::readonly_memory_stream awbStream{ acb.streamAwbTocData[0] };
			atom::wave_bank_deserializer dr(awbStream);

			curAwbId = 0;

			for (std::size_t i = 0; i < patchInfos.size(); ++i)
			{
				const auto& patchInfo = patchInfos[i];
				auto& soundElementInfo = soundElementInfos.data[i];

				// NOTE: We use a sampleRate of 0 as a marker that no replacement
				// data was found for this waveform, and we shouldn't patch it.
				if (soundElementInfo.sampleRate != 0)
				{
					curAwbId += soundElementInfo.segmentCount;
					continue;
				}

				// Patch ACB waveform.
				if (patchInfo.waveformIndex > acb.waveforms.size())
				{
					throw std::runtime_error(
						"HE1ML built-in patch info for this cue sheet was incorrect; "
						"invalid waveform index"
					);
				}

				auto& waveform = acb.waveforms[patchInfo.waveformIndex];
				if (waveform.streamAwbId == UINT16_MAX)
				{
					continue;
				}

				const auto cleanAwbEntry = GetAWBEntryById(dr, waveform.streamAwbId);

				waveform.streamAwbId = curAwbId++;

				// Fix sound element info.
				if (!cleanAwbEntry)
				{
					throw std::runtime_error(
						"ACB waveform references AWB entry whose ID was not found "
						"in the streaming AWB TOC"
					);
				}

				const auto cleanAwbDataPos = hl::align<hl::u32>(
					cleanAwbEntry->unalignedDataPos,
					dr.info().dataAlignment
				);

				const auto dataSize = (
					(cleanAwbEntry + 1)->unalignedDataPos -
					cleanAwbDataPos
				);

				soundElementInfo.segmentCount = 1;
				soundElementInfo.streamingFilePath = streamingInfo.cleanAwbPath;

				soundElementInfo.segments[0].streamingDataPos = cleanAwbDataPos;
				soundElementInfo.segments[0].dataSize = dataSize;
			}
		}

		// Generate new streaming AWB TOC.
		{
			rad::memory_stream awbStream; // TODO: Pass allocator
			GenerateStreamingAWB(soundElementInfos, awbStream);

			acb.streamAwbTocData.clear();
			acb.streamAwbTocData.emplace_back(awbStream.release());

			if (acb.streamAwbHashes.empty())
			{
				acb.streamAwbHashes.emplace_back(
					acb.name,
					atom::md5_hash{}
				);
			}
		}
	}
}
