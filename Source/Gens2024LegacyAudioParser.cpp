#include <algorithm>
#include <rad/rad_endian.h>
#include <rad/rad_memory_stream.h>
#include <rad/rad_path.h>
#include <hedgelib/cri/hl_cri_aax.h>
#include <hedgelib/cri/hl_cri_atom_wave_bank.h>
#include "Gens2024LegacyAudioParser.h"
#include "Gens2024LegacyAudioPatcher.h"

using namespace hl::cri;

namespace gens2024
{
	SoundElementInfo GetSoundElementInfoFromAAXMemory(
		rad::span<const unsigned char> aaxData,
		unsigned short firstSegmentGlobalIndex)
	{
		SoundElementInfo soundElementInfo;
		rad::readonly_memory_stream stream(aaxData);
		const auto aaxEntries = audio::read_aax_entries(stream);

		if (aaxEntries.size() > std::size(soundElementInfo.segments))
		{
			throw std::runtime_error("Unsupported AAX segment count");
		}

		for (std::size_t i = 0; i < aaxEntries.size(); ++i)
		{
			const auto& aaxEntry = aaxEntries[i];

			if (aaxEntry.dataPosition + aaxEntry.dataSize > aaxData.size())
			{
				throw std::runtime_error("AAX segment surpasses end of AAX data");
			}

			if (aaxEntry.dataSize < sizeof(RawADXHeader))
			{
				throw std::runtime_error("Invalid or unsupported ADX header");
			}

			const auto aaxSegment = aaxData.slice_unchecked(
				aaxEntry.dataPosition,
				aaxEntry.dataSize
			);

			const auto adx = reinterpret_cast<const RawADXHeader*>(aaxSegment.data());

			const auto sampleRate = rad::endian::big_to_native(adx->sampleRate);

			if (soundElementInfo.sampleRate &&
				sampleRate != soundElementInfo.sampleRate)
			{
				throw std::runtime_error(
					"AAX segments with differing sample rates are not supported"
				);
			}

			soundElementInfo.sampleRate = sampleRate;

			if (soundElementInfo.channelCount &&
				adx->channelCount != soundElementInfo.channelCount)
			{
				throw std::runtime_error(
					"AAX segments with differing channel counts are not supported"
				);
			}

			soundElementInfo.channelCount = adx->channelCount;

			auto& segmentInfo = soundElementInfo.segments[i];
			segmentInfo.embeddedData = aaxSegment.data();
			segmentInfo.dataSize = aaxEntry.dataSize;
			segmentInfo.sampleCount = rad::endian::big_to_native(adx->sampleCount);

			if (aaxEntry.doesLoop)
			{
				if (soundElementInfo.DoesLoop())
				{
					throw std::runtime_error(
						"Multiple looping segments in one AAX is not supported"
					);
				}

				soundElementInfo.loopSegmentIndex = static_cast<unsigned char>(i);
			}
		}

		soundElementInfo.firstSegmentGlobalIndex = firstSegmentGlobalIndex;
		soundElementInfo.segmentCount = static_cast<unsigned char>(aaxEntries.size());

		return soundElementInfo;
	}

	SoundElementInfo GetSoundElementInfoFromAAXStreaming(
		rad::stream& stream,
		unsigned long long aaxSize,
		std::string_view streamingFilePath,
		unsigned short firstSegmentGlobalIndex)
	{
		SoundElementInfo soundElementInfo;
		const auto aaxStartPos = stream.tell();
		const auto aaxEntries = audio::read_aax_entries(stream);

		if (aaxEntries.size() > std::size(soundElementInfo.segments))
		{
			throw std::runtime_error("Unsupported AAX segment count");
		}

		for (std::size_t i = 0; i < aaxEntries.size(); ++i)
		{
			const auto& aaxEntry = aaxEntries[i];

			if ((aaxEntry.dataPosition - aaxStartPos) + aaxEntry.dataSize > aaxSize)
			{
				throw std::runtime_error("AAX segment surpasses end of AAX data");
			}

			if (aaxEntry.dataSize < sizeof(RawADXHeader))
			{
				throw std::runtime_error("Invalid or unsupported ADX header");
			}

			stream.jump_to(aaxEntry.dataPosition);

			RawADXHeader adx;
			stream.read_as(adx);

			const auto sampleRate = rad::endian::big_to_native(adx.sampleRate);

			if (soundElementInfo.sampleRate &&
				sampleRate != soundElementInfo.sampleRate)
			{
				throw std::runtime_error(
					"AAX segments with differing sample rates are not supported"
				);
			}

			soundElementInfo.sampleRate = sampleRate;

			if (soundElementInfo.channelCount &&
				adx.channelCount != soundElementInfo.channelCount)
			{
				throw std::runtime_error(
					"AAX segments with differing channel counts are not supported"
				);
			}

			soundElementInfo.channelCount = adx.channelCount;

			auto& segmentInfo = soundElementInfo.segments[i];
			segmentInfo.streamingDataPos = aaxEntry.dataPosition;
			segmentInfo.dataSize = aaxEntry.dataSize;
			segmentInfo.sampleCount = rad::endian::big_to_native(adx.sampleCount);

			if (aaxEntry.doesLoop)
			{
				if (soundElementInfo.DoesLoop())
				{
					throw std::runtime_error(
						"Multiple looping segments in one AAX is not supported"
					);
				}

				soundElementInfo.loopSegmentIndex = static_cast<unsigned char>(i);
			}
		}

		soundElementInfo.firstSegmentGlobalIndex = firstSegmentGlobalIndex;
		soundElementInfo.segmentCount = static_cast<unsigned char>(aaxEntries.size());
		soundElementInfo.streamingFilePath = streamingFilePath;

		return soundElementInfo;
	}

	audio::cue_sheet ReadCSB(
		const void* csbData,
		unsigned long csbDataSize,
		rad::allocator& tmpAllocator,
		rad::allocator& csbAllocator)
	{
		rad::readonly_memory_stream stream(csbData, csbDataSize);
		return audio::cue_sheet{stream, tmpAllocator, csbAllocator};
	}

	SoundElementInfo CPKStreamer::GetSoundElementInfoFromAAXFile(
		const rad::string& localAaxFilePath, // TODO: Change type to string_view once we update find function to take a string_view
		unsigned short firstSegmentGlobalIndex)
	{
		const auto it = m_cpk.find(localAaxFilePath);

		if (it == m_cpk.end())
		{
			throw std::runtime_error(
				"CSB streaming sound element was not found in the CPK"
			);
		}

		m_cpkStream.jump_to(it->second.get_proxy_data_offset());

		return GetSoundElementInfoFromAAXStreaming(
			m_cpkStream,
			it->second.extract_size(),
			m_cpkPath,
			firstSegmentGlobalIndex
		);
	}

	CPKStreamer::CPKStreamer(rad::string cpkPath)
		: m_cpkPath(std::move(cpkPath))
		, m_cpkStream(m_cpkPath.c_str(), rad::file_stream::OPEN_MODE_READ_ONLY)
		, m_cpk(m_cpkStream, packed_file::data_read_mode::skip)
	{
	}

	static bool TryGetStreamingSoundElementInfo(
		SoundElementInfo& outSoundElementInfo,
		rad::vector<rad::string>& redirectPaths,
		std::string_view localAaxFilePath,
		const SoundElementStreamingInfo& streamingInfo,
		unsigned short firstSegmentGlobalIndex = 0)
	{
		// Attempt to get sound element info from redirected AAX file.
		if (streamingInfo.redirectDir)
		{
			auto redirectFile = rad::path::combine(
				streamingInfo.redirectDir.view(),
				localAaxFilePath
			);

			if (rad::path::exists(redirectFile.c_str()))
			{
				rad::file_stream aaxStream(
					redirectFile.c_str(),
					rad::file_stream::OPEN_MODE_READ_ONLY
				);

				auto& redirectPath = redirectPaths.emplace_back(redirectFile);

				outSoundElementInfo = GetSoundElementInfoFromAAXStreaming(
					aaxStream,
					aaxStream.get_size(),
					redirectPath,
					firstSegmentGlobalIndex
				);

				return true;
			}
		}

		// Attempt to get sound element info from redirected CPK file.
		if (streamingInfo.redirectCPK)
		{
			outSoundElementInfo = streamingInfo.redirectCPK->GetSoundElementInfoFromAAXFile(
				localAaxFilePath,
				firstSegmentGlobalIndex
			);

			return true;
		}

		// Failed to get streaming sound element info.
		return false;
	}

	static SoundElementInfo GetSoundElementInfoForUpgrade(
		const hl::cri::audio::sound_element& soundElement,
		rad::vector<rad::string>& redirectPaths,
		const SoundElementStreamingInfo* streamingInfo,
		unsigned short firstSegmentGlobalIndex = 0)
	{
		if (soundElement.format != audio::sound_element_format::aax)
		{
			throw std::runtime_error("Unsupported CSB sound element format");
		}
		
		if (!soundElement.embeddedData.empty())
		{
			return GetSoundElementInfoFromAAXMemory(
				soundElement.embeddedData,
				firstSegmentGlobalIndex
			);
		}
		else
		{
			if (!streamingInfo)
			{
				throw std::runtime_error(
					"CSB has a streaming sound element, but no streaming file is set"
				);
			}

			SoundElementInfo soundElementInfo;

			if (!TryGetStreamingSoundElementInfo(
				soundElementInfo,
				redirectPaths,
				soundElement.name,
				*streamingInfo,
				firstSegmentGlobalIndex))
			{
				// Attempt to get sound element info from built-in streaming
				// sound element info for vanilla Gens 2011.
				// TODO: This will be used in the case where you have a custom CSB but no custom CPK or streaming directory!
				// Or, alternatively, in the case where you have a custom CSB and a custom streaming directory
				// but not all of the sound elements have override files within said streaming directory.
				// In those cases, we want to stream directly from the vanilla 2024 AWB!
				throw std::runtime_error("Not yet supported"); // TODO
			}

			return soundElementInfo;
		}
	}

	SoundElementInfos GetSoundElementInfosForUpgrade(
		const hl::cri::audio::cue_sheet& csb,
		const SoundElementStreamingInfo* streamingInfo,
		rad::allocator& allocator)
	{
		SoundElementInfos soundElementInfos(allocator);
		soundElementInfos.data.reserve(csb.soundElements.size());

		for (const auto& soundElement : csb.soundElements)
		{
			const auto& soundElementInfo = soundElementInfos.data.push_back_unchecked(
				GetSoundElementInfoForUpgrade(
					soundElement,
					soundElementInfos.redirectPaths,
					streamingInfo,
					soundElementInfos.totalSegmentCount
				)
			);

			const auto hasEmbeddedData = soundElementInfo.HasEmbeddedData();
			soundElementInfos.totalSegmentCount += soundElementInfo.segmentCount;
			soundElementInfos.totalEmbeddedDataCount += hasEmbeddedData;
			soundElementInfos.totalStreamingDataCount += !hasEmbeddedData;
		}

		return soundElementInfos;
	}

	SoundElementInfos GetSoundElementInfosForPatch(
		const rad::span<const ACBPatchInfo>& patchInfo,
		const SoundElementStreamingInfo& streamingInfo,
		rad::allocator& allocator)
	{
		SoundElementInfos soundElementInfos(allocator);
		soundElementInfos.data.reserve(patchInfo.size());

		for (const auto& info : patchInfo)
		{
			SoundElementInfo soundElementInfo;

			const bool gotStreamingSoundElementInfo = TryGetStreamingSoundElementInfo(
				soundElementInfo,
				soundElementInfos.redirectPaths,
				info.localStreamingPath,
				streamingInfo,
				soundElementInfos.totalSegmentCount
			);

			soundElementInfos.data.push_back_unchecked(
				soundElementInfo
			);

			soundElementInfos.totalSegmentCount += soundElementInfo.segmentCount;
			soundElementInfos.totalStreamingDataCount += gotStreamingSoundElementInfo;
		}

		return soundElementInfos;
	}

	void GenerateEmbeddedAWB(
		const SoundElementInfos& soundElementInfos,
		rad::stream& outputStream)
	{
		atom::wave_bank_serializer sr(outputStream);
		sr.start();

		// Write waveform IDs.
		hl::u16 curAwbId = 0;

		for (const auto& soundElementInfo : soundElementInfos.data)
		{
			if (soundElementInfo.HasStreamingData()) continue;

			for (unsigned char i = 0; i < soundElementInfo.segmentCount; ++i)
			{
				sr.write_id(curAwbId++);
			}
		}

		// Write waveform data.
		auto wr = sr.begin_data_section();

		for (const auto& soundElementInfo : soundElementInfos.data)
		{
			if (soundElementInfo.HasStreamingData()) continue;

			for (unsigned char i = 0; i < soundElementInfo.segmentCount; ++i)
			{
				const auto& segment = soundElementInfo.segments[i];

				wr.start();
				wr.stream().write(segment.embeddedData, segment.dataSize);
				wr.finish();
			}
		}

		sr.finish();
	}

	void GenerateStreamingAWB(
		const SoundElementInfos& soundElementInfos,
		rad::stream& outputStream)
	{
		atom::wave_bank_serializer sr(outputStream);
		const auto awbStartPos = outputStream.tell();

		sr.start(atom::wave_bank_info{
			.version = 1,
			.dataAlignment = 1
		});

		// Get first streaming path.
		std::string_view firstStreamingPath;
		for (const auto& soundElementInfo : soundElementInfos.data)
		{
			if (soundElementInfo.HasEmbeddedData()) continue;

			firstStreamingPath = soundElementInfo.streamingFilePath;
			break;
		}

		// Write waveform IDs.
		hl::u16 curAwbId = 0;
		bool allStreamingPathsAreSame = true;

		for (const auto& soundElementInfo : soundElementInfos.data)
		{
			if (soundElementInfo.HasEmbeddedData()) continue;

			if (soundElementInfo.streamingFilePath != firstStreamingPath)
			{
				allStreamingPathsAreSame = false;
			}

			for (unsigned char i = 0; i < soundElementInfo.segmentCount; ++i)
			{
				sr.write_id(curAwbId++);
			}
		}

		// Write HE1ML extra data.
		// NOTE: This data is only for the purposes of
		// HE1ML; it is not present in real AWB files.
		auto wr = sr.begin_data_section();
		const auto awbDataSectionPos = outputStream.tell();
		uint32_t curStreamingPathOff = 0;

		if (allStreamingPathsAreSame)
		{
			wr.stream().write(firstStreamingPath.data(), firstStreamingPath.size());
			wr.stream().write_as(uint8_t{0});
		}

		for (const auto& soundElementInfo : soundElementInfos.data)
		{
			if (soundElementInfo.HasEmbeddedData()) continue;

			if (!allStreamingPathsAreSame)
			{
				curStreamingPathOff = static_cast<uint32_t>(
					wr.stream().tell() - awbDataSectionPos
				);

				wr.stream().write(
					soundElementInfo.streamingFilePath.data(),
					soundElementInfo.streamingFilePath.size()
				);

				wr.stream().write_as(uint8_t{0});
			}

			for (unsigned char i = 0; i < soundElementInfo.segmentCount; ++i)
			{
				const auto& segment = soundElementInfo.segments[i];

				wr.start();

				const HE1MLExtraWaveformData data =
				{
					.dataOffset = segment.streamingDataPos,
					.dataSize = segment.dataSize,
					.streamingPathOff = curStreamingPathOff
				};

				wr.stream().write_as(data);
				wr.finish();
			}
		}

		sr.finish();

		// HACK: Use subkey field as special marker
		// indicating this AWB has extra HE1ML data.
		outputStream.jump_to(awbStartPos + 14);
		outputStream.write_as<uint16_t>(67);
	}

	void InsertTrackEventPlaySynthCommands(
		hl::cri::atom::command_table& evCmd,
		hl::cri::atom::command_table::const_iterator pos,
		const SoundElementInfo& soundElementInfo,
		std::size_t firstSynthIndex,
		hl::u16 loopIndex)
	{
		double totalSampleCount = 0.0;

		for (unsigned char segmentIndex = 0;
			segmentIndex < soundElementInfo.segmentCount;
			++segmentIndex)
		{
			const auto& segment = soundElementInfo.segments[segmentIndex];

			// Append ACB track commands for segment.
			const auto segmentStartPosSec = (
				totalSampleCount / soundElementInfo.sampleRate
			);

			const auto segmentStartPosMilli = static_cast<hl::u32>(
				segmentStartPosSec * 1000.0
			);

			if (segmentStartPosMilli != 0)
			{
				pos = evCmd.insert_wait(pos, segmentStartPosMilli);
				++pos;
			}

			if (segmentIndex == soundElementInfo.loopSegmentIndex)
			{
				pos = evCmd.insert_seq_loop_start(pos, loopIndex);
				++pos;
			}

			// TODO: Replace this with a play synth command!!!
			pos = evCmd.insert_play(
				pos,
				atom::ref_type::synth,
				static_cast<hl::u16>(firstSynthIndex + segmentIndex)
			);

			++pos;
			totalSampleCount += segment.sampleCount;

			if (segmentIndex == soundElementInfo.loopSegmentIndex)
			{
				const auto segmentEndPosSec = (
					totalSampleCount / soundElementInfo.sampleRate
				);

				const auto loopDurationMilli = static_cast<hl::u32>(
					((segmentEndPosSec - segmentStartPosSec) * 1000.0)
				);

				pos = evCmd.insert_wait(pos, loopDurationMilli);
				++pos;

				pos = evCmd.insert_seq_loop_end(pos, loopIndex, loopDurationMilli);
				++pos;

				if (soundElementInfo.segmentCount > segmentIndex + 1)
				{
					throw std::runtime_error(
						"Additional AAX segments after a loop segment "
						"are not yet supported"
					);
				}
			}
		}
	}
}
