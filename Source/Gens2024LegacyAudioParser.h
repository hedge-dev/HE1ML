#pragma once
#include <rad/rad_allocator.h>
#include <rad/rad_stream.h>
#include <rad/rad_span.h>
#include <rad/rad_vector.h>
#include <rad/rad_string.h>
#include <rad/rad_file.h>
#include <hedgelib/cri/hl_cri_packed_file.h>
#include <hedgelib/cri/hl_cri_audio_cue_sheet.h>
#include <hedgelib/cri/hl_cri_atom_cue_sheet.h>

namespace gens2024
{
	struct ACBPatchInfo;

	struct RawADXHeader
	{
		uint16_t magic;
		uint16_t headerSize;
		uint8_t encodingType;
		uint8_t frameSize;
		uint8_t bitsPerSample;
		uint8_t channelCount;
		uint32_t sampleRate;
		uint32_t sampleCount;
		uint16_t cutoff;
		uint16_t version;
		uint32_t unknown1;
	};

	struct AAXSegmentInfo
	{
		union
		{
			const unsigned char* embeddedData;
			unsigned long long streamingDataPos;
		};

		unsigned long dataSize;
		unsigned long sampleCount;
	};

	struct SoundElementInfo
	{
		unsigned short firstSegmentGlobalIndex = 0;

		unsigned char segmentCount : 4 = 0;

		// The index of the loop segment, or 0xF
		// if the sound element does not loop.
		unsigned char loopSegmentIndex : 4 = 0xF;

		unsigned char channelCount = 0;
		unsigned long sampleRate = 0;

		// The path to the streaming file, or empty
		// if the sound element data is embedded.
		std::string_view streamingFilePath;

		AAXSegmentInfo segments[4];

		inline bool DoesLoop() const noexcept
		{
			return loopSegmentIndex != 0xF;
		}

		inline bool HasEmbeddedData() const noexcept
		{
			return streamingFilePath.empty();
		}

		inline bool HasStreamingData() const noexcept
		{
			return !streamingFilePath.empty();
		}

		unsigned long ComputeRealSampleCount(
			unsigned char segmentIndex) const
		{
			unsigned long totalSampleCount = 0;

			for (unsigned char i = 0; i < segmentIndex; ++i)
			{
				totalSampleCount += segments[i].sampleCount;
			}

			return totalSampleCount;
		}

		float ComputeStartSeconds(
			unsigned char segmentIndex,
			unsigned long sampleRate) const
		{
			const auto totalSampleCount = ComputeRealSampleCount(segmentIndex);

			return (
				static_cast<float>(totalSampleCount) /
				sampleRate
			);
		}

		float ComputeEndSeconds(unsigned long sampleRate) const
		{
			const auto totalSampleCount = ComputeRealSampleCount(segmentCount);

			return (
				static_cast<float>(totalSampleCount) /
				sampleRate
			);
		}
	};

	SoundElementInfo GetSoundElementInfoFromAAXMemory(
		rad::span<const unsigned char> aaxData,
		unsigned short firstSegmentGlobalIndex = 0
	);

	SoundElementInfo GetSoundElementInfoFromAAXStreaming(
		rad::stream& stream,
		unsigned long long aaxSize,
		std::string_view streamingFilePath,
		unsigned short firstSegmentGlobalIndex = 0
	);

	hl::cri::audio::cue_sheet ReadCSB(
		const void* csbData,
		unsigned long csbDataSize,
		rad::allocator& tmpAllocator = rad::default_allocator,
		rad::allocator& csbAllocator = rad::default_allocator
	);

	class CPKStreamer
	{
		rad::string m_cpkPath;
		rad::file_stream m_cpkStream;
		hl::cri::packed_file m_cpk;

	public:
		SoundElementInfo GetSoundElementInfoFromAAXFile(
			const rad::string& localAaxFilePath,
			unsigned short firstSegmentGlobalIndex = 0
		);

		CPKStreamer(rad::string cpkPath);
	};

	struct SoundElementStreamingInfo
	{
		rad::optional_string redirectDir;
		CPKStreamer* redirectCPK = nullptr;
		std::string_view cleanAwbPath;
	};

	struct SoundElementInfos
	{
		rad::vector<rad::string> redirectPaths;
		rad::vector<SoundElementInfo> data;
		unsigned short totalSegmentCount = 0;
		unsigned short totalEmbeddedDataCount = 0;
		unsigned short totalStreamingDataCount = 0;

		SoundElementInfos(
			rad::allocator& allocator = rad::default_allocator) noexcept
			: redirectPaths(allocator)
			, data(allocator)
		{
		}
	};

	SoundElementInfos GetSoundElementInfosForUpgrade(
		const hl::cri::audio::cue_sheet& csb,
		const SoundElementStreamingInfo* streamingInfo = nullptr,
		rad::allocator& allocator = rad::default_allocator
	);

	SoundElementInfos GetSoundElementInfosForPatch(
		const rad::span<const ACBPatchInfo>& patchInfo,
		const SoundElementStreamingInfo& streamingInfo,
		rad::allocator& allocator = rad::default_allocator
	);

	struct HE1MLExtraWaveformData
	{
		uint64_t dataOffset;
		uint32_t dataSize;
		uint32_t streamingPathOff;
	};

	void GenerateEmbeddedAWB(
		const SoundElementInfos& soundElementInfos,
		rad::stream& outputStream
	);

	void GenerateStreamingAWB(
		const SoundElementInfos& soundElementInfos,
		rad::stream& outputStream
	);

	void InsertTrackEventPlaySynthCommands(
		hl::cri::atom::command_table& evCmd,
		hl::cri::atom::command_table::const_iterator pos,
		const SoundElementInfo& soundElementInfo,
		std::size_t firstSynthIndex,
		hl::u16 loopIndex = 0
	);
}
