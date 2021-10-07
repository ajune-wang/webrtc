/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packet.h"

#include <cstdint>
#include <cstring>
#include <utility>

#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {
constexpr size_t kFixedHeaderSize = 12;
constexpr uint8_t kRtpVersion = 2;
constexpr uint16_t kOneByteExtensionProfileId = 0xBEDE;
constexpr uint16_t kTwoByteExtensionProfileId = 0x1000;
constexpr uint16_t kTwobyteExtensionProfileIdAppBitsFilter = 0xfff0;
constexpr size_t kOneByteExtensionHeaderLength = 1;
constexpr size_t kTwoByteExtensionHeaderLength = 2;
constexpr size_t kDefaultPacketSize = 1500;
}  // namespace

//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|X|  CC   |M|     PT      |       sequence number         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           timestamp                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           synchronization source (SSRC) identifier            |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |            Contributing source (CSRC) identifiers             |
// |                             ....                              |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |  header eXtension profile id  |       length in 32bits        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                          Extensions                           |
// |                             ....                              |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |                           Payload                             |
// |             ....              :  padding...                   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |               padding         | Padding size  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
RtpPacket::RtpPacket() : RtpPacket(nullptr, kDefaultPacketSize) {}

RtpPacket::RtpPacket(const ExtensionManager* extensions)
    : RtpPacket(extensions, kDefaultPacketSize) {}

RtpPacket::RtpPacket(const RtpPacket&) = default;

RtpPacket::RtpPacket(const ExtensionManager* extensions, size_t capacity)
    : buffer_(capacity) {
  RTC_DCHECK_GE(capacity, kFixedHeaderSize);
  Clear();
  if (extensions != nullptr) {
    allow_create_two_byte_header_extension_ = extensions->ExtmapAllowMixed();
    extensions->ListRegisteredExtensions([this](int id, absl::string_view uri) {
      ExtensionInfo entry;
      entry.id = id;
      entry.type = uri.data();
      extension_entries_.push_back(entry);
    });
  }
}

RtpPacket::~RtpPacket() = default;

void RtpPacket::IdentifyExtensions(const RtpHeaderExtensionMap& extensions) {
  allow_create_two_byte_header_extension_ = extensions.ExtmapAllowMixed();
  for (auto& entry : extension_entries_) {
    entry.type = nullptr;
  }
  extensions.ListRegisteredExtensions([this](int id, absl::string_view uri) {
    for (auto& entry : extension_entries_) {
      if (entry.id == id) {
        entry.type = uri.data();
        return;
      }
    }
    ExtensionInfo entry;
    entry.id = id;
    entry.type = uri.data();
    extension_entries_.push_back(entry);
  });
}

bool RtpPacket::Parse(const uint8_t* buffer, size_t buffer_size) {
  if (!ParseBuffer(buffer, buffer_size)) {
    Clear();
    return false;
  }
  buffer_.SetData(buffer, buffer_size);
  RTC_DCHECK_EQ(size(), buffer_size);
  return true;
}

bool RtpPacket::Parse(rtc::ArrayView<const uint8_t> packet) {
  return Parse(packet.data(), packet.size());
}

bool RtpPacket::Parse(rtc::CopyOnWriteBuffer buffer) {
  if (!ParseBuffer(buffer.cdata(), buffer.size())) {
    Clear();
    return false;
  }
  size_t buffer_size = buffer.size();
  buffer_ = std::move(buffer);
  RTC_DCHECK_EQ(size(), buffer_size);
  return true;
}

std::vector<uint32_t> RtpPacket::Csrcs() const {
  size_t num_csrc = data()[0] & 0x0F;
  RTC_DCHECK_GE(capacity(), kFixedHeaderSize + num_csrc * 4);
  std::vector<uint32_t> csrcs(num_csrc);
  for (size_t i = 0; i < num_csrc; ++i) {
    csrcs[i] =
        ByteReader<uint32_t>::ReadBigEndian(&data()[kFixedHeaderSize + i * 4]);
  }
  return csrcs;
}

void RtpPacket::CopyHeaderFrom(const RtpPacket& packet) {
  marker_ = packet.marker_;
  payload_type_ = packet.payload_type_;
  sequence_number_ = packet.sequence_number_;
  timestamp_ = packet.timestamp_;
  ssrc_ = packet.ssrc_;
  payload_offset_ = packet.payload_offset_;
  extension_entries_ = packet.extension_entries_;
  extensions_size_ = packet.extensions_size_;
  buffer_ = packet.buffer_.Slice(0, packet.headers_size());
  // Reset payload and padding.
  payload_size_ = 0;
  padding_size_ = 0;
}

void RtpPacket::SetMarker(bool marker_bit) {
  marker_ = marker_bit;
  if (marker_) {
    WriteAt(1, data()[1] | 0x80);
  } else {
    WriteAt(1, data()[1] & 0x7F);
  }
}

void RtpPacket::SetPayloadType(uint8_t payload_type) {
  RTC_DCHECK_LE(payload_type, 0x7Fu);
  payload_type_ = payload_type;
  WriteAt(1, (data()[1] & 0x80) | payload_type);
}

void RtpPacket::SetSequenceNumber(uint16_t seq_no) {
  sequence_number_ = seq_no;
  ByteWriter<uint16_t>::WriteBigEndian(WriteAt(2), seq_no);
}

void RtpPacket::SetTimestamp(uint32_t timestamp) {
  timestamp_ = timestamp;
  ByteWriter<uint32_t>::WriteBigEndian(WriteAt(4), timestamp);
}

void RtpPacket::SetSsrc(uint32_t ssrc) {
  ssrc_ = ssrc;
  ByteWriter<uint32_t>::WriteBigEndian(WriteAt(8), ssrc);
}

void RtpPacket::ZeroMutableExtensions() {
  for (const ExtensionInfo& extension : extension_entries_) {
    if (extension.type == ExtensionType<VideoTimingExtension>()) {
      // Nullify last entries, starting at pacer delay.
      // These are set by pacer and SFUs
      if (VideoTimingExtension::kPacerExitDeltaOffset < extension.length) {
        memset(WriteAt(extension.offset +
                       VideoTimingExtension::kPacerExitDeltaOffset),
               0,
               extension.length - VideoTimingExtension::kPacerExitDeltaOffset);
      }
      continue;
    }
    if (extension.type == ExtensionType<TransportSequenceNumber>() ||
        extension.type == ExtensionType<TransportSequenceNumberV2>() ||
        extension.type == ExtensionType<TransmissionOffset>() ||
        extension.type == ExtensionType<AbsoluteSendTime>()) {
      // Nullify whole extension, as it's filled in the pacer.
      memset(WriteAt(extension.offset), 0, extension.length);
      continue;
    }
    // Non-mutable extension or unknown. Don't change it.
  }
}

void RtpPacket::SetCsrcs(rtc::ArrayView<const uint32_t> csrcs) {
  RTC_DCHECK_EQ(extensions_size_, 0);
  RTC_DCHECK_EQ(payload_size_, 0);
  RTC_DCHECK_EQ(padding_size_, 0);
  RTC_DCHECK_LE(csrcs.size(), 0x0fu);
  RTC_DCHECK_LE(kFixedHeaderSize + 4 * csrcs.size(), capacity());
  payload_offset_ = kFixedHeaderSize + 4 * csrcs.size();
  WriteAt(0, (data()[0] & 0xF0) | rtc::dchecked_cast<uint8_t>(csrcs.size()));
  size_t offset = kFixedHeaderSize;
  for (uint32_t csrc : csrcs) {
    ByteWriter<uint32_t>::WriteBigEndian(WriteAt(offset), csrc);
    offset += 4;
  }
  buffer_.SetSize(payload_offset_);
}

rtc::ArrayView<uint8_t> RtpPacket::AllocateRawExtension(ExtensionInfo& entry,
                                                        size_t length) {
  if (entry.offset != 0) {
    // Extension already reserved. Check if same length is used.
    if (entry.length == length)
      return rtc::MakeArrayView(WriteAt(entry.offset), length);

    RTC_LOG(LS_ERROR) << "Length mismatch for extension id " << entry.id
                      << ": expected " << static_cast<int>(entry.length)
                      << ". received " << length;
    return nullptr;
  }
  if (payload_size_ > 0) {
    RTC_LOG(LS_ERROR) << "Can't add new extension id " << entry.id
                      << " after payload was set.";
    return nullptr;
  }
  if (padding_size_ > 0) {
    RTC_LOG(LS_ERROR) << "Can't add new extension id " << entry.id
                      << " after padding was set.";
    return nullptr;
  }

  const size_t num_csrc = data()[0] & 0x0F;
  const size_t extensions_offset = kFixedHeaderSize + (num_csrc * 4) + 4;
  // Determine if two-byte header is required for the extension based on id and
  // length. Please note that a length of 0 also requires two-byte header
  // extension. See RFC8285 Section 4.2-4.3.
  const bool two_byte_header_required =
      entry.id > RtpExtension::kOneByteHeaderExtensionMaxId ||
      length > RtpExtension::kOneByteHeaderExtensionMaxValueSize || length == 0;
  RTC_CHECK(!two_byte_header_required ||
            allow_create_two_byte_header_extension_);

  uint16_t profile_id;
  if (extensions_size_ > 0) {
    profile_id =
        ByteReader<uint16_t>::ReadBigEndian(data() + extensions_offset - 4);
    if (profile_id == kOneByteExtensionProfileId && two_byte_header_required) {
      // Is buffer size big enough to fit promotion and new data field?
      // The header extension will grow with one byte per already allocated
      // extension + the size of the extension that is about to be allocated.
      size_t expected_new_extensions_size =
          extensions_size_ + extension_entries_.size() +
          kTwoByteExtensionHeaderLength + length;
      if (extensions_offset + expected_new_extensions_size > capacity()) {
        RTC_LOG(LS_ERROR)
            << "Extension cannot be registered: Not enough space left in "
               "buffer to change to two-byte header extension and add new "
               "extension.";
        return nullptr;
      }
      // Promote already written data to two-byte header format.
      PromoteToTwoByteHeaderExtension();
      profile_id = kTwoByteExtensionProfileId;
    }
  } else {
    // Profile specific ID, set to OneByteExtensionHeader unless
    // TwoByteExtensionHeader is required.
    profile_id = two_byte_header_required ? kTwoByteExtensionProfileId
                                          : kOneByteExtensionProfileId;
  }

  const size_t extension_header_size = profile_id == kOneByteExtensionProfileId
                                           ? kOneByteExtensionHeaderLength
                                           : kTwoByteExtensionHeaderLength;
  size_t new_extensions_size =
      extensions_size_ + extension_header_size + length;
  if (extensions_offset + new_extensions_size > capacity()) {
    RTC_LOG(LS_ERROR)
        << "Extension cannot be registered: Not enough space left in buffer.";
    return nullptr;
  }

  // All checks passed, write down the extension headers.
  if (extensions_size_ == 0) {
    RTC_DCHECK_EQ(payload_offset_, kFixedHeaderSize + (num_csrc * 4));
    WriteAt(0, data()[0] | 0x10);  // Set extension bit.
    ByteWriter<uint16_t>::WriteBigEndian(WriteAt(extensions_offset - 4),
                                         profile_id);
  }

  if (profile_id == kOneByteExtensionProfileId) {
    uint8_t one_byte_header = rtc::dchecked_cast<uint8_t>(entry.id) << 4;
    one_byte_header |= rtc::dchecked_cast<uint8_t>(length - 1);
    WriteAt(extensions_offset + extensions_size_, one_byte_header);
  } else {
    // TwoByteHeaderExtension.
    uint8_t extension_id = rtc::dchecked_cast<uint8_t>(entry.id);
    WriteAt(extensions_offset + extensions_size_, extension_id);
    uint8_t extension_length = rtc::dchecked_cast<uint8_t>(length);
    WriteAt(extensions_offset + extensions_size_ + 1, extension_length);
  }

  entry.offset = rtc::dchecked_cast<uint16_t>(
      extensions_offset + extensions_size_ + extension_header_size);
  entry.length = rtc::dchecked_cast<uint8_t>(length);

  extensions_size_ = new_extensions_size;

  uint16_t extensions_size_padded =
      SetExtensionLengthMaybeAddZeroPadding(extensions_offset);
  payload_offset_ = extensions_offset + extensions_size_padded;
  buffer_.SetSize(payload_offset_);
  return rtc::MakeArrayView(WriteAt(entry.offset), entry.length);
}

void RtpPacket::PromoteToTwoByteHeaderExtension() {
  size_t num_csrc = data()[0] & 0x0F;
  size_t extensions_offset = kFixedHeaderSize + (num_csrc * 4) + 4;

  RTC_CHECK_GT(extension_entries_.size(), 0);
  RTC_CHECK_EQ(payload_size_, 0);
  RTC_CHECK_EQ(kOneByteExtensionProfileId, ByteReader<uint16_t>::ReadBigEndian(
                                               data() + extensions_offset - 4));
  // Rewrite data.
  // Each extension adds one to the offset. The write-read delta for the last
  // extension is therefore the same as the number of extension entries.
  size_t write_read_delta = extension_entries_.size();
  for (auto extension_entry = extension_entries_.rbegin();
       extension_entry != extension_entries_.rend(); ++extension_entry) {
    size_t read_index = extension_entry->offset;
    size_t write_index = read_index + write_read_delta;
    // Update offset.
    extension_entry->offset = rtc::dchecked_cast<uint16_t>(write_index);
    // Copy data. Use memmove since read/write regions may overlap.
    memmove(WriteAt(write_index), data() + read_index, extension_entry->length);
    // Rewrite id and length.
    WriteAt(--write_index, extension_entry->length);
    WriteAt(--write_index, extension_entry->id);
    --write_read_delta;
  }

  // Update profile header, extensions length, and zero padding.
  ByteWriter<uint16_t>::WriteBigEndian(WriteAt(extensions_offset - 4),
                                       kTwoByteExtensionProfileId);
  extensions_size_ += extension_entries_.size();
  uint16_t extensions_size_padded =
      SetExtensionLengthMaybeAddZeroPadding(extensions_offset);
  payload_offset_ = extensions_offset + extensions_size_padded;
  buffer_.SetSize(payload_offset_);
}

uint16_t RtpPacket::SetExtensionLengthMaybeAddZeroPadding(
    size_t extensions_offset) {
  // Update header length field.
  uint16_t extensions_words = rtc::dchecked_cast<uint16_t>(
      (extensions_size_ + 3) / 4);  // Wrap up to 32bit.
  ByteWriter<uint16_t>::WriteBigEndian(WriteAt(extensions_offset - 2),
                                       extensions_words);
  // Fill extension padding place with zeroes.
  size_t extension_padding_size = 4 * extensions_words - extensions_size_;
  memset(WriteAt(extensions_offset + extensions_size_), 0,
         extension_padding_size);
  return 4 * extensions_words;
}

uint8_t* RtpPacket::AllocatePayload(size_t size_bytes) {
  // Reset payload size to 0. If CopyOnWrite buffer_ was shared, this will cause
  // reallocation and memcpy. Keeping just header reduces memcpy size.
  SetPayloadSize(0);
  return SetPayloadSize(size_bytes);
}

uint8_t* RtpPacket::SetPayloadSize(size_t size_bytes) {
  RTC_DCHECK_EQ(padding_size_, 0);
  if (payload_offset_ + size_bytes > capacity()) {
    RTC_LOG(LS_WARNING) << "Cannot set payload, not enough space in buffer.";
    return nullptr;
  }
  payload_size_ = size_bytes;
  buffer_.SetSize(payload_offset_ + payload_size_);
  return WriteAt(payload_offset_);
}

bool RtpPacket::SetPadding(size_t padding_bytes) {
  if (payload_offset_ + payload_size_ + padding_bytes > capacity()) {
    RTC_LOG(LS_WARNING) << "Cannot set padding size " << padding_bytes
                        << ", only "
                        << (capacity() - payload_offset_ - payload_size_)
                        << " bytes left in buffer.";
    return false;
  }
  padding_size_ = rtc::dchecked_cast<uint8_t>(padding_bytes);
  buffer_.SetSize(payload_offset_ + payload_size_ + padding_size_);
  if (padding_size_ > 0) {
    size_t padding_offset = payload_offset_ + payload_size_;
    size_t padding_end = padding_offset + padding_size_;
    memset(WriteAt(padding_offset), 0, padding_size_ - 1);
    WriteAt(padding_end - 1, padding_size_);
    WriteAt(0, data()[0] | 0x20);  // Set padding bit.
  } else {
    WriteAt(0, data()[0] & ~0x20);  // Clear padding bit.
  }
  return true;
}

void RtpPacket::Clear() {
  marker_ = false;
  payload_type_ = 0;
  sequence_number_ = 0;
  timestamp_ = 0;
  ssrc_ = 0;
  payload_offset_ = kFixedHeaderSize;
  payload_size_ = 0;
  padding_size_ = 0;
  extensions_size_ = 0;
  extension_entries_.clear();

  memset(WriteAt(0), 0, kFixedHeaderSize);
  buffer_.SetSize(kFixedHeaderSize);
  WriteAt(0, kRtpVersion << 6);
}

bool RtpPacket::ParseBuffer(const uint8_t* buffer, size_t size) {
  if (size < kFixedHeaderSize) {
    return false;
  }
  const uint8_t version = buffer[0] >> 6;
  if (version != kRtpVersion) {
    return false;
  }
  const bool has_padding = (buffer[0] & 0x20) != 0;
  const bool has_extension = (buffer[0] & 0x10) != 0;
  const uint8_t number_of_crcs = buffer[0] & 0x0f;
  marker_ = (buffer[1] & 0x80) != 0;
  payload_type_ = buffer[1] & 0x7f;

  sequence_number_ = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]);
  timestamp_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[4]);
  ssrc_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[8]);
  if (size < kFixedHeaderSize + number_of_crcs * 4) {
    return false;
  }
  payload_offset_ = kFixedHeaderSize + number_of_crcs * 4;

  extensions_size_ = 0;
  extension_entries_.clear();
  if (has_extension) {
    /* RTP header extension, RFC 3550.
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      defined by profile       |           length              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        header extension                       |
    |                             ....                              |
    */
    size_t extension_offset = payload_offset_ + 4;
    if (extension_offset > size) {
      return false;
    }
    uint16_t profile =
        ByteReader<uint16_t>::ReadBigEndian(&buffer[payload_offset_]);
    size_t extensions_capacity =
        ByteReader<uint16_t>::ReadBigEndian(&buffer[payload_offset_ + 2]);
    extensions_capacity *= 4;
    if (extension_offset + extensions_capacity > size) {
      return false;
    }
    if (profile != kOneByteExtensionProfileId &&
        (profile & kTwobyteExtensionProfileIdAppBitsFilter) !=
            kTwoByteExtensionProfileId) {
      RTC_LOG(LS_WARNING) << "Unsupported rtp extension " << profile;
    } else {
      size_t extension_header_length = profile == kOneByteExtensionProfileId
                                           ? kOneByteExtensionHeaderLength
                                           : kTwoByteExtensionHeaderLength;
      constexpr uint8_t kPaddingByte = 0;
      constexpr uint8_t kPaddingId = 0;
      constexpr uint8_t kOneByteHeaderExtensionReservedId = 15;
      while (extensions_size_ + extension_header_length < extensions_capacity) {
        if (buffer[extension_offset + extensions_size_] == kPaddingByte) {
          extensions_size_++;
          continue;
        }
        int id;
        uint8_t length;
        if (profile == kOneByteExtensionProfileId) {
          id = buffer[extension_offset + extensions_size_] >> 4;
          length = 1 + (buffer[extension_offset + extensions_size_] & 0xf);
          if (id == kOneByteHeaderExtensionReservedId ||
              (id == kPaddingId && length != 1)) {
            break;
          }
        } else {
          id = buffer[extension_offset + extensions_size_];
          length = buffer[extension_offset + extensions_size_ + 1];
        }

        if (extensions_size_ + extension_header_length + length >
            extensions_capacity) {
          RTC_LOG(LS_WARNING) << "Oversized rtp header extension.";
          break;
        }

        ExtensionInfo& extension_info = FindOrCreateExtensionInfo(id);
        if (extension_info.length != 0) {
          RTC_LOG(LS_VERBOSE)
              << "Duplicate rtp header extension id " << id << ". Overwriting.";
        }

        size_t offset =
            extension_offset + extensions_size_ + extension_header_length;
        if (!rtc::IsValueInRangeForNumericType<uint16_t>(offset)) {
          RTC_DLOG(LS_WARNING) << "Oversized rtp header extension.";
          break;
        }
        extension_info.offset = static_cast<uint16_t>(offset);
        extension_info.length = length;
        extensions_size_ += extension_header_length + length;
      }
    }
    payload_offset_ = extension_offset + extensions_capacity;
  }

  if (has_padding && payload_offset_ < size) {
    padding_size_ = buffer[size - 1];
    if (padding_size_ == 0) {
      RTC_LOG(LS_WARNING) << "Padding was set, but padding size is zero";
      return false;
    }
  } else {
    padding_size_ = 0;
  }

  if (payload_offset_ + padding_size_ > size) {
    return false;
  }
  payload_size_ = size - payload_offset_ - padding_size_;
  return true;
}

const RtpPacket::ExtensionInfo* RtpPacket::FindExtensionInfo(int id) const {
  for (const ExtensionInfo& extension : extension_entries_) {
    if (extension.id == id) {
      return &extension;
    }
  }
  return nullptr;
}

RtpPacket::ExtensionInfo& RtpPacket::FindOrCreateExtensionInfo(int id) {
  for (ExtensionInfo& extension : extension_entries_) {
    if (extension.id == id) {
      return extension;
    }
  }
  ExtensionInfo new_entry;
  new_entry.id = id;
  extension_entries_.push_back(new_entry);
  return extension_entries_.back();
}

rtc::ArrayView<const uint8_t> RtpPacket::UnsafeFindExtension(
    const void* type) const {
  for (const ExtensionInfo& extension : extension_entries_) {
    if (extension.type == type) {
      rtc::MakeArrayView(data() + extension.offset, extension.length);
    }
  }
  return nullptr;
}

rtc::ArrayView<uint8_t> RtpPacket::UnsafeAllocateExtension(const void* type,
                                                           size_t length) {
  if (allow_create_two_byte_header_extension_) {
    if (length > RtpExtension::kMaxValueSize) {
      return {};
    }
  } else {
    if (length == 0 ||
        length > RtpExtension::kOneByteHeaderExtensionMaxValueSize) {
      return {};
    }
  }

  for (ExtensionInfo& extension : extension_entries_) {
    if (extension.type == type) {
      return AllocateRawExtension(extension, length);
    }
  }

  // Extension not registered.
  return {};
}

RtpPacket::ExtensionInfo* RtpPacket::FindExtensionInfo(const void* type) {
  for (auto& entry : extension_entries_) {
    if (entry.type == type) {
      return &entry;
    }
  }
  return nullptr;
}

const RtpPacket::ExtensionInfo* RtpPacket::FindExtensionInfo(
    const void* type) const {
  for (auto& entry : extension_entries_) {
    if (entry.type == type) {
      return &entry;
    }
  }
  return nullptr;
}

bool RtpPacket::UnsafeRemoveExtension(const void* type) {
  ExtensionInfo* entry = FindExtensionInfo(type);
  if (entry == nullptr || entry->offset == 0) {
    // Extension is not registered, or wasn't set.
    return false;
  }

  // Rebuild new packet from scratch.
  RtpPacket new_packet;

  new_packet.SetMarker(Marker());
  new_packet.SetPayloadType(PayloadType());
  new_packet.SetSequenceNumber(SequenceNumber());
  new_packet.SetTimestamp(Timestamp());
  new_packet.SetSsrc(Ssrc());
  new_packet.extension_entries_ = extension_entries_;

  // Copy all extensions, except the one we are removing.
  for (size_t i = 0; i < extension_entries_.size(); ++i) {
    const ExtensionInfo& old_entry = extension_entries_[i];
    ExtensionInfo& new_entry = new_packet.extension_entries_[i];
    new_entry.offset = 0;
    new_entry.length = 0;
    if (old_entry.type == type) {
      // Extension that should be removed.
      continue;
    }
    if (old_entry.offset == 0) {
      // Unset extension, nothing to copy.
      continue;
    }
    auto extension_data =
        new_packet.AllocateRawExtension(new_entry, old_entry.length);
    if (extension_data.size() != old_entry.length) {
      RTC_LOG(LS_ERROR) << "Failed to allocate extension id=" << old_entry.id
                        << ", length=" << old_entry.length
                        << ", packet=" << ToString();
      return false;
    }

    // Copy extension data to new packet.
    memcpy(extension_data.data(), ReadAt(old_entry.offset), old_entry.length);
  }

  // Copy payload data to new packet.
  memcpy(new_packet.AllocatePayload(payload_size()), payload().data(),
         payload_size());

  // Allocate padding -- must be last!
  new_packet.SetPadding(padding_size());

  // Success, replace current packet with newly built packet.
  *this = new_packet;
  return true;
}

std::string RtpPacket::ToString() const {
  rtc::StringBuilder result;
  result << "{payload_type=" << payload_type_ << "marker=" << marker_
         << ", sequence_number=" << sequence_number_
         << ", padding_size=" << padding_size_ << ", timestamp=" << timestamp_
         << ", ssrc=" << ssrc_ << ", payload_offset=" << payload_offset_
         << ", payload_size=" << payload_size_ << ", total_size=" << size()
         << "}";

  return result.Release();
}

}  // namespace webrtc
