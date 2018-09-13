/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/delta_encoding.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/bitbuffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace {

size_t BitsToBytes(size_t bits) {
  return (bits / 8) + (bits % 8 > 0 ? 1 : 0);
}

// TODO(eladalon): Replace by something more efficient.
uint64_t BitWidth(uint64_t input) {
  uint64_t width = 0;
  do {  // input == 0 -> width == 1
    width += 1;
    input >>= 1;
  } while (input != 0);
  return width;
}

uint64_t MaxSignedDeltaBitWidth(const std::vector<uint64_t>& inputs,
                                uint64_t original_width_bits) {
  // This prevent the use of signed deltas, by always assuming
  // they will not provide value over unsigned.
  // TODO(eladalon): Add support for signed deltas.
  return 64;
}

// TODO: !!! Difference between uint64_t and size_t throughout file.

// Return the maximum integer of a given bit width.
// Examples:
// MaxValueOfBitWidth(1) = 0x01
// MaxValueOfBitWidth(6) = 0x3f
// MaxValueOfBitWidth(8) = 0xff
// MaxValueOfBitWidth(32) = 0xffffffff
uint64_t MaxValueOfBitWidth(size_t bit_width) {
  RTC_DCHECK_GE(bit_width, 1);
  RTC_DCHECK_LE(bit_width, 64);
  return (bit_width == 64) ? std::numeric_limits<uint64_t>::max()
                           : ((1 << bit_width) - 1);
}

// Computes the delta between |previous| and |current|, under the assumption
// that wrap-around occurs after width |bit_width| is exceeded.
uint64_t ComputeDelta(uint64_t previous, uint64_t current, uint64_t width) {
  RTC_DCHECK(width == 64 || current < (1 << width));
  RTC_DCHECK(width == 64 || previous < (1 << width));

  if (current >= previous) {
    // Simply "walk" forward.
    return current - previous;
  } else {  // previous > current
    // "Walk" until the max value, one more step to 0, then to |current|.
    return (MaxValueOfBitWidth(width) - previous) + 1 + current;
  }
}

// TODO: !!! Explain.
enum class EncodingType {
  kFixedSizeWithOnlyMandatoryFields = 0,
  kFixedSizeWithAllOptionalFields = 1,  // TODO: !!! Name
  kReserved1 = 2,
  kReserved2 = 3,
  kNumberOfEncodingTypes  // Keep last
};

// The width of each field in the encoding header. Note that this is the
// width in case the field exists; not all fields occur in all encoding types.
constexpr size_t kBitsInHeaderForEncodingType = 2;
constexpr size_t kBitsInHeaderForOriginalWidthBits = 6;
constexpr size_t kBitsInHeaderForDeltaWidthBits = 6;
constexpr size_t kBitsInHeaderForSignedDeltas = 1;
constexpr size_t kBitsInHeaderForValuesOptional = 1;

// Default values for when the encoding header does not specify explicitly.
constexpr uint64_t kDefaultOriginalWidthBits = 64;
constexpr bool kDefaultSignedDeltas = false;
constexpr bool kDefaultValuesOptional = false;

static_assert(static_cast<size_t>(EncodingType::kNumberOfEncodingTypes) <=
                  1 << kBitsInHeaderForEncodingType,
              "Not all encoding types fit.");

// Wrap BitBufferWriter and extend its functionality by (1) keeping track of
//  the number of bits written and (2) owning its buffer.
class BitWriter final {
 public:
  BitWriter(size_t byte_count)
      : buffer_(byte_count, '\0'),
        bit_writer_(reinterpret_cast<uint8_t*>(&buffer_[0]), buffer_.size()),
        written_bits_(0),
        valid_(true) {
    RTC_DCHECK_GT(byte_count, 0);
  }

  // TODO: !!! Make sure the allocation never messes up.
  void WriteBits(uint64_t val, size_t bit_count) {
    RTC_DCHECK(valid_);
    const bool success = bit_writer_.WriteBits(val, bit_count);
    RTC_DCHECK(success);
    written_bits_ += bit_count;
  }

  std::string GetString() {
    RTC_DCHECK(valid_);
    valid_ = false;

    buffer_.resize(BitsToBytes(written_bits_));

    std::string result;
    std::swap(buffer_, result);
    return result;
  }

 private:
  std::string buffer_;
  rtc::BitBufferWriter bit_writer_;
  // Note: Counting bits instead of bytes wrap around earlier than it has to,
  // which means the maximum length is lower than it must be. We don't expect
  // to go anywhere near the limit, though, so this is good enough.
  size_t written_bits_;
  bool valid_;

  RTC_DISALLOW_COPY_AND_ASSIGN(BitWriter);
};

// Performs delta-encoding of a single (non-empty) sequence of values, using
// an encoding where all deltas are encoded using the same number of bits.
// (With the exception of optional values, which are encoded using one of two
// fixed numbers of bits.)
class FixedLengthDeltaEncoder final {
 public:
  // See webrtc::EncodeDeltas() for general details.
  // This function must write into |output| a bit pattern that would allow the
  // decoder to determine whether it was produced by FixedLengthDeltaEncoder,
  // and can therefore be decoded by FixedLengthDeltaDecoder, or whether it
  // was produced by a different encoder.
  static std::string EncodeDeltas(uint64_t base,
                                  const std::vector<uint64_t>& values);

 private:
  // FixedLengthDeltaEncoder objects are to be created by EncodeDeltas() and
  // released by it before it returns. They're mostly a convenient way to
  // avoid having to pass a lot of state between different functions.
  // Therefore, it was deemed acceptable to let them have a reference to
  // |values|, whose lifetime must exceed the lifetime of |this|.
  FixedLengthDeltaEncoder(uint64_t original_width_bits,
                          uint64_t delta_width_bits,
                          bool signed_deltas,
                          bool values_optional,
                          uint64_t base,
                          const std::vector<uint64_t>& values);

  // Perform delta-encoding using the parameters given to the ctor on the
  // sequence of values given to the ctor.
  std::string Encode();

  // Lower bounds on the output length. May be exceeded by arbitrary margins.
  size_t LowerBoundOutputLengthBytes(uint64_t num_of_deltas) const;
  size_t LowerBoundHeaderLengthBits() const;
  size_t LowerBoundEncodedDeltasLengthBits(uint64_t num_of_deltas) const;

  // Encode the compression parameters into the stream.
  void EncodeHeader();

  // Encode a given delta into the stream.
  void EncodeDelta(uint64_t previous, uint64_t current);

  // Number of bits necessary to hold the largest value in the sequence of
  // values that |this| will be used to encode.
  const uint64_t original_width_bits_;

  // Number of bits necessary to hold the widest(*) of the deltas between the
  // values |this| will be used to encode.
  // (*) - Widest might not be the largest, if signed deltas are used.
  const uint64_t delta_width_bits_;

  // Whether deltas are signed.
  // TODO(eladalon): Add support for signed deltas.
  const bool signed_deltas_;

  // Whether the values encoded by |this| are optional. That is, it may be
  // that some of them might have to be non-existent rather than assume
  // a value. (Do not confuse value 0 with non-existence; the two are distinct).
  // TODO(eladalon): Add support for optional elements.
  const bool values_optional_;  // TODO: !!! Rename.

  // The encoding scheme assumes that at least one value is transmitted OOB,
  // so that the first value can be encoded as a delta from that OOB value,
  // which is |base_|.
  const uint64_t base_;

  // The values to be encoded.
  // Note: This is a non-owning reference. See comment above ctor for details.
  const std::vector<uint64_t>& values_;

  // Buffer into which encoded values will be written.
  // This is created dynmically as a way to enforce that the rest of the
  // ctor has finished running when this is constructed, so that the lower
  // bound on the buffer size would be guaranteed correct.
  std::unique_ptr<BitWriter> writer_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FixedLengthDeltaEncoder);
};

std::string FixedLengthDeltaEncoder::EncodeDeltas(
    uint64_t base,
    const std::vector<uint64_t>& values) {
  RTC_DCHECK(!values.empty());

  const uint64_t original_width_bits =
      std::max(BitWidth(base),
               BitWidth(*std::max_element(values.begin(), values.end())));

  std::vector<uint64_t> deltas(values.size());
  deltas[0] = ComputeDelta(base, values[0], original_width_bits);
  uint64_t max_delta = deltas[0];
  for (size_t i = 1; i < deltas.size(); ++i) {
    deltas[i] = ComputeDelta(values[i - 1], values[i], original_width_bits);
    max_delta = std::max(deltas[i], max_delta);
  }

  // We indicate the special case of all values being equal to the base with
  // the empty string.
  if (max_delta == 0) {
    RTC_DCHECK(std::all_of(values.cbegin(), values.cend(),
                           [base](uint64_t val) { return val == base; }));
    return std::string();
  }

  const uint64_t delta_width_bits_unsigned = BitWidth(max_delta);
  const uint64_t delta_width_bits_signed =
      MaxSignedDeltaBitWidth(deltas, original_width_bits);

  // Note: Preference for unsigned if the two have the same width (efficiency).
  const bool signed_deltas =
      delta_width_bits_signed < delta_width_bits_unsigned;
  const uint64_t delta_width_bits =
      signed_deltas ? delta_width_bits_signed : delta_width_bits_unsigned;

  const bool values_optional = false;

  FixedLengthDeltaEncoder encoder(original_width_bits, delta_width_bits,
                                  signed_deltas, values_optional, base, values);
  return encoder.Encode();
}

FixedLengthDeltaEncoder::FixedLengthDeltaEncoder(
    uint64_t original_width_bits,
    uint64_t delta_width_bits,
    bool signed_deltas,
    bool values_optional,
    uint64_t base,
    const std::vector<uint64_t>& values)
    : original_width_bits_(original_width_bits),
      delta_width_bits_(delta_width_bits),
      signed_deltas_(signed_deltas),
      values_optional_(values_optional),
      base_(base),
      values_(values) {
  RTC_DCHECK_GE(delta_width_bits_, 1);
  RTC_DCHECK_LE(delta_width_bits_, 64);
  RTC_DCHECK_GE(original_width_bits_, 1);
  RTC_DCHECK_LE(original_width_bits_, 64);
  RTC_DCHECK_LE(delta_width_bits_, original_width_bits_);
  RTC_DCHECK(!values_.empty());

  writer_ =
      absl::make_unique<BitWriter>(LowerBoundOutputLengthBytes(values_.size()));
}

std::string FixedLengthDeltaEncoder::Encode() {
  EncodeHeader();

  uint64_t previous = base_;
  for (uint64_t value : values_) {
    EncodeDelta(previous, value);
    previous = value;
  }

  return writer_->GetString();
}

// TODO: !!! It might be that the deltas are even smaller than the values.
size_t FixedLengthDeltaEncoder::LowerBoundOutputLengthBytes(
    uint64_t num_of_deltas) const {
  const size_t length_bits = LowerBoundHeaderLengthBits() +
                             LowerBoundEncodedDeltasLengthBits(num_of_deltas);
  return BitsToBytes(length_bits);
}

size_t FixedLengthDeltaEncoder::LowerBoundHeaderLengthBits() const {
  return kBitsInHeaderForEncodingType + kBitsInHeaderForOriginalWidthBits +
         kBitsInHeaderForDeltaWidthBits + kBitsInHeaderForSignedDeltas +
         kBitsInHeaderForValuesOptional;
}

size_t FixedLengthDeltaEncoder::LowerBoundEncodedDeltasLengthBits(
    uint64_t num_of_deltas) const {
  return num_of_deltas * (delta_width_bits_ + values_optional_);
}

void FixedLengthDeltaEncoder::EncodeHeader() {
  RTC_DCHECK(writer_);
  // Note: Since it's meaningless for a field to be of width 0, we encode
  // width == 1 as 0, width == 2 as 1, etc.
  // TODO: !!! Encoding type...?
  writer_->WriteBits(
      static_cast<uint64_t>(EncodingType::kFixedSizeWithAllOptionalFields),
      kBitsInHeaderForEncodingType);
  writer_->WriteBits(original_width_bits_ - 1,
                     kBitsInHeaderForOriginalWidthBits);
  writer_->WriteBits(delta_width_bits_ - 1, kBitsInHeaderForDeltaWidthBits);
  writer_->WriteBits(static_cast<uint64_t>(signed_deltas_),
                     kBitsInHeaderForSignedDeltas);
  writer_->WriteBits(static_cast<uint64_t>(values_optional_),
                     kBitsInHeaderForValuesOptional);
}

void FixedLengthDeltaEncoder::EncodeDelta(uint64_t previous, uint64_t current) {
  RTC_DCHECK(writer_);
  writer_->WriteBits(ComputeDelta(previous, current, original_width_bits_),
                     delta_width_bits_);
}

class FixedLengthDeltaDecoder final {
 public:
  // Checks whether FixedLengthDeltaDecoder is a suitable decoder for this
  // bitstream. Note that this does not necessarily mean that the stream is
  // not defective; decoding might still fail later.
  static bool IsSuitableDecoderFor(const std::string& input);

  // TODO: !!!
  static std::vector<uint64_t> DecodeDeltas(const std::string& input,
                                            uint64_t base,
                                            size_t num_of_deltas);

 private:
  // TODO: !!!
  static std::unique_ptr<FixedLengthDeltaDecoder>
  Create(const std::string& input, uint64_t base, size_t num_of_deltas);

  static bool ParseWithOnlyMandatoryFields(rtc::BitBuffer* reader,
                                           uint64_t* original_width_bits,
                                           uint64_t* delta_width_bits,
                                           bool* signed_deltas,
                                           bool* values_optional);

  static bool ParseWithAllOptionalFields(rtc::BitBuffer* reader,
                                         uint64_t* original_width_bits,
                                         uint64_t* delta_width_bits,
                                         bool* signed_deltas,
                                         bool* values_optional);

  FixedLengthDeltaDecoder(std::unique_ptr<rtc::BitBuffer> reader,
                          uint64_t original_width_bits,
                          uint64_t delta_width_bits,
                          bool signed_deltas,
                          bool values_optional,
                          uint64_t base,
                          size_t num_of_deltas);

  std::vector<uint64_t> Decode();

  bool GetDelta(uint64_t* delta);

  uint64_t ApplyDelta(uint64_t base, uint64_t delta) const;

  const std::unique_ptr<rtc::BitBuffer> reader_;

  const uint64_t original_width_bits_;

  const uint64_t delta_width_bits_;

  const bool signed_deltas_;
  const bool values_optional_;

  const uint64_t base_;
  const size_t num_of_deltas_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FixedLengthDeltaDecoder);
};

bool FixedLengthDeltaDecoder::IsSuitableDecoderFor(const std::string& input) {
  if (input.length() < kBitsInHeaderForEncodingType) {
    return false;
  }

  rtc::BitBuffer reader(reinterpret_cast<const uint8_t*>(&input[0]),
                        kBitsInHeaderForEncodingType);

  uint32_t encoding_type_bits;
  const bool result =
      reader.ReadBits(&encoding_type_bits, kBitsInHeaderForEncodingType);
  RTC_DCHECK(result);

  const auto encoding_type = static_cast<EncodingType>(encoding_type_bits);
  return encoding_type == EncodingType::kFixedSizeWithOnlyMandatoryFields ||
         encoding_type == EncodingType::kFixedSizeWithAllOptionalFields;
}

std::vector<uint64_t> FixedLengthDeltaDecoder::DecodeDeltas(
    const std::string& input,
    uint64_t base,
    size_t num_of_deltas) {
  auto decoder = FixedLengthDeltaDecoder::Create(input, base, num_of_deltas);
  if (!decoder) {
    return std::vector<uint64_t>();
  }

  return decoder->Decode();
}

std::unique_ptr<FixedLengthDeltaDecoder> FixedLengthDeltaDecoder::Create(
    const std::string& input,
    uint64_t base,
    size_t num_of_deltas) {
  if (input.length() < kBitsInHeaderForEncodingType) {
    return nullptr;
  }

  auto reader = absl::make_unique<rtc::BitBuffer>(
      reinterpret_cast<const uint8_t*>(&input[0]), input.length());

  uint32_t encoding_type_bits;
  const bool result =
      reader->ReadBits(&encoding_type_bits, kBitsInHeaderForEncodingType);
  RTC_DCHECK(result);

  uint64_t original_width_bits;
  uint64_t delta_width_bits;
  bool signed_deltas;
  bool values_optional;

  bool encoding_type_parsed = false;
  switch (static_cast<EncodingType>(encoding_type_bits)) {
    case EncodingType::kFixedSizeWithOnlyMandatoryFields:
      if (!ParseWithOnlyMandatoryFields(reader.get(), &original_width_bits,
                                        &delta_width_bits, &signed_deltas,
                                        &values_optional)) {
        return nullptr;
      }
      encoding_type_parsed = true;
      break;
    case EncodingType::kFixedSizeWithAllOptionalFields:
      if (!ParseWithAllOptionalFields(reader.get(), &original_width_bits,
                                      &delta_width_bits, &signed_deltas,
                                      &values_optional)) {
        return nullptr;
      }
      encoding_type_parsed = true;
      break;
    case EncodingType::kReserved1:
    case EncodingType::kReserved2:
    case EncodingType::kNumberOfEncodingTypes:
      return nullptr;
  }
  if (!encoding_type_parsed) {
    RTC_LOG(LS_WARNING) << "Unrecognized encoding type.";
    return nullptr;
  }

  return absl::WrapUnique(new FixedLengthDeltaDecoder(
      std::move(reader), original_width_bits, delta_width_bits, signed_deltas,
      values_optional, base, num_of_deltas));
}

bool FixedLengthDeltaDecoder::ParseWithOnlyMandatoryFields(
    rtc::BitBuffer* reader,
    uint64_t* original_width_bits,
    uint64_t* delta_width_bits,
    bool* signed_deltas,
    bool* values_optional) {
  RTC_DCHECK(reader);
  RTC_DCHECK(original_width_bits);
  RTC_DCHECK(delta_width_bits);
  RTC_DCHECK(signed_deltas);
  RTC_DCHECK(values_optional);

  uint32_t read_buffer;
  if (!reader->ReadBits(&read_buffer, kBitsInHeaderForDeltaWidthBits)) {
    return false;
  }
  RTC_DCHECK_LE(read_buffer, 64 - 1);   // See encoding for -1's rationale.
  *delta_width_bits = read_buffer + 1;  // See encoding for +1's rationale.

  *original_width_bits = kDefaultOriginalWidthBits;
  *signed_deltas = kDefaultSignedDeltas;
  *values_optional = kDefaultValuesOptional;

  return true;
}

bool FixedLengthDeltaDecoder::ParseWithAllOptionalFields(
    rtc::BitBuffer* reader,
    uint64_t* original_width_bits,
    uint64_t* delta_width_bits,
    bool* signed_deltas,
    bool* values_optional) {
  RTC_DCHECK(reader);
  RTC_DCHECK(original_width_bits);
  RTC_DCHECK(delta_width_bits);
  RTC_DCHECK(signed_deltas);
  RTC_DCHECK(values_optional);

  uint32_t read_buffer;

  // Original width
  if (!reader->ReadBits(&read_buffer, kBitsInHeaderForOriginalWidthBits)) {
    return false;
  }
  RTC_DCHECK_LE(read_buffer, 64 - 1);      // See encoding for -1's rationale.
  *original_width_bits = read_buffer + 1;  // See encoding for +1's rationale.

  // Delta width
  if (!reader->ReadBits(&read_buffer, kBitsInHeaderForDeltaWidthBits)) {
    return false;
  }
  RTC_DCHECK_LE(read_buffer, 64 - 1);   // See encoding for -1's rationale.
  *delta_width_bits = read_buffer + 1;  // See encoding for +1's rationale.

  // Signed deltas
  if (!reader->ReadBits(&read_buffer, kBitsInHeaderForSignedDeltas)) {
    return false;
  }
  *signed_deltas = rtc::dchecked_cast<bool>(read_buffer);

  // Optional values
  if (!reader->ReadBits(&read_buffer, kBitsInHeaderForValuesOptional)) {
    return false;
  }
  RTC_DCHECK_LE(read_buffer, 1);
  *values_optional = rtc::dchecked_cast<bool>(read_buffer);

  return true;
}

FixedLengthDeltaDecoder::FixedLengthDeltaDecoder(
    std::unique_ptr<rtc::BitBuffer> reader,
    uint64_t original_width_bits,
    uint64_t delta_width_bits,
    bool signed_deltas,
    bool values_optional,
    uint64_t base,
    size_t num_of_deltas)
    : reader_(std::move(reader)),
      original_width_bits_(original_width_bits),
      delta_width_bits_(delta_width_bits),
      signed_deltas_(signed_deltas),
      values_optional_(values_optional),
      base_(base),
      num_of_deltas_(num_of_deltas) {
  RTC_DCHECK(reader_);
  // TODO(eladalon): Support signed deltas.
  RTC_DCHECK(!signed_deltas_) << "Not implemented.";
  // TODO(eladalon): Support optional values.
  RTC_DCHECK(!values_optional_) << "Not implemented.";
}

std::vector<uint64_t> FixedLengthDeltaDecoder::Decode() {
  std::vector<uint64_t> values(num_of_deltas_);

  uint64_t previous = base_;
  for (size_t i = 0; i < num_of_deltas_; ++i) {
    uint64_t delta;
    if (!GetDelta(&delta)) {
      return std::vector<uint64_t>();
    }
    values[i] = ApplyDelta(previous, delta);
    previous = values[i];
  }

  return values;
}

// TODO: !!! Invalidate on first error.
// TODO: !!! Optionals, signed, etc.
bool FixedLengthDeltaDecoder::GetDelta(uint64_t* delta) {
  RTC_DCHECK(reader_);

  // BitBuffer and BitBufferWriter read/write higher bits before lower bits.

  const size_t lower_bit_count = std::min<uint64_t>(delta_width_bits_, 32u);
  const size_t higher_bit_count =
      (delta_width_bits_ <= 32u) ? 0 : delta_width_bits_ - 32u;

  uint32_t lower_bits;
  uint32_t higher_bits;

  if (higher_bit_count > 0) {
    if (!reader_->ReadBits(&higher_bits, higher_bit_count)) {
      RTC_LOG(LS_WARNING) << "Failed to read higher half of delta.";
      return false;
    }
  } else {
    higher_bits = 0;
  }

  if (!reader_->ReadBits(&lower_bits, lower_bit_count)) {
    RTC_LOG(LS_WARNING) << "Failed to read lower half of delta.";
    return false;
  }

  const uint64_t lower_bits_64 = static_cast<uint64_t>(lower_bits);
  const uint64_t higher_bits_64 = static_cast<uint64_t>(higher_bits);

  *delta = (higher_bits_64 << 32) | lower_bits_64;
  return true;
}

uint64_t FixedLengthDeltaDecoder::ApplyDelta(uint64_t base,
                                             uint64_t delta) const {
  RTC_DCHECK_LE(base, MaxValueOfBitWidth(original_width_bits_));
  RTC_DCHECK_LE(delta, MaxValueOfBitWidth(delta_width_bits_));

  RTC_DCHECK(!signed_deltas_) << "Not implemented.";
  RTC_DCHECK(!values_optional_) << "Not implemented.";

  RTC_DCHECK_LE(delta_width_bits_, original_width_bits_);  // Reminder.
  uint64_t result = base + delta;
  if (original_width_bits_ < 64) {  // Naturally wraps around otherwise.
    result %= (1 << original_width_bits_);
  }
  return result;
}

}  // namespace

std::string EncodeDeltas(uint64_t base, const std::vector<uint64_t>& values) {
  // TODO(eladalon): Support additional encodings.
  return FixedLengthDeltaEncoder::EncodeDeltas(base, values);
}

std::vector<uint64_t> DecodeDeltas(const std::string& input,
                                   uint64_t base,
                                   size_t num_of_deltas) {
  RTC_DCHECK_GT(num_of_deltas, 0);  // Allows empty vector to indicate error.

  // The empty string is a special case indicating that all values were equal
  // to the base.
  if (input.empty()) {
    std::vector<uint64_t> result(num_of_deltas);
    std::fill(result.begin(), result.end(), base);
    return result;
  }

  if (FixedLengthDeltaDecoder::IsSuitableDecoderFor(input)) {
    return FixedLengthDeltaDecoder::DecodeDeltas(input, base, num_of_deltas);
  }

  RTC_LOG(LS_WARNING) << "Could not decode delta-encoded stream.";
  return std::vector<uint64_t>();
}

}  // namespace webrtc
