#include "Gens2024LegacyAudioParser.h"
#include <hedgelib/cri/hl_cri_aax.h>
#include <rad/rad_memory_stream.h>
#include <rad/rad_endian.h>

using namespace hl::cri_new;

namespace gens2024
{
	void GetWaveformInfoFromAAXMemory(
		rad::span<const unsigned char> embeddedAaxData,
		WaveformInfo& output)
	{
		rad::readonly_memory_stream aaxStream(embeddedAaxData);
		const auto aaxEntries = audio::read_aax_entries(aaxStream);
		bool hasAnyLoopingSegments = false;

		if (aaxEntries.size() > std::size(output.segments))
		{
			throw std::runtime_error("Unsupported AAX segment count");
		}

		for (std::size_t i = 0; i < aaxEntries.size(); ++i)
		{
			const auto& aaxEntry = aaxEntries[i];
			const auto aaxSegment = aaxStream.data().slice_unchecked(
				aaxEntry.dataPosition,
				aaxEntry.dataSize
			);

			const auto adx = reinterpret_cast<const RawADXHeader*>(aaxSegment.data());

			if (rad::endian::big_to_native(adx->headerSize) < 0x20)
			{
				throw std::runtime_error("Invalid or unsupported ADX header");
			}

			auto& segmentInfo = output.segments[i];
			segmentInfo.embeddedData = aaxSegment.data();
			segmentInfo.dataSize = static_cast<unsigned long>(aaxSegment.size());
			segmentInfo.sampleCount = rad::endian::big_to_native(adx->sampleCount);

			if (aaxEntry.doesLoop)
			{
				output.loopSegmentIndex = static_cast<unsigned char>(i);
				hasAnyLoopingSegments = true;
			}
		}

		output.segmentCount = static_cast<unsigned char>(aaxEntries.size());
		output.doesLoop = hasAnyLoopingSegments;
		output.hasEmbeddedData = true;
	}

	void GetWaveformInfoFromAAXStream(
		rad::stream& stream,
		WaveformInfo& output)
	{
		const auto aaxEntries = audio::read_aax_entries(stream);
		bool hasAnyLoopingSegments = false;

		if (aaxEntries.size() > std::size(output.segments))
		{
			throw std::runtime_error("Unsupported AAX segment count");
		}

		for (std::size_t i = 0; i < aaxEntries.size(); ++i)
		{
			const auto& aaxEntry = aaxEntries[i];
			stream.jump_to(aaxEntry.dataPosition);

			RawADXHeader adx;
			stream.read_as(adx);

			if (rad::endian::big_to_native(adx.headerSize) < 0x20)
			{
				throw std::runtime_error("Invalid or unsupported ADX header");
			}

			auto& segmentInfo = output.segments[i];
			segmentInfo.streamingDataPos = aaxEntry.dataPosition;
			segmentInfo.dataSize = aaxEntry.dataSize;
			segmentInfo.sampleCount = rad::endian::big_to_native(adx.sampleCount);

			if (aaxEntry.doesLoop)
			{
				output.loopSegmentIndex = static_cast<unsigned char>(i);
				hasAnyLoopingSegments = true;
			}
		}

		output.segmentCount = static_cast<unsigned char>(aaxEntries.size());
		output.doesLoop = hasAnyLoopingSegments;
		output.hasEmbeddedData = false;
	}
}
