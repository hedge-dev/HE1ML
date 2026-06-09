#include <exception>
#include <rad/rad_memory_stream.h>
#include <string_view>
#include "Gens2024LegacyAudioUpgrader.h"

using namespace hl::cri;

namespace gens2024
{
	enum ACFCategory
	{
		ACF_CATEGORY_BGM = 0,
		ACF_CATEGORY_SE = 1,
		ACF_CATEGORY_VOICE = 2,
		ACF_CATEGORY_SYSTEM = 3,
		ACF_CATEGORY_SE_EVENT = 4,
	};

	static const std::string_view GlobalAisacControls[] =
	{
		"distance",
		"uw_switch",
		"sn_speed",
		"bob_speed",
		"lock_lv",
		"rkt_speed",
		"sn_speed_fall",
		"rtop",
		"cgw_speed",
		"trl_speed",
		"sd_speed",
		"rev_speed",
		"filter",
		"boost",
		"area",
		"vol"
	};

	static constexpr hl::u16 GlobalAisacControlCount = static_cast<hl::u16>(
		std::size(GlobalAisacControls)
	);

	static hl::u16 GetGlobalAisacControlIndex(std::string_view aisacControlName) noexcept
	{
		for (hl::u16 i = 0; i < GlobalAisacControlCount; ++i)
		{
			if (aisacControlName == GlobalAisacControls[i])
			{
				return i;
			}
		}

		return UINT16_MAX;
	}

	static const audio::sound_element& GetCSBLinkSoundElement(
		const audio::cue_sheet& csb,
		std::string_view linkName)
	{
		const auto csbLinkSoundElement = csb.find_sound_element(linkName);
		if (csbLinkSoundElement == csb.soundElements.end())
		{
			throw std::runtime_error(
				"CSB synth links to a sound element which was not found"
			);
		}

		return *csbLinkSoundElement;
	}

	static const audio::synth& GetCSBLinkSynth(
		const audio::cue_sheet& csb,
		std::string_view linkName)
	{
		const auto csbLinkSynth = csb.find_synth(linkName);
		if (csbLinkSynth == csb.synths.end())
		{
			throw std::runtime_error(
				"CSB synth links to a synth which was not found"
			);
		}

		return *csbLinkSynth;
	}

	static void GenerateACBAisacIndices(
		atom::cue_sheet& outAcb,
		const audio::cue_sheet& csb,
		rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
		const audio::synth& csbSynth,
		rad::vector<hl::u16>& localAisacIndices)
	{
		localAisacIndices.reserve(csbSynth.aisacNames.size());

		for (const auto& csbAisacName : csbSynth.aisacNames)
		{
			const auto csbAisacIt = csb.find_aisac(csbAisacName);
			if (csbAisacIt == csb.aisacs.end())
			{
				// TODO: Make this a warning instead?
				throw std::runtime_error(
					"CSB synth references an AISAC which was not found"
				);
			}

			const auto csbAisacIndex = static_cast<hl::u16>(
				csbAisacIt - csb.aisacs.begin()
			);

			const auto& acbAisac = outAcb.aisacs[csbAisacIndex];
			const auto aisacControlIndex = static_cast<hl::u16>(
				acbAisac.controlId - 1000
			);

			assert((aisacControlIndex / 8) < aisacControlMap.size() &&
				"AISAC control index exceeds allocated AISAC Control Map size; "
				"this should never happen!"
			);

			aisacControlMap[aisacControlIndex / 8] |= (1 << (aisacControlIndex % 8));

			localAisacIndices.push_back_unchecked(csbAisacIndex);
		}
	}

	static void GenerateACBCommands(
		atom::command_table& cmd,
		const audio::synth& csbSynth)
	{
		{
			const auto acbVolume = static_cast<unsigned short>(
				std::round(static_cast<float>(csbSynth.volume) / 10.0f)
			);

			if (acbVolume != 100)
			{
				cmd.append_set_volume(acbVolume);
			}
		}

		if (csbSynth.pitch != 0)
		{
			cmd.append_set_pitch(csbSynth.pitch);
		}

		if (csbSynth.envelopeInfo.attack != 0)
		{
			cmd.append_set_eg_attack(
				csbSynth.envelopeInfo.attack
			);
		}

		if (csbSynth.envelopeInfo.hold != 0)
		{
			cmd.append_set_eg_hold(
				csbSynth.envelopeInfo.hold
			);
		}

		if (csbSynth.envelopeInfo.decay != 0)
		{
			cmd.append_set_eg_decay(
				csbSynth.envelopeInfo.decay
			);
		}

		if (csbSynth.envelopeInfo.release != 0)
		{
			cmd.append_set_eg_release(
				csbSynth.envelopeInfo.release
			);
		}

		if (csbSynth.envelopeInfo.sustain != 1000)
		{
			cmd.append_set_eg_sustain(
				csbSynth.envelopeInfo.sustain
			);
		}

		/*
		if (csbSynth.linkType == audio::synth_link_type::sound_element)
		{
			// TODO: Is this formula correct?
			const auto acbPan3dVolume = (csbSynth.pan3dInfo.volume != 0) ?
				static_cast<hl::u16>(csbSynth.pan3dInfo.volume * 10) :
				static_cast<hl::u16>(
					((static_cast<double>(csbSynth.dry[0]) / 255.0) * 10000.0)
			);

			if (acbPan3dVolume != 10000)
			{
				cmd.append_set_pan3d_volume(acbPan3dVolume);
			}
		}
		*/

		// TODO: SetPan3DAngle
	}

	static hl::u16 GenerateACBSequence(
		atom::cue_sheet& outAcb,
		const audio::cue_sheet& csb,
		const SoundElementStreamingInfo* streamingInfo,
		const SoundElementInfos& soundElementInfos,
		rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
		const audio::synth& csbSynth
	);

	static hl::u16 GenerateACBTrack(
		atom::cue_sheet& outAcb,
		const audio::cue_sheet& csb,
		const SoundElementStreamingInfo* streamingInfo,
		const SoundElementInfos& soundElementInfos,
		rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
		const audio::synth& csbSynth,
		bool csbSynthIsRecursive,
		hl::u32 category,
		hl::u16 loopIndex)
	{
		const auto acbTrackIndex = outAcb.tracks.size();
		auto& acbTrack = outAcb.tracks.emplace_back(
			outAcb.tracks.allocator()
		);

		// Generate ACB AISAC Indices
		GenerateACBAisacIndices(
			outAcb,
			csb,
			aisacControlMap,
			csbSynth,
			acbTrack.localAisacIndices
		);

		// Generate ACB track commands.
		acbTrack.commandIndex = static_cast<hl::u16>(outAcb.trackCommands.size());

		auto& cmd = outAcb.trackCommands.emplace_back(
			outAcb.trackCommands.allocator()
		);

		GenerateACBCommands(cmd, csbSynth);

		cmd.append_set_bus_send(0, 10000);
		cmd.append_set_bus_send(1, 0); // TODO: Figure out why tf some sounds have this set to 10000 (full reverb) and others have it set to 0 (no reverb)
		//cmd.append_set_bus_send(1, (category == ACF_CATEGORY_BGM) ? 0 : 10000);
		cmd.append_set_bus_send(2, 0);
		cmd.append_set_bus_send(3, 0);
		cmd.append_set_bus_send(4, 0);
		cmd.append_set_bus_send(5, 0);
		cmd.append_set_bus_send(6, 0);
		cmd.append_set_bus_send(7, 0);

		// Generate ACB track event.
		const auto evIndex = outAcb.trackEventCommands.size();
		acbTrack.eventIndex = static_cast<hl::u16>(evIndex);

		auto evCmd = &outAcb.trackEventCommands.emplace_back(
			outAcb.trackEventCommands.allocator()
		);

		if (csbSynth.delayTime != 0)
		{
			evCmd->append_wait(csbSynth.delayTime);
		}

		switch (csbSynth.linkType)
		{
		case audio::synth_link_type::sound_element:
		{
			for (const auto& linkName : csbSynth.linkNames)
			{
				const auto& csbSoundElement = GetCSBLinkSoundElement(csb, linkName);
				const auto csbLinkIndex = static_cast<std::size_t>(
					&csbSoundElement - csb.soundElements.begin()
				);

				//const auto sampleRate = csbSoundElement.sampleRate;
				const auto& soundElementInfo = soundElementInfos.data[csbLinkIndex];
				const auto firstAcbSynthIndex = outAcb.synths.size();

				for (unsigned char segmentIndex = 0;
					segmentIndex < soundElementInfo.segmentCount;
					++segmentIndex)
				{
					auto& acbSynth = outAcb.synths.emplace_back(
						atom::synth_type::polyphonic,
						outAcb.synths.allocator()
					);

					// TODO: Set voiceLimitGroupName !
					acbSynth.commandIndex = 0;

					acbSynth.refItems.emplace_back(
						atom::ref_type::waveform,
						static_cast<hl::u16>(
							soundElementInfo.firstSegmentGlobalIndex +
							segmentIndex
						)
					);
				}

				InsertTrackEventPlaySynthCommands(
					*evCmd,
					evCmd->end(),
					soundElementInfo,
					firstAcbSynthIndex,
					loopIndex
				);
			}

			break;
		}

		case audio::synth_link_type::synth:
		{
			const auto acbSeqRefIndex = GenerateACBSequence(
				outAcb,
				csb,
				streamingInfo,
				soundElementInfos,
				aisacControlMap,
				csbSynth
			);

			// NOTE: This is necessary since the vector may be reallocated above.
			evCmd = outAcb.trackEventCommands.data() + evIndex;

			evCmd->append_play(
				atom::ref_type::sequence,
				acbSeqRefIndex
			);

			break;
		}

		default:
			throw std::runtime_error("Unsupported CSB synth link type");
		}

		evCmd->append_no_op();
		return static_cast<hl::u16>(acbTrackIndex);
	}

	hl::u16 GenerateACBSequence(
		atom::cue_sheet& outAcb,
		const audio::cue_sheet& csb,
		const SoundElementStreamingInfo* streamingInfo,
		const SoundElementInfos& soundElementInfos,
		rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
		const audio::synth& csbSynth)
	{
		// Generate ACB sequence.
		atom::sequence_type acbSequenceType;
		bool needsTrackValues = false;

		switch (csbSynth.complexType)
		{
		case audio::synth_complex_type::polyphonic:
			acbSequenceType = atom::sequence_type::polyphonic;
			break;

		case audio::synth_complex_type::random_no_repeat:
			acbSequenceType = atom::sequence_type::random_no_repeat;
			needsTrackValues = true;
			break;

		case audio::synth_complex_type::sequential:
			acbSequenceType = atom::sequence_type::sequential;
			break;

		case audio::synth_complex_type::random:
			acbSequenceType = atom::sequence_type::random;
			needsTrackValues = true;
			break;

		case audio::synth_complex_type::sequential_no_loop:
			// TODO: Handle this complex type ?
			throw std::runtime_error("Unsupported CSB synth complex type");

		default:
			throw std::runtime_error("Unsupported CSB synth complex type");
		}

		const bool csbSynthIsRecursive = (csbSynth.linkType == audio::synth_link_type::synth);

		const auto acbSequenceIndex = outAcb.sequences.size();
		auto& acbSequence = outAcb.sequences.emplace_back(
			acbSequenceType,
			outAcb.sequences.allocator()
		);

		acbSequence.commandIndex = static_cast<hl::u16>(
			outAcb.sequenceCommands.size()
		);

		auto& cmd = outAcb.sequenceCommands.emplace_back(
			outAcb.sequenceCommands.allocator()
		);

		// TODO: Write SetCategoryPriority command if necessary ?

		// TODO: Is there a better way to determine category ??
		hl::u32 category;
		if (csbSynth.name.starts_with("Synth/3000000_voice/"))
		{
			category = ACF_CATEGORY_VOICE;
		}
		else if (streamingInfo)
		{
			category = ACF_CATEGORY_BGM;
		}
		else
		{
			category = ACF_CATEGORY_SE;
		}

		// TODO: Use ACF_CATEGORY_SYSTEM
		// TODO: Use ACF_CATEGORY_SE_EVENT

		cmd.append_set_category(category);

		GenerateACBCommands(cmd, csbSynth);

		for (hl::u16 i = 0; i < 8; ++i)
		{
			cmd.append_set_bus_send(i, 10000);
		}

		// Generate ACB AISAC Indices
		GenerateACBAisacIndices(
			outAcb,
			csb,
			aisacControlMap,
			csbSynth,
			acbSequence.localAisacIndices
		);

		if (needsTrackValues)
		{
			acbSequence.trackValues.reserve(csbSynth.linkNames.size());
		}

		// Generate ACB tracks.
		hl::u16 loopIndex = 0;
		acbSequence.trackIndices.reserve(csbSynth.linkNames.size());

		for (const auto& linkName : csbSynth.linkNames)
		{
			const auto& csbLinkSynth = ((csbSynthIsRecursive)
				? GetCSBLinkSynth(csb, linkName)
				: csbSynth
			);

			const auto acbTrackIndex = GenerateACBTrack(
				outAcb,
				csb,
				streamingInfo,
				soundElementInfos,
				aisacControlMap,
				csbLinkSynth,
				csbSynthIsRecursive,
				category,
				loopIndex++
			);

			auto& acbSequenceRef = outAcb.sequences[acbSequenceIndex];

			acbSequenceRef.trackIndices.push_back_unchecked(
				acbTrackIndex
			);

			if (needsTrackValues)
			{
				acbSequenceRef.trackValues.push_back_unchecked(
					static_cast<unsigned int>(csbLinkSynth.probability) /
					static_cast<unsigned int>(csbSynth.linkNames.size())
				);
			}
		}

		return static_cast<hl::u16>(acbSequenceIndex);
	}

	static void UpgradeCSB(
		atom::cue_sheet& outAcb,
		const void* csbData,
		unsigned long csbDataSize,
		const SoundElementStreamingInfo* streamingInfo = nullptr,
		rad::allocator& tmpAllocator = rad::default_allocator,
		rad::allocator& csbAllocator = rad::default_allocator)
	{
		const auto csb = ReadCSB(csbData, csbDataSize, tmpAllocator, csbAllocator);

		const auto soundElementInfos = GetSoundElementInfosForUpgrade(
			csb,
			streamingInfo,
			csbAllocator
		);

		// Generate embedded AWB data.
		if (soundElementInfos.totalEmbeddedDataCount)
		{
			rad::memory_stream awbStream; // TODO: Pass allocator
			GenerateEmbeddedAWB(soundElementInfos, awbStream);
			outAcb.embeddedAwbData = awbStream.release();
		}

		// Generate streaming AWB data.
		if (soundElementInfos.totalStreamingDataCount)
		{
			rad::memory_stream awbStream; // TODO: Pass allocator
			GenerateStreamingAWB(soundElementInfos, awbStream);
			outAcb.streamAwbTocData.emplace_back(awbStream.release());
			outAcb.streamAwbHashes.emplace_back(atom::wave_bank_hash{outAcb.name, {0x20, 0x82, 0x21, 0xa2, 0x22, 0xb5, 0x00, 0xb9, 0x8f, 0x0d, 0x43, 0x1c, 0xf9, 0x5a, 0xbd, 0x2f}}); // TODO: Actual hash!!!
		}

		// Generate string values.
		static const std::string_view defaultBusNames[] =
		{
			"MasterOut",
			"BUS1",
			"BUS2",
			"BUS3",
			"BUS4",
			"BUS5",
			"BUS6",
			"BUS7",
		};

		outAcb.stringValues.reserve(std::size(defaultBusNames));

		for (const auto& defaultBusName : defaultBusNames)
		{
			assert(outAcb.stringValues.size() < outAcb.stringValues.capacity() &&
				"String value count exceeded capacity; this should never happen!"
			);

			outAcb.stringValues.emplace_back_unchecked(
				outAcb.stringValues.allocator(),
				defaultBusName
			);
		}

		// Generate default ACF references.
		outAcb.acfRefItems.emplace_back(
			atom::config_ref_item_type::category,
			rad::string{ outAcb.acfRefItems.allocator(), "BGM" },
			rad::string{ outAcb.acfRefItems.allocator() },
			ACF_CATEGORY_BGM
		);

		outAcb.acfRefItems.emplace_back(
			atom::config_ref_item_type::category,
			rad::string{ outAcb.acfRefItems.allocator(), "SE" },
			rad::string{ outAcb.acfRefItems.allocator() },
			ACF_CATEGORY_SE
		);

		outAcb.acfRefItems.emplace_back(
			atom::config_ref_item_type::category,
			rad::string{ outAcb.acfRefItems.allocator(), "VOICE" },
			rad::string{ outAcb.acfRefItems.allocator() },
			ACF_CATEGORY_VOICE
		);

		for (const auto& defaultBusName : defaultBusNames)
		{
			outAcb.acfRefItems.emplace_back(
				atom::config_ref_item_type::dsp_bus,
				rad::string{ outAcb.acfRefItems.allocator(), defaultBusName },
				rad::string{ outAcb.acfRefItems.allocator() }
			);
		}

		for (const auto& globalAisacControl : GlobalAisacControls)
		{
			outAcb.acfRefItems.emplace_back(
				atom::config_ref_item_type::aisac_control,
				rad::string{ outAcb.acfRefItems.allocator(), globalAisacControl }
			);
		}

		// Upgrade sound elements.
		hl::u16 curMemoryAwbId = 0, curStreamAwbId = 0;

		outAcb.waveforms.reserve(soundElementInfos.totalSegmentCount);

		for (const auto& soundElementInfo : soundElementInfos.data)
		{
			for (unsigned char segmentIndex = 0;
				segmentIndex < soundElementInfo.segmentCount;
				++segmentIndex)
			{
				const auto& segment = soundElementInfo.segments[segmentIndex];

				// Generate ACB waveform.
				assert(outAcb.waveforms.size() < outAcb.waveforms.capacity() &&
					"Waveform count exceeded capacity; this should never happen!"
				);

				outAcb.waveforms.emplace_back_unchecked(
					((soundElementInfo.HasStreamingData()) ?	// memoryAwbId
						UINT16_MAX : curMemoryAwbId++),
					
					((soundElementInfo.HasStreamingData()) ?	// streamAwbId
						curStreamAwbId++ : UINT16_MAX),

					atom::waveform_encode_type::adx,			// encodeType

					((soundElementInfo.HasStreamingData()) ?	// streamType
						atom::waveform_stream_type::stream :
						atom::waveform_stream_type::memory
					),

					atom::waveform_loop_type::one_shot,			// loopType
					soundElementInfo.channelCount,				// channelCount
					soundElementInfo.sampleRate,				// sampleRate
					segment.sampleCount,						// sampleCount

					((soundElementInfo.HasStreamingData()) ?	// streamAwbPort
						0 : UINT16_MAX)
				);
			}
		}

		// Generate default ACB AIASC control names.
		outAcb.aisacControls.reserve(GlobalAisacControlCount);

		for (hl::u16 i = 0; i < GlobalAisacControlCount; ++i)
		{
			assert(outAcb.aisacControls.size() < outAcb.aisacControls.capacity() &&
				"AISAC control count exceeded capacity; this should never happen!"
			);

			outAcb.aisacControls.emplace_back_unchecked(
				rad::string{ outAcb.aisacControls.allocator(), GlobalAisacControls[i] },
				static_cast<hl::u16>(1000 + i)
			);
		}

		// Upgrade AISACs.
		outAcb.aisacs.reserve(csb.aisacs.size());

		for (const auto& csbAisac : csb.aisacs)
		{
			const auto controlIndex = GetGlobalAisacControlIndex(csbAisac.controlName);
			if (controlIndex == UINT16_MAX)
			{
				// TODO: Maybe handle this somehow instead of throwing?
				throw std::runtime_error("CSB had unsupported AISAC control");
			}

			const auto controlID = static_cast<hl::u16>(1000 + controlIndex);

			assert(outAcb.aisacs.size() < outAcb.aisacs.capacity() &&
				"AISAC count exceeded capacity; this should never happen!"
			);

			auto& acbAisac = outAcb.aisacs.emplace_back_unchecked(
				atom::aisac_type::simple,
				controlID,
				outAcb.aisacs.allocator()
			);

			// Generate ACB graph.
			acbAisac.graphIndices.reserve(csbAisac.graphs.size());

			for (const auto& csbGraph : csbAisac.graphs)
			{
				atom::graph_type acbGraphType;
				switch (csbGraph.type)
				{
				case audio::graph_type::volume:
					acbGraphType = atom::graph_type::volume;
					break;

				case audio::graph_type::pitch:
					acbGraphType = atom::graph_type::pitch;
					break;

				case audio::graph_type::bandpass_cutoff_low:
					acbGraphType = atom::graph_type::bandpass_cutoff_low;
					break;

				case audio::graph_type::bandpass_cutoff_high:
					acbGraphType = atom::graph_type::bandpass_cutoff_high;
					break;

				case audio::graph_type::bus_send_0:
					acbGraphType = atom::graph_type::bus_send_0;
					break;

				case audio::graph_type::bus_send_1:
					acbGraphType = atom::graph_type::bus_send_1;
					break;

				case audio::graph_type::voice_priority:
					acbGraphType = atom::graph_type::voice_priority;
					break;

				case audio::graph_type::unknown23:
				case audio::graph_type::unknown26:
					// TODO: What are these ?? Support these somehow?
					continue;

				default:
					throw std::runtime_error("CSB had unsupported graph type");
				}

				assert(acbAisac.graphIndices.size() < acbAisac.graphIndices.capacity() &&
					"Graph index count exceeded capacity; this should never happen!"
				);

				acbAisac.graphIndices.push_back_unchecked(
					static_cast<hl::u16>(outAcb.graphs.size())
				);

				auto& acbGraph = outAcb.graphs.emplace_back(
					acbGraphType,
					outAcb.graphs.allocator()
				);

				// Convert points.
				acbGraph.curves.resize(csbGraph.points.size(), static_cast<hl::u16>(100));
				acbGraph.controls.reserve(csbGraph.points.size());
				acbGraph.destinations.reserve(csbGraph.points.size());

				for (const auto& csbPoint : csbGraph.points)
				{
					acbGraph.controls.push_back_unchecked(
						static_cast<float>(
							(static_cast<double>(csbPoint.in) / 10000.0) *
							(csbGraph.inputMax - csbGraph.inputMin) +
							csbGraph.inputMin
						)
					);

					acbGraph.destinations.push_back_unchecked(
						static_cast<hl::u16>(
							((static_cast<double>(csbPoint.out) / 10000.0) *
							(csbGraph.outputMax - csbGraph.outputMin) +
							csbGraph.outputMin) * 10000.0
						)
					);
				}
			}
		}

		// Generate default synth command table.
		auto& defaultSynthCmd = outAcb.synthCommands.emplace_back();

		for (hl::u16 i = 0; i < 8; ++i)
		{
			defaultSynthCmd.append_set_bus_send(i, 10000);
		}

		// Upgrade cues.
		outAcb.cues.reserve(csb.cues.size());

		for (const auto& csbCue : csb.cues)
		{
			// Generate ACB cue.
			assert(outAcb.cues.size() < outAcb.cues.capacity() &&
				"Cue count exceeded capacity; this should never happen!"
			);

			auto& acbCue = outAcb.cues.emplace_back_unchecked(
				csbCue.id,
				atom::ref_item{ 
					atom::ref_type::sequence,
					static_cast<hl::u16>(outAcb.sequences.size())
				},
				rad::optional_string{
					outAcb.cues.allocator(),
					csbCue.name
				},
				outAcb.cues.allocator()
			);

			acbCue.userData.assign(csbCue.userData.has_value() ?
				csbCue.userData.value() : rad::string{}
			);

			acbCue.aisacControlMap.assign(2, 0);

			if (csbCue.flags & audio::CUE_FLAGS_DOES_LOOP)
			{
				acbCue.playDuration = UINT32_MAX;
			}
			else
			{
				// TODO: Calculate play duration
			}

			// Upgrade associated synth.
			const auto csbSynthIt = csb.find_synth(csbCue.synthName);
			if (csbSynthIt == csb.synths.end())
			{
				throw std::runtime_error(
					"CSB cue links to a synth which was not found"
				);
			}

			GenerateACBSequence(
				outAcb,
				csb,
				streamingInfo,
				soundElementInfos,
				acbCue.aisacControlMap,
				*csbSynthIt
			);
		}
	}

	bool TryUpgradeCSB(
		hl::cri::atom::cue_sheet& outAcb,
		const void* csbData,
		unsigned long csbDataSize,
		const SoundElementStreamingInfo* streamingInfo,
		rad::allocator& tmpAllocator,
		rad::allocator& csbAllocator)
	{
		try
		{
			UpgradeCSB(
				outAcb,
				csbData,
				csbDataSize,
				streamingInfo,
				tmpAllocator,
				csbAllocator
			);
		}
		catch (const std::exception& ex)
		{
			g_loader->WriteLog(
				ML_LOG_LEVEL_ERROR,
				ML_LOG_CATEGORY_GENERAL,
				"Failed to upgrade 2011 CueSheet \"%s\": \"%s\"\n",
				reinterpret_cast<size_t>(outAcb.name.data()),
				reinterpret_cast<size_t>(ex.what()),
				nullptr
			);

			return false;
		}
		catch (...)
		{
			g_loader->WriteLog(
				ML_LOG_LEVEL_ERROR,
				ML_LOG_CATEGORY_GENERAL,
				"Failed to upgrade 2011 CueSheet \"%s\": \"%s\"\n",
				reinterpret_cast<size_t>(outAcb.name.data()),
				reinterpret_cast<size_t>("[NO ERROR MESSAGE]"),
				nullptr
			);

			return false;
		}

		return true;
	}
}