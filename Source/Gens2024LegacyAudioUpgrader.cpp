#include "Gens2024LegacyAudioUpgrader.h"
#include "Gens2024LegacyAudioGenerator.h"
#include "Gens2024LegacyAudioParser.h"
#include <hedgelib/cri/hl_cri_cue_sheet.h>
#include <hedgelib/cri/hl_cri_atom_cue_sheet.h>
#include <hedgelib/cri/hl_cri_atom_wave_bank.h>
#include <hedgelib/cri/hl_cri_packed_file.h>
#include <rad/rad_stack_or_heap_array.h>
#include <rad/rad_file.h> // TODO: REMOVE THIS

using namespace hl::cri_new;

namespace gens2024
{
	struct CSBToACBMapEntry
	{
		hl::u16 aisacIndex;
		hl::u16 aisacControlIndex;

		CSBToACBMapEntry(hl::u16 aisacIndex, hl::u16 aisacControlIndex) noexcept
			: aisacIndex(aisacIndex)
			, aisacControlIndex(aisacControlIndex)
		{
		}
	};

	using CSBToACBMap = ankerl::unordered_dense::map<std::string_view, CSBToACBMapEntry>;

	static const char* GlobalAisacControls[] =
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

	static hl::u16 GetAisacControlIndex(const char* aisacControlName)
	{
		for (hl::u16 i = 0; i < static_cast<hl::u16>(std::size(GlobalAisacControls)); ++i)
		{
			if (std::strcmp(GlobalAisacControls[i], aisacControlName) == 0)
			{
				return i;
			}
		}

		return UINT16_MAX;
	}

	enum ACFCategory
	{
		ACF_CATEGORY_BGM = 0,
		ACF_CATEGORY_SE = 1,
		ACF_CATEGORY_VOICE = 2,
	};
	
	class CSBUpgrader
	{
		atom::cue_sheet									acb;
		const audio::cue_sheet*							csb;
		const WaveformInfo*								waveformsInfo;
		rad::stack_or_heap_array<unsigned short, 64>	soundElementToWaveformMap;
		bool											hasUpgraded = false;

		static void assignAisacIndices(
			const audio::synth& csbSynth,
			const CSBToACBMap& aisacMap,
			rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
			rad::vector<hl::u16>& localAisacIndices
		);

		unsigned short generateSynth(unsigned short acbWavwformIndex);

		unsigned short generateTrackCmdTable(
			const audio::synth& csbSynth,
			ACFCategory category
		);

		unsigned short generateTrackEvent(
			const audio::synth& csbSynth,
			const CSBToACBMap& aisacMap,
			rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
			ACFCategory category
		);

		unsigned short generateTrack(
			const audio::synth& csbSynth,
			const CSBToACBMap& aisacMap,
			rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
			ACFCategory category
		);

		unsigned short generateBgmSequenceCmdTable();

		unsigned short generateVoiceSequenceCmdTable();

		unsigned short generateSequence(
			const audio::synth& csbSynth,
			const CSBToACBMap& aisacMap,
			rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
			ACFCategory category
		);

	public:
		inline const atom::cue_sheet& result() const noexcept
		{
			return acb;
		}

		CSBUpgrader(
			const audio::cue_sheet& csb,
			rad::span<const WaveformInfo> waveformsInfo,
			std::string_view name
		);
	};

	void CSBUpgrader::assignAisacIndices(
		const audio::synth& csbSynth,
		const CSBToACBMap& aisacMap,
		rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
		rad::vector<hl::u16>& localAisacIndices)
	{
		localAisacIndices.reserve(csbSynth.aisacNames.size());

		for (const auto& csbAisacName : csbSynth.aisacNames)
		{
			const auto it = aisacMap.find(csbAisacName);
			if (it == aisacMap.end())
			{
				// TODO: This should probably just be a warning.
				throw std::runtime_error("Failed to map CSB AISACs to ACB");
			}

			localAisacIndices.push_back_unchecked(it->second.aisacIndex);

			assert((it->second.aisacControlIndex / 8) < aisacControlMap.size());
			aisacControlMap[it->second.aisacControlIndex / 8] |= (1 << (it->second.aisacControlIndex % 8));
		}
	}

	unsigned short CSBUpgrader::generateSynth(
		unsigned short acbWaveformIndex)
	{
		const auto acbSynthIndex = static_cast<unsigned short>(acb.synths.size());
		auto& acbSynth = acb.synths.emplace_back();

		acbSynth.refItems.emplace_back(
			atom::reference_type::waveform,
			acbWaveformIndex
		);

		return acbSynthIndex;
	}

	unsigned short CSBUpgrader::generateTrackCmdTable(
		const audio::synth& csbSynth,
		ACFCategory category)
	{
		const auto acbTrackCmdTableIndex = static_cast<unsigned short>(acb.trackCmdTables.size());
		auto& acbTrackCmdTable = acb.trackCmdTables.emplace_back();

		const auto acbSynthVolume = static_cast<unsigned short>(
			std::round(static_cast<float>(csbSynth.volume) / 10.0f)
		);

		if (acbSynthVolume != 100)
		{
			acbTrackCmdTable.append_set_volume(acbSynthVolume);
		}

		acbTrackCmdTable.append_set_bus_send(0, 10000);
		//acbTrackCmdTable.append_set_bus_send(1, 10000);
		acbTrackCmdTable.append_set_bus_send(1, (category == ACF_CATEGORY_BGM) ? 0 : 10000);
		acbTrackCmdTable.append_set_bus_send(2, 0);
		acbTrackCmdTable.append_set_bus_send(3, 0);
		acbTrackCmdTable.append_set_bus_send(4, 0);
		acbTrackCmdTable.append_set_bus_send(5, 0);
		acbTrackCmdTable.append_set_bus_send(6, 0);
		acbTrackCmdTable.append_set_bus_send(7, 0);

		return acbTrackCmdTableIndex;
	}

	unsigned short CSBUpgrader::generateTrackEvent(
		const audio::synth& csbSynth,
		const CSBToACBMap& aisacMap,
		rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
		ACFCategory category)
	{
		const auto acbTrackEventIndex = static_cast<unsigned short>(acb.trackEventCmdTables.size());
		auto acbTrackEvent = &acb.trackEventCmdTables.emplace_back();

		if (csbSynth.linkType == audio::SYNTH_LINK_TYPE_SOUND_ELEMENT)
		{
			// Recurse through synth links.
			if (csbSynth.nodeLinkNames.size() != 1)
			{
				throw std::runtime_error("Synth had unexpected link count");
			}

			//const auto& csbSoundElementNode = csb.at(csbSynth.nodeLinkIndices[0]);
			const auto csbSoundElementIt = csb->soundElements.find(csbSynth.nodeLinkNames[0]);

			if (csbSoundElementIt == csb->soundElements.end())
			{
				throw std::runtime_error("Could not find referenced sound element");
			}

			// Get waveform index.
			const auto& csbSoundElement = csbSoundElementIt->second;
			const auto csbSoundElementIndex = static_cast<std::size_t>(
				csbSoundElementIt - csb->soundElements.begin()
			);

			const auto& waveformInfo = waveformsInfo[csbSoundElementIndex];
			auto acbWaveformIndex = soundElementToWaveformMap[csbSoundElementIndex];

			// Generate synths and appropriate play commands.
			if (waveformInfo.segmentCount > 0)
			{
				float loopStartPosSec = 0;
				unsigned char i = 0;

				while (true)
				{
					const auto acbSynthIndex = generateSynth(acbWaveformIndex++);

					acbTrackEvent->append_play(
						atom::reference_type::synth,
						acbSynthIndex
					);

					if (++i == waveformInfo.segmentCount)
					{
						break;
					}

					const auto segStartPosSec = waveformInfo.ComputeStartSeconds(
						i,
						csbSoundElement.sampleRate
					);

					const auto segStartPosMilli = static_cast<unsigned long>(
						segStartPosSec * 1000.0
					);
						
					acbTrackEvent->append_wait(segStartPosMilli);

					if (waveformInfo.doesLoop && waveformInfo.loopSegmentIndex == i)
					{
						acbTrackEvent->append_seq_loop_start(0);
						loopStartPosSec = segStartPosSec;
					}
				}

				if (waveformInfo.doesLoop && waveformInfo.segmentCount > 1)
				{
					const auto loopEndPosSec = waveformInfo.ComputeEndSeconds(
						csbSoundElement.sampleRate
					);

					const auto loopDurationMilli = static_cast<unsigned long>(
						(loopEndPosSec - loopStartPosSec) * 1000.0
					);

					acbTrackEvent->append_wait(loopDurationMilli);
					acbTrackEvent->append_seq_loop_end(0, loopDurationMilli);
				}
			}
		}
		else if (csbSynth.linkType == audio::SYNTH_LINK_TYPE_SYNTH)
		{
			const auto acbSequenceIndex = generateSequence(
				csbSynth,
				aisacMap,
				aisacControlMap,
				category
			);

			// Update pointers, since the vectors might have changed.
			acbTrackEvent = &acb.trackEventCmdTables[acbTrackEventIndex];

			acbTrackEvent->append_play(
				atom::reference_type::sequence,
				acbSequenceIndex
			);
		}
		else
		{
			throw std::runtime_error("Cannot convert csb synth to acb track");
		}

		acbTrackEvent->append_no_op();
		return acbTrackEventIndex;
	}

	unsigned short CSBUpgrader::generateTrack(
		const audio::synth& csbSynth,
		const CSBToACBMap& aisacMap,
		rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
		ACFCategory category)
	{
		// Generate track.
		const auto acbTrackIndex = static_cast<unsigned short>(acb.tracks.size());
		auto acbTrack = &acb.tracks.emplace_back();

		// Assign AISAC indices.
		assignAisacIndices(csbSynth, aisacMap, aisacControlMap, acbTrack->localAisacIndices);

		// Generate track command table.
		acbTrack->commandIndex = generateTrackCmdTable(csbSynth, category);

		// Generate track event command table.
		const auto trackEventIndex = generateTrackEvent(
			csbSynth,
			aisacMap,
			aisacControlMap,
			category
		);

		// Update pointers, since the vectors might have changed.
		acbTrack = &acb.tracks[acbTrackIndex];

		acbTrack->eventIndex = trackEventIndex;
		return acbTrackIndex;
	}

	unsigned short CSBUpgrader::generateBgmSequenceCmdTable()
	{
		const auto acbSeqCmdTableIndex = static_cast<unsigned short>(acb.sequenceCmdTables.size());
		auto& acbSeqCmdTable = acb.sequenceCmdTables.emplace_back();

		// TODO: SetCategoryPriorityLevel(255)
		acbSeqCmdTable.append_set_category(ACF_CATEGORY_BGM);
		acbSeqCmdTable.append_set_bus_send(0, 10000);
		acbSeqCmdTable.append_set_bus_send(1, 10000);
		acbSeqCmdTable.append_set_bus_send(2, 10000);
		acbSeqCmdTable.append_set_bus_send(3, 10000);
		acbSeqCmdTable.append_set_bus_send(4, 10000);
		acbSeqCmdTable.append_set_bus_send(5, 10000);
		acbSeqCmdTable.append_set_bus_send(6, 10000);
		acbSeqCmdTable.append_set_bus_send(7, 10000);

		return acbSeqCmdTableIndex;
	}

	unsigned short CSBUpgrader::generateVoiceSequenceCmdTable()
	{
		const auto acbSeqCmdTableIndex = static_cast<unsigned short>(acb.sequenceCmdTables.size());
		auto& acbSeqCmdTable = acb.sequenceCmdTables.emplace_back();

		acbSeqCmdTable.append_set_category(ACF_CATEGORY_VOICE);
		acbSeqCmdTable.append_set_bus_send(0, 10000);
		acbSeqCmdTable.append_set_bus_send(1, 10000);
		acbSeqCmdTable.append_set_bus_send(2, 10000);
		acbSeqCmdTable.append_set_bus_send(3, 10000);
		acbSeqCmdTable.append_set_bus_send(4, 10000);
		acbSeqCmdTable.append_set_bus_send(5, 10000);
		acbSeqCmdTable.append_set_bus_send(6, 10000);
		acbSeqCmdTable.append_set_bus_send(7, 10000);

		return acbSeqCmdTableIndex;
	}

	unsigned short CSBUpgrader::generateSequence(
		const audio::synth& csbSynth,
		const CSBToACBMap& aisacMap,
		rad::stack_or_heap_array<hl::u8, 8>& aisacControlMap,
		ACFCategory category)
	{
		const auto acbSequenceIndex = static_cast<unsigned short>(acb.sequences.size());
		auto acbSequence = &acb.sequences.emplace_back();

		// Assign AISAC indices.
		assignAisacIndices(csbSynth, aisacMap, aisacControlMap, acbSequence->localAisacIndices);

		// Assign type.
		acbSequence->trackIndices.reserve(csbSynth.nodeLinkNames.size());

		switch (csbSynth.complexType)
		{
		case audio::SYNTH_COMPLEX_TYPE_POLYPHONIC:
			acbSequence->type = atom::sequence_type::polyphonic;
			break;

		case audio::SYNTH_COMPLEX_TYPE_RANDOM_NO_REPEAT:
			acbSequence->type = atom::sequence_type::random_no_repeat;
			acbSequence->trackValues.reserve(csbSynth.nodeLinkNames.size());
			break;

		case audio::SYNTH_COMPLEX_TYPE_SEQUENTIAL:
			acbSequence->type = atom::sequence_type::sequential;
			break;

		case audio::SYNTH_COMPLEX_TYPE_RANDOM:
			// TODO: It seems gens 2024 also uses RANDOM_NO_REPEAT for all of these??
			acbSequence->type = atom::sequence_type::random;
			acbSequence->trackValues.reserve(csbSynth.nodeLinkNames.size());
			break;

		case audio::SYNTH_COMPLEX_TYPE_SEQUENTIAL_NO_LOOP:
		default:
			throw std::runtime_error("Cannot convert synth type to acb");
		}

		// Set sequence command table.
		if (category == ACF_CATEGORY_BGM)
		{
			acbSequence->commandIndex = 0;
		}
		else
		{
			// TODO: Handle SE also!
			acbSequence->commandIndex = 1;
		}

		// Generate tracks.
		if (csbSynth.linkType == audio::SYNTH_LINK_TYPE_SYNTH)
		{
			for (const auto& nodeLinkName : csbSynth.nodeLinkNames)
			{
				//const auto& csbLinkSynthNode = csb.at(nodeLinkIndex);
				//const auto& csbLinkSynth = csb.synths().at(csbLinkSynthNode.data_index());
				const auto& csbLinkSynth = csb->synths.at(nodeLinkName);

				// Generate track.
				const auto acbTrackIndex = generateTrack(
					csbLinkSynth,
					aisacMap,
					aisacControlMap,
					category
				);

				// Update pointers, since the vectors might have changed.
				acbSequence = &acb.sequences[acbSequenceIndex];

				// Generate track values.
				if (csbLinkSynth.linkType == audio::SYNTH_LINK_TYPE_SOUND_ELEMENT &&
					(acbSequence->type == atom::sequence_type::random ||
					acbSequence->type == atom::sequence_type::random_no_repeat))
				{
					// TODO: Use probability from CSB !!!
					acbSequence->trackValues.push_back(100);
					//acbSequence->trackValues.push_back(csbSynth.probability);
				}

				acbSequence->trackIndices.push_back(acbTrackIndex);
			}
		}
		else if (csbSynth.linkType == audio::SYNTH_LINK_TYPE_SOUND_ELEMENT)
		{
			// Generate track.
			const auto acbTrackIndex = generateTrack(
				csbSynth,
				aisacMap,
				aisacControlMap,
				category
			);

			// Update pointers, since the vectors might have changed.
			acbSequence = &acb.sequences[acbSequenceIndex];

			if (acbSequence->type == atom::sequence_type::random ||
				acbSequence->type == atom::sequence_type::random_no_repeat)
			{
				// This should never happen, but if it does, set sequence type to
				// polyphonic since there isn't more than one sound anyway.

				acbSequence->type = atom::sequence_type::polyphonic;
			}

			acbSequence->trackIndices.push_back(acbTrackIndex);
		}
		else
		{
			throw std::runtime_error("Cannot convert csb synth to acb sequence");
		}

		// Divide track values by track count.
		for (auto& trackValue : acbSequence->trackValues)
		{
			if (trackValue != 0)
			{
				trackValue /= static_cast<unsigned short>(
					acbSequence->trackValues.size()
				);
			}
		}

		return acbSequenceIndex;
	}

	static atom::graph_type ConvertGraphType(audio::graph_type type)
	{
		switch (type)
		{
		case audio::graph_type::volume:
			return atom::graph_type::volume;

		case audio::graph_type::bandpass_cutoff_low:
			return atom::graph_type::bandpass_cutoff_low;

		case audio::graph_type::bandpass_cutoff_high:
			return atom::graph_type::bandpass_cutoff_high;

		case audio::graph_type::bus_send_0:
			return atom::graph_type::bus_send_0;

		case audio::graph_type::bus_send_1:
			return atom::graph_type::bus_send_1;

		default:
			throw std::runtime_error("Unknown or unsupported CSB AISAC Graph Type");
		}
	}

	CSBUpgrader::CSBUpgrader(
		const audio::cue_sheet& csb,
		rad::span<const WaveformInfo> waveformsInfo,
		std::string_view name)
		: csb(&csb)
		, waveformsInfo(waveformsInfo.data())
		, soundElementToWaveformMap(rad::no_value_init, waveformsInfo.size())
	{
		assert(csb.soundElements.size() == waveformsInfo.size());

		acb.name = rad::string(name);

		// Generate default sequence command tables.
		generateBgmSequenceCmdTable();
		//generateVoiceSequenceCmdTable();

		// Generate string values.
		acb.stringValues.reserve(8);
		acb.stringValues.emplace_back("MasterOut");
		acb.stringValues.emplace_back("BUS1");
		acb.stringValues.emplace_back("BUS2");
		acb.stringValues.emplace_back("BUS3");
		acb.stringValues.emplace_back("BUS4");
		acb.stringValues.emplace_back("BUS5");
		acb.stringValues.emplace_back("BUS6");
		acb.stringValues.emplace_back("BUS7");

		// Generate ACF reference items.
		acb.acfRefItems.reserve(25);

		acb.acfRefItems.emplace_back(
			atom::config_reference_item_type::category,
			"BGM",
			"",
			ACF_CATEGORY_BGM
		);

		//// TODO: Determine if we need this or not somehow - maybe search for synths starting with "Synth/3000000_voice" ??
		//acb.acfRefItems.emplace_back(
			//atom::acf_reference_item_type::category,
			//"VOICE",
			//"",
			//ACF_CATEGORY_VOICE
		//);

		acb.acfRefItems.emplace_back(
			atom::config_reference_item_type::dsp_bus,
			"MasterOut"
		);

		acb.acfRefItems.emplace_back(
			atom::config_reference_item_type::dsp_bus,
			"BUS1"
		);

		acb.acfRefItems.emplace_back(
			atom::config_reference_item_type::dsp_bus,
			"BUS2"
		);

		acb.acfRefItems.emplace_back(
			atom::config_reference_item_type::dsp_bus,
			"BUS3"
		);

		acb.acfRefItems.emplace_back(
			atom::config_reference_item_type::dsp_bus,
			"BUS4"
		);

		acb.acfRefItems.emplace_back(
			atom::config_reference_item_type::dsp_bus,
			"BUS5"
		);

		acb.acfRefItems.emplace_back(
			atom::config_reference_item_type::dsp_bus,
			"BUS6"
		);

		acb.acfRefItems.emplace_back(
			atom::config_reference_item_type::dsp_bus,
			"BUS7"
		);

		for (std::size_t i = 0; i < std::size(GlobalAisacControls); ++i)
		{
			acb.acfRefItems.emplace_back(
				atom::config_reference_item_type::aisac_control,
				GlobalAisacControls[i]
			);
		}

		//acb.acfRefItems.emplace_back(
			//atom::config_reference_item_type::aisac,
			//"uw_switch1"
		//);

		//acb.acfRefItems.emplace_back(
			//atom::config_reference_item_type::aisac_control,
			//"uw_switch"
		//);

		//acb.globalAisacRefTable.reserve(1);
		//acb.globalAisacRefTable.emplace_back("uw_switch1");

		// Convert AISACs.
		CSBToACBMap aisacMap;

		aisacMap.reserve(csb.aisacs.size());
		acb.aisacs.reserve(csb.aisacs.size());

		for (const auto& csbAisac : csb.aisacs)
		{
			// Convert AISAC Control.
			const auto controlIndex = GetAisacControlIndex(csbAisac.second.controlName.c_str());
			if (controlIndex == UINT16_MAX) continue;

			const auto controlID = controlIndex + 1000;
			acb.aisacControls.emplace_back(csbAisac.second.controlName, controlID);

			// Convert AISAC.
			const auto p = aisacMap.try_emplace(csbAisac.first,
				static_cast<hl::u16>(acb.aisacs.size()),
				controlIndex
			);

			assert(p.second);
			auto& acbAisac = acb.aisacs.emplace_back();

			acbAisac.controlID = controlID;
			acbAisac.graphIndices.reserve(csbAisac.second.graphs.size());

			// Convert AISAC graphs.
			for (const auto& csbGraph : csbAisac.second.graphs)
			{
				acbAisac.graphIndices.push_back_unchecked(
					static_cast<hl::u16>(acb.graphs.size())
				);

				auto& acbGraph = acb.graphs.emplace_back();
				acbGraph.type = ConvertGraphType(csbGraph.type);

				acbGraph.points.reserve(csbGraph.points.size());
				
				for (const auto& csbPoint : csbGraph.points)
				{
					acbGraph.points.emplace_back(
						static_cast<float>(
							(static_cast<double>(csbPoint.in) / 10000.0) *
							(csbGraph.inputMax - csbGraph.inputMin) +
							csbGraph.inputMin
						),
						static_cast<hl::u16>(
							((static_cast<double>(csbPoint.out) / 10000.0) *
							(csbGraph.outputMax - csbGraph.outputMin) +
							csbGraph.outputMin) * 10000.0
						)
					);
				}
			}
		}

		// Convert waveforms.
		//const auto csbCues = csb.cues();
		//const auto csbSynths = csb.synths();
		//const auto csbSoundElements = csb.sound_elements();

		//rad::vector<audio::aax_entry> aaxEntries;
		unsigned short curMemAwbId = 0, curStreamAwbId = 0;
		//acb.waveforms.reserve(csbSoundElements.size());

		//aaxEntries.reserve(2);

		//for (std::size_t i = 0; i < csb.soundElements.size(); ++i)
		auto waveformInfo = this->waveformsInfo;
		auto curSoundElementToWaveformIndex = soundElementToWaveformMap.data();

		for (const auto& csbSoundElement : csb.soundElements)
		{
			//const auto& csbSoundElement = csbSoundElements[i];
			*curSoundElementToWaveformIndex = static_cast<unsigned short>(acb.waveforms.size());

			switch (csbSoundElement.second.format)
			{
			case audio::SOUND_ELEMENT_FORMAT_AAX:
				//const auto curAaxEntryCount = audio::get_aax_entries(
					//csbSoundElement.second.embeddedData,
					//&aaxEntries
				//);

				for (unsigned char i = 0; i < waveformInfo->segmentCount; ++i)
				{
					const auto& segmentInfo = waveformInfo->segments[i];
					auto& acbWaveform = acb.waveforms.emplace_back();

					const bool isStreamingData = csbSoundElement.second.embeddedData.empty();

					acbWaveform.awbId = (isStreamingData) ? curStreamAwbId++ : curMemAwbId++;
					acbWaveform.encodeType = atom::waveform_encode_type::adx;
					acbWaveform.isStreaming = isStreamingData;
					acbWaveform.channelCount = csbSoundElement.second.channelCount;

					acbWaveform.loopFlags = atom::WAVEFORM_LOOP_FLAG_UNKNOWN1;
					//acbWaveform.loopFlags = ((waveformInfo->doesLoop) ?
						//atom::WAVEFORM_LOOP_FLAG_UNKNOWN2 :
						//atom::WAVEFORM_LOOP_FLAG_UNKNOWN1
					//);

					acbWaveform.sampleRate = csbSoundElement.second.sampleRate;
					acbWaveform.sampleCount = segmentInfo.sampleCount;
				}

				//aaxEntries.clear();

				break;

			default:
				throw std::runtime_error("Unsupported sound element data format");
			}

			++waveformInfo;
			++curSoundElementToWaveformIndex;
		}

		// Convert cues.
		acb.cues.reserve(csb.cues.size());

		//for (std::size_t i = 0; i < csbCues.size(); ++i)
		for (const auto& csbCue : csb.cues)
		{
			// Convert cue.
			//const auto& csbCue = csbCues[i];
			auto& acbCue = acb.cues.emplace_back();

			acbCue.id = csbCue.id;
			acbCue.name = csbCue.name.value_or(rad::string{});

			if (csbCue.userData.has_value())
			{
				acbCue.userData = csbCue.userData.data();
			}

			acbCue.aisacControlMap.assign(2, 0);

			const bool doesLoop = (csbCue.flags & audio::CUE_FLAGS_DOES_LOOP);

			acbCue.playDuration = ((doesLoop) ?
				UINT32_MAX :
				csbCue.compute_play_duration(csb)
			);

			// Determine ACF Category type.
			// TODO: Is there a better way to do this??
			ACFCategory category;
			if (csbCue.synthName.starts_with("Synth/3000000_voice/"))
			{
				category = ACF_CATEGORY_VOICE;
			}
			else
			{
				// TODO: Handle SE categories!
				category = ACF_CATEGORY_BGM;
			}

			// Generate sequence.
			//const auto& csbSynthNode = csbCue.get_synth_node(csb);
			const auto& csbSynth = csbCue.get_synth(csb);
			const auto acbSequenceIndex = generateSequence(
				csbSynth,
				aisacMap,
				acbCue.aisacControlMap,
				category
			);

			acbCue.refItem =
			{
				atom::reference_type::sequence,
				acbSequenceIndex
			};
		}
	}

	static unsigned short GenerateEmbeddedAwb(
		rad::span<const WaveformInfo> waveformsInfo,
		rad::stream& stream)
	{
		unsigned short curAwbID = 0;

		atom::wave_bank_writer writer(stream);
		writer.start();

		// Write AWB IDs.
		for (const auto& waveformInfo : waveformsInfo)
		{
			if (!waveformInfo.hasEmbeddedData) continue;

			for (unsigned char i = 0; i < waveformInfo.segmentCount; ++i)
			{
				writer.write_id(curAwbID++);
			}
		}

		// Write AWB data.
		writer.write_data_positions();

		for (const auto& waveformInfo : waveformsInfo)
		{
			if (!waveformInfo.hasEmbeddedData) continue;

			for (unsigned char i = 0; i < waveformInfo.segmentCount; ++i)
			{
				const auto& segmentInfo = waveformInfo.segments[i];
				writer.write_data({ segmentInfo.embeddedData, segmentInfo.dataSize });
			}
		}

		writer.finish();
		return curAwbID;
	}

	static void GenerateStreamingAwb(
		const char* streamFilePath,
		rad::span<const WaveformInfo> waveformsInfo,
		rad::stream& stream)
	{
		unsigned short curAwbID = 0;

		atom::wave_bank_writer writer(stream);
		writer.start(1);

		// Write AWB IDs.
		for (const auto& waveformInfo : waveformsInfo)
		{
			if (waveformInfo.hasEmbeddedData) continue;

			for (unsigned char i = 0; i < waveformInfo.segmentCount; ++i)
			{
				writer.write_id(curAwbID++);
			}
		}

		// Write AWB data.
		writer.write_data_positions();
		curAwbID = 0;

		stream.write_string8(streamFilePath);

		for (const auto& waveformInfo : waveformsInfo)
		{
			if (waveformInfo.hasEmbeddedData) continue;

			for (unsigned char i = 0; i < waveformInfo.segmentCount; ++i)
			{
				const auto& segmentInfo = waveformInfo.segments[i];
				const uint32_t arr[] =
				{
					0,
					static_cast<uint32_t>(segmentInfo.streamingDataPos),
					segmentInfo.dataSize
				};

				writer.write_data({ (const unsigned char*)arr, sizeof(arr)});
				//writer.fill_data_position(segmentInfo.streamingDataPos, segmentInfo.dataSize);
			}
		}

		writer.finish();

		//stream.jump_to(0);
		//stream.write_as<uint32_t>(0x324C4D48); // HML2
	}

	static bool HasAnyStreamingSoundElements(const audio::cue_sheet& csb)
	{
		for (const auto& soundElement : csb.soundElements)
		{
			if (soundElement.second.embeddedData.empty())
			{
				return true;
			}
		}

		return false;
	}

	static void GetWaveformInfoFromCSB(
		const audio::cue_sheet& csb,
		WaveformInfo* output,
		const packed_file* cpk = nullptr,
		rad::stream* cpkStream = nullptr)
	{
		for (const auto& soundElement : csb.soundElements)
		{
			switch (soundElement.second.format)
			{
			case audio::SOUND_ELEMENT_FORMAT_AAX:
				if (!soundElement.second.embeddedData.empty())
				{
					GetWaveformInfoFromAAXMemory(soundElement.second.embeddedData, *output);
				}
				else
				{
					assert(cpk && cpkStream);

					const auto& aaxCpkEntry = cpk->at(soundElement.first);
					const auto aaxCpkDataPos = aaxCpkEntry.get_proxy_data_offset();

					cpkStream->jump_to(aaxCpkDataPos);

					GetWaveformInfoFromAAXStream(*cpkStream, *output);
				}

				++output;
				break;

			default:
				throw std::runtime_error("Unsupported sound element data format");
			}
		}
	}

	static audio::cue_sheet ParseCSB(
		const void* data,
		unsigned long dataSize)
	{
		rad::readonly_memory_stream stream(data, dataSize);
		return audio::cue_sheet(stream);
	}

	static void UpgradeCSB(
		rad::stream& acbOutputStream,
		const void* data,
		unsigned long dataSize,
		std::string_view name,
		StreamingDataType streamDataType,
		const char* streamDataPath)
	{
		// Parse CSB data.
		const auto csb = ParseCSB(data, dataSize);

		// Check if we have any streaming sound elements.
		const bool usesStreaming = HasAnyStreamingSoundElements(csb);

		if (usesStreaming && streamDataType == StreamingDataType::None)
		{
			throw std::runtime_error("CSB has streaming sound elements but no streaming data was provided");
		}

		// Get waveform info.
		using WaveformInfoArr_t = rad::stack_or_heap_array<WaveformInfo, 32>;

		WaveformInfoArr_t waveformsInfo(
			csb.soundElements.size()
		);

		// Load CPK if necessary.
		if (streamDataType == StreamingDataType::Cpk)
		{
			//// Build original path to CPK.
			//static const std::string_view pathPrefix = ".\\image\\x64\\generations\\Sound\\";
			//static const std::string_view pathSuffix = ".cpk";

			//std::string originalPath;
			//originalPath.reserve(pathPrefix.size() + name.size() + pathSuffix.size());

			//originalPath.append(pathPrefix);
			//originalPath.append(name);
			//originalPath.append(pathSuffix);

			//// Replace path to CPK with mod file path.
			//std::string replacePath{};
			//const auto err = g_loader->binder->ResolvePath(originalPath.c_str(), &replacePath);
			
			//if (err == eBindError_None)
			//{
				//LOG("%s -> %s", originalPath.c_str(), replacePath.c_str());
			//}
			//else
			//{
				//throw std::runtime_error("Failed to find CPK for CSB with streaming data");
			//}

			// Load CPK.
			rad::file_stream cpkStream(
				streamDataPath,
				rad::file_stream::OPEN_MODE_READ_ONLY |
				rad::file_stream::OPEN_FLAG_SHARED | // TODO: Should we get exclusive ownership of the cpk file?
				rad::file_stream::OPEN_HINT_SEQUENTIAL_ACCESS // TODO: Should this be random access?
			);

			packed_file cpk;
			cpk.read(cpkStream, packed_file::data_read_mode::skip);

			GetWaveformInfoFromCSB(csb, waveformsInfo.data(), &cpk, &cpkStream);
		}

		// Load from folder
		else if (streamDataType == StreamingDataType::Cpk_redirect_folder)
		{
			// TODO
			throw std::runtime_error("Using streaming audio data from a cpk-redirect folder is not yet supported");
		}

		// Load embedded data.
		else
		{
			GetWaveformInfoFromCSB(csb, waveformsInfo.data());
		}

		// Generate AWBs.
		rad::memory_stream embeddedAWBStream;
		const auto embeddedWaveformCount = GenerateEmbeddedAwb(
			waveformsInfo,
			embeddedAWBStream
		);

		rad::memory_stream streamingAWBStream;

		if (usesStreaming)
		{
			GenerateStreamingAwb(streamDataPath, waveformsInfo, streamingAWBStream);
		}

		// Generate ACB.
		const CSBUpgrader upgrader(
			csb,
			waveformsInfo,
			name
		);

		// Write ACB data out to memory stream.
		upgrader.result().write(
			acbOutputStream,
			(embeddedWaveformCount) ? embeddedAWBStream.data() : nullptr,
			streamingAWBStream.data(),
			//atom::packed_version(1, 12, 00)
			atom::packed_version(1, 30, 00)
		);
	}

	bool TryUpgradeCSB(
		rad::memory_stream& acbOutputStream,
		const void* data,
		unsigned long dataSize,
		std::string_view name,
		StreamingDataType streamDataType,
		const char* streamDataPath)
	{
		if (streamDataPath && streamDataType == StreamingDataType::None)
		{
			LOG("Mod csb + no cpk combination is not yet supported; falling back to no redirection");
		}

		try
		{
			UpgradeCSB(
				acbOutputStream,
				data,
				dataSize,
				name,
				streamDataType,
				streamDataPath
			);
		}
		catch (const std::exception& ex)
		{
			LOG("Failed to upgrade %s.csb - \"%s\"", name.data(), ex.what());
			return false;
		}
		catch (...)
		{
			LOG("Failed to upgrade %s.csb", name.data());
			return false;
		}

		return true;
	}
}