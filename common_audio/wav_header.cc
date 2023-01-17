/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Based on the WAV file format documentation at
// https://ccrma.stanford.edu/courses/422/projects/WaveFormat/ and
// http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html

#include "common_audio/wav_header.h"

#include <cstring>
#include <limits>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/sanitizer.h"
#include "rtc_base/system/arch.h"

namespace webrtc {
namespace {

#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Code not working properly for big endian platforms."
#endif

#pragma pack(2)
struct ChunkHeader {
  uint32_t ID;
  uint32_t Size;
};
static_assert(sizeof(ChunkHeader) == 8, "ChunkHeader size");

#pragma pack(2)
struct RiffHeader {
  ChunkHeader header;
  uint32_t Format;
};
static_assert(sizeof(RiffHeader) == sizeof(ChunkHeader) + 4, "RiffHeader size");

using NumChannelsType = uint16_t;
using SampleRateType = uint32_t;
using ByteRateType = uint32_t;
using BitsPerSampleType = uint16_t;

// We can't nest this definition in WavHeader, because VS2013 gives an error
// on sizeof(WavHeader::fmt): "error C2070: 'unknown': illegal sizeof operand".
#pragma pack(2)
struct FmtPcmSubchunk {
  ChunkHeader header;
  uint16_t AudioFormat;
  NumChannelsType NumChannels;
  SampleRateType SampleRate;
  ByteRateType ByteRate;
  uint16_t BlockAlign;
  BitsPerSampleType BitsPerSample;
};
static_assert(sizeof(FmtPcmSubchunk) == 24, "FmtPcmSubchunk size");
const uint32_t kFmtPcmSubchunkSize =
    sizeof(FmtPcmSubchunk) - sizeof(ChunkHeader);

// Pack struct to avoid additional padding bytes.
#pragma pack(2)
struct FmtIeeeFloatSubchunk {
  ChunkHeader header;
  uint16_t AudioFormat;
  NumChannelsType NumChannels;
  SampleRateType SampleRate;
  ByteRateType ByteRate;
  uint16_t BlockAlign;
  BitsPerSampleType BitsPerSample;
  uint16_t ExtensionSize;
};
static_assert(sizeof(FmtIeeeFloatSubchunk) == 26, "FmtIeeeFloatSubchunk size");
const uint32_t kFmtIeeeFloatSubchunkSize =
    sizeof(FmtIeeeFloatSubchunk) - sizeof(ChunkHeader);

// Simple PCM wav header. It does not include chunks that are not essential to
// read audio samples.
#pragma pack(2)
struct WavHeaderPcm {
  RiffHeader riff;
  FmtPcmSubchunk fmt;
  struct {
    ChunkHeader header;
  } data;
};
static_assert(sizeof(WavHeaderPcm) == kPcmWavHeaderSize,
              "no padding in header");

// IEEE Float Wav header, includes extra chunks necessary for proper non-PCM
// WAV implementation.
#pragma pack(2)
struct WavHeaderIeeeFloat {
  RiffHeader riff;
  FmtIeeeFloatSubchunk fmt;
  struct {
    ChunkHeader header;
    uint32_t SampleLength;
  } fact;
  struct {
    ChunkHeader header;
  } data;
};
static_assert(sizeof(WavHeaderIeeeFloat) == kIeeeFloatWavHeaderSize,
              "no padding in header");

uint32_t PackFourCC(char a, char b, char c, char d) {
  uint32_t packed_value =
      static_cast<uint32_t>(a) | static_cast<uint32_t>(b) << 8 |
      static_cast<uint32_t>(c) << 16 | static_cast<uint32_t>(d) << 24;
  return packed_value;
}

std::string ReadFourCC(uint32_t x) {
  return std::string(reinterpret_cast<char*>(&x), 4);
}

uint16_t MapWavFormatToHeaderField(WavFormat format) {
  switch (format) {
    case WavFormat::kWavFormatPcm:
      return 1;
    case WavFormat::kWavFormatIeeeFloat:
      return 3;
    case WavFormat::kWavFormatALaw:
      return 6;
    case WavFormat::kWavFormatMuLaw:
      return 7;
  }
  RTC_CHECK_NOTREACHED();
}

WavFormat MapHeaderFieldToWavFormat(uint16_t format_header_value) {
  if (format_header_value == 1) {
    return WavFormat::kWavFormatPcm;
  }
  if (format_header_value == 3) {
    return WavFormat::kWavFormatIeeeFloat;
  }

  RTC_CHECK(false) << "Unsupported WAV format";
}

uint32_t RiffChunkSize(size_t bytes_in_payload, size_t header_size) {
  return static_cast<uint32_t>(bytes_in_payload + header_size -
                               sizeof(ChunkHeader));
}

ByteRateType ByteRate(size_t num_channels,
                      int sample_rate,
                      size_t bytes_per_sample) {
  return static_cast<ByteRateType>(num_channels * sample_rate *
                                   bytes_per_sample);
}

uint16_t BlockAlign(size_t num_channels, size_t bytes_per_sample) {
  return static_cast<uint16_t>(num_channels * bytes_per_sample);
}

// Finds a chunk having the sought ID. If found, then `readable` points to the
// first byte of the sought chunk data. If not found, the end of the file is
// reached.
void FindWaveChunk(ChunkHeader* chunk_header,
                   WavHeaderReader* readable,
                   const std::string sought_chunk_id) {
  RTC_DCHECK_EQ(sought_chunk_id.size(), 4);
  while (true) {
    RTC_CHECK_EQ(readable->Read(chunk_header, sizeof(*chunk_header)),
                 sizeof(*chunk_header))
        << "Cannot find '" << sought_chunk_id << "' chunk";
    if (ReadFourCC(chunk_header->ID) == sought_chunk_id)
      return;  // Sought chunk found.
    // Ignore current chunk by skipping its payload.
    RTC_CHECK(readable->SeekForward(chunk_header->Size))
        << "Cannot find '" << sought_chunk_id << "' chunk";
  }
}

void ReadFmtChunkData(FmtPcmSubchunk* fmt_subchunk, WavHeaderReader* readable) {
  // Reads "fmt " chunk payload.
  RTC_CHECK_EQ(
      readable->Read(&(fmt_subchunk->AudioFormat), kFmtPcmSubchunkSize),
      kFmtPcmSubchunkSize)
      << "Invalid 'fmt ' chunk: incomplete chunk";
  const uint32_t fmt_size = fmt_subchunk->header.Size;
  if (fmt_size != kFmtPcmSubchunkSize) {
    // There is an optional two-byte extension field permitted to be present
    // with PCM, but which must be zero.
    int16_t ext_size;
    RTC_CHECK_EQ(kFmtPcmSubchunkSize + sizeof(ext_size), fmt_size)
        << "Invalid 'fmt ' chunk: incorrect size when accounting for two-byte "
           "extension field";
    RTC_CHECK_EQ(readable->Read(&ext_size, sizeof(ext_size)), sizeof(ext_size))
        << "Invalid 'fmt ' chunk: incomplete extension field";
    RTC_CHECK_EQ(ext_size, 0)
        << "Invalid 'fmt ' chunk: non-zero extension field";
  }
}

void WritePcmWavHeader(size_t num_channels,
                       int sample_rate,
                       size_t bytes_per_sample,
                       size_t num_samples,
                       uint8_t* buf,
                       size_t* header_size) {
  RTC_CHECK(buf);
  RTC_CHECK(header_size);
  *header_size = kPcmWavHeaderSize;
  auto header = rtc::MsanUninitialized<WavHeaderPcm>({});
  const size_t bytes_in_payload = bytes_per_sample * num_samples;

  header.riff.header.ID = PackFourCC('R', 'I', 'F', 'F');
  header.riff.header.Size = RiffChunkSize(bytes_in_payload, *header_size);
  header.riff.Format = PackFourCC('W', 'A', 'V', 'E');
  header.fmt.header.ID = PackFourCC('f', 'm', 't', ' ');
  header.fmt.header.Size = kFmtPcmSubchunkSize;
  header.fmt.AudioFormat = MapWavFormatToHeaderField(WavFormat::kWavFormatPcm);
  header.fmt.NumChannels = static_cast<NumChannelsType>(num_channels);
  header.fmt.SampleRate = sample_rate;
  header.fmt.ByteRate = ByteRate(num_channels, sample_rate, bytes_per_sample);
  header.fmt.BlockAlign = BlockAlign(num_channels, bytes_per_sample);
  header.fmt.BitsPerSample =
      static_cast<BitsPerSampleType>(8 * bytes_per_sample);
  header.data.header.ID = PackFourCC('d', 'a', 't', 'a');
  header.data.header.Size = static_cast<uint32_t>(bytes_in_payload);

  // Do an extra copy rather than writing everything to buf directly, since buf
  // might not be correctly aligned.
  memcpy(buf, &header, *header_size);
}

void WriteIeeeFloatWavHeader(size_t num_channels,
                             int sample_rate,
                             size_t bytes_per_sample,
                             size_t num_samples,
                             uint8_t* buf,
                             size_t* header_size) {
  RTC_CHECK(buf);
  RTC_CHECK(header_size);
  *header_size = kIeeeFloatWavHeaderSize;
  auto header = rtc::MsanUninitialized<WavHeaderIeeeFloat>({});
  const size_t bytes_in_payload = bytes_per_sample * num_samples;

  header.riff.header.ID = PackFourCC('R', 'I', 'F', 'F');
  header.riff.header.Size = RiffChunkSize(bytes_in_payload, *header_size);
  header.riff.Format = PackFourCC('W', 'A', 'V', 'E');
  header.fmt.header.ID = PackFourCC('f', 'm', 't', ' ');
  header.fmt.header.Size = kFmtIeeeFloatSubchunkSize;
  header.fmt.AudioFormat =
      MapWavFormatToHeaderField(WavFormat::kWavFormatIeeeFloat);
  header.fmt.NumChannels = static_cast<NumChannelsType>(num_channels);
  header.fmt.SampleRate = sample_rate;
  header.fmt.ByteRate = ByteRate(num_channels, sample_rate, bytes_per_sample);
  header.fmt.BlockAlign = BlockAlign(num_channels, bytes_per_sample);
  header.fmt.BitsPerSample =
      static_cast<BitsPerSampleType>(8 * bytes_per_sample);
  header.fmt.ExtensionSize = 0;
  header.fact.header.ID = PackFourCC('f', 'a', 'c', 't');
  header.fact.header.Size = 4;
  header.fact.SampleLength = static_cast<uint32_t>(num_channels * num_samples);
  header.data.header.ID = PackFourCC('d', 'a', 't', 'a');
  header.data.header.Size = static_cast<uint32_t>(bytes_in_payload);

  // Do an extra copy rather than writing everything to buf directly, since buf
  // might not be correctly aligned.
  memcpy(buf, &header, *header_size);
}

// Returns the number of bytes per sample for the format.
size_t GetFormatBytesPerSample(WavFormat format) {
  switch (format) {
    case WavFormat::kWavFormatPcm:
      // Other values may be OK, but for now we're conservative.
      return 2;
    case WavFormat::kWavFormatALaw:
    case WavFormat::kWavFormatMuLaw:
      return 1;
    case WavFormat::kWavFormatIeeeFloat:
      return 4;
  }
  RTC_CHECK_NOTREACHED();
}

void CheckWavParameters(size_t num_channels,
                        int sample_rate,
                        WavFormat format,
                        size_t bytes_per_sample,
                        size_t num_samples) {
  // num_channels, sample_rate, and bytes_per_sample must be positive, must fit
  // in their respective fields, and their product must fit in the 32-bit
  // ByteRate field.
  RTC_CHECK_GT(num_channels, 0) << "Number of channels cannot be zero";
  RTC_CHECK_GT(sample_rate, 0) << "Sample rate must be positive";
  RTC_CHECK_GT(bytes_per_sample, 0) << "Bytes per sample cannot be zero";

  RTC_CHECK_EQ(sample_rate & std::numeric_limits<SampleRateType>::max(),
               sample_rate)
      << "Sample rate too large to represent";
  RTC_CHECK_EQ(num_channels & std::numeric_limits<NumChannelsType>::max(),
               num_channels)
      << "Number of channels too large to represent";

  constexpr BitsPerSampleType kBytesPerSampleMax =
      std::numeric_limits<BitsPerSampleType>::max() / 8;
  RTC_CHECK_EQ(bytes_per_sample & kBytesPerSampleMax, bytes_per_sample)
      << "Bytes per sample too large to represent";

  const uint64_t byte_rate =
      static_cast<uint64_t>(sample_rate) * num_channels * bytes_per_sample;
  // It is actually the byte rate rather than bit rate that is stored, but the
  // error message uses bit rate which is probably better recognized.
  RTC_CHECK_EQ(byte_rate & std::numeric_limits<ByteRateType>::max(), byte_rate)
      << "Bit rate too large to represent";

  // format and bytes_per_sample must agree.
  switch (format) {
    case WavFormat::kWavFormatPcm:
      // Other values may be OK, but for now we're conservative:
      RTC_CHECK(bytes_per_sample == 1 || bytes_per_sample == 2)
          << "Unsupported bytes per sample for PCM format (expected 1 or 2): "
          << bytes_per_sample;
      break;
    case WavFormat::kWavFormatALaw:
      RTC_CHECK_EQ(bytes_per_sample, 1)
          << "Wrong bytes per sample for A law format";
      break;
    case WavFormat::kWavFormatMuLaw:
      RTC_CHECK_EQ(bytes_per_sample, 1)
          << "Wrong bytes per sample for Mu law format";
      break;
    case WavFormat::kWavFormatIeeeFloat:
      RTC_CHECK_EQ(bytes_per_sample, 4)
          << "Wrong bytes per sample for IEEE float format";
      break;
    default:
      RTC_CHECK_NOTREACHED();
  }

  // The number of bytes in the file, not counting the first ChunkHeader, must
  // be less than 2^32; otherwise, the ChunkSize field overflows.
  const uint64_t header_size = kPcmWavHeaderSize - sizeof(ChunkHeader);
  const uint64_t max_samples =
      (std::numeric_limits<uint32_t>::max() - header_size) / bytes_per_sample;
  RTC_CHECK_LE(num_samples, max_samples) << "File too large";

  // Each channel must have the same number of samples.
  RTC_CHECK_EQ(num_samples % num_channels, 0)
      << "Channels have different numbers of samples";
}

}  // namespace

void CheckWavParameters(size_t num_channels,
                        int sample_rate,
                        WavFormat format,
                        size_t num_samples) {
  CheckWavParameters(num_channels, sample_rate, format,
                     GetFormatBytesPerSample(format), num_samples);
}

void WriteWavHeader(size_t num_channels,
                    int sample_rate,
                    WavFormat format,
                    size_t num_samples,
                    uint8_t* buf,
                    size_t* header_size) {
  RTC_CHECK(buf);
  RTC_CHECK(header_size);

  const size_t bytes_per_sample = GetFormatBytesPerSample(format);
  CheckWavParameters(num_channels, sample_rate, format, bytes_per_sample,
                     num_samples);
  if (format == WavFormat::kWavFormatPcm) {
    WritePcmWavHeader(num_channels, sample_rate, bytes_per_sample, num_samples,
                      buf, header_size);
  } else {
    RTC_CHECK_EQ(format, WavFormat::kWavFormatIeeeFloat);
    WriteIeeeFloatWavHeader(num_channels, sample_rate, bytes_per_sample,
                            num_samples, buf, header_size);
  }
}

void ReadWavHeader(WavHeaderReader* readable,
                   size_t* num_channels,
                   int* sample_rate,
                   WavFormat* format,
                   size_t* bytes_per_sample,
                   size_t* num_samples,
                   int64_t* data_start_pos) {
  // Read using the PCM header, even though it might be float Wav file
  auto header = rtc::MsanUninitialized<WavHeaderPcm>({});

  // Read RIFF chunk.
  const size_t bytes_read = readable->Read(&header.riff, sizeof(header.riff));
  RTC_CHECK_EQ(bytes_read, sizeof(header.riff)) << "Invalid 'RIFF' chunk";

  RTC_CHECK_EQ(ReadFourCC(header.riff.header.ID), "RIFF")
      << "Expected 'RIFF' chunk";
  RTC_CHECK_EQ(ReadFourCC(header.riff.Format), "WAVE")
      << "Wrong form type in 'RIFF' chunk";

  // Find "fmt " and "data" chunks. While the official Wave file specification
  // does not put requirements on the chunks order, it is uncommon to find the
  // "data" chunk before the "fmt " one. The code below fails if this is not the
  // case.
  FindWaveChunk(&header.fmt.header, readable, "fmt ");
  ReadFmtChunkData(&header.fmt, readable);
  FindWaveChunk(&header.data.header, readable, "data");

  // Parse needed fields.
  *format = MapHeaderFieldToWavFormat(header.fmt.AudioFormat);
  *num_channels = header.fmt.NumChannels;
  *sample_rate = header.fmt.SampleRate;
  *bytes_per_sample = header.fmt.BitsPerSample / 8;
  const size_t bytes_in_payload = header.data.header.Size;
  RTC_CHECK_NE(*bytes_per_sample, 0)
      << "Invalid 'fmt ' chunk: bytes per sample set to zero";
  *num_samples = bytes_in_payload / *bytes_per_sample;

  const size_t header_size = *format == WavFormat::kWavFormatPcm
                                 ? kPcmWavHeaderSize
                                 : kIeeeFloatWavHeaderSize;

  // Header may report larger size than expected due to the extension field.
  RTC_CHECK_GE(header.riff.header.Size,
               RiffChunkSize(bytes_in_payload, header_size))
      << "'RIFF' chunk reports smaller file size than expected";
  RTC_CHECK_EQ(header.fmt.ByteRate,
               ByteRate(*num_channels, *sample_rate, *bytes_per_sample))
      << "Invalid 'fmt ' chunk: unexpected byte rate";
  RTC_CHECK_EQ(header.fmt.BlockAlign,
               BlockAlign(*num_channels, *bytes_per_sample))
      << "Invalid 'fmt ' chunk: unexpected block alignment";

  CheckWavParameters(*num_channels, *sample_rate, *format, *bytes_per_sample,
                     *num_samples);

  *data_start_pos = readable->GetPosition();
}

}  // namespace webrtc
