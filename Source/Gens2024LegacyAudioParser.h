#pragma once
#include <rad/rad_stream.h>
#include <rad/rad_span.h>

namespace gens2024
{
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

	struct WaveformInfo
	{
		AAXSegmentInfo segments[2];
		unsigned char segmentCount;
		bool doesLoop;
		unsigned char loopSegmentIndex;
		bool hasEmbeddedData;

		unsigned long ComputeSampleCount(
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
			const auto totalSampleCount = ComputeSampleCount(segmentIndex);

			return (
				static_cast<float>(totalSampleCount) /
				sampleRate
			);
		}

		float ComputeEndSeconds(unsigned long sampleRate) const
		{
			const auto totalSampleCount = ComputeSampleCount(segmentCount);

			return (
				static_cast<float>(totalSampleCount) /
				sampleRate
			);
		}
	};

	void GetWaveformInfoFromAAXMemory(
		rad::span<const unsigned char> embeddedAaxData,
		WaveformInfo& output
	);

	void GetWaveformInfoFromAAXStream(
		rad::stream& stream,
		WaveformInfo& output
	);
}
