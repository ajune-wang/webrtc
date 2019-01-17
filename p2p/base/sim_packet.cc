#include "p2p/base/sim_packet.h"

#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

#if defined(USE_RAW_IP_PACKET)

SimPacket::SimPacket(const uint8_t* raw_ip_packet, size_t len)
    : creation_ts_(rtc::TimeMillis()), buffer_(raw_ip_packet, len) {
  rtc::ByteBufferReader reader(reinterpret_cast<char*>(buffer_.data()),
                               buffer_.size());
  if (!Read(&reader)) {
    buffer_.Clear();
  }
}

bool SimPacket::Read(rtc::ByteBufferReader* reader) {
  return ReadHeader(reader) && ReadPayload(reader);
}

bool SimPacket::ReadHeader(rtc::ByteBufferReader* reader) {
  uint8_t ver_and_ihl;
  if (!reader->ReadUInt8(&ver_and_ihl)) {
    RTC_LOG(INFO) << "Cannot read the version and IHL fields";
    return false;
  }
  uint8_t ip_version = ver_and_ihl >> 4;
  switch (ip_version) {
    case 4:
      af_ = AddressFamily::kIpv4;
      break;
    case 6:
      af_ = AddressFamily::kIpv6;
      break;
    default:
      af_ = AddressFamily::kUnsupported;
      break;
  }
  // Only support IPv4 now.
  if (af_ != AddressFamily::kIpv4) {
    RTC_LOG(INFO) << "Supported IPv6 packet.";
    return false;
  }
  header_len_ = ver_and_ihl & 0x0f;
  uint16_t flags_and_frag_offset;
  if (!reader->ReadUInt8(&service_type_) || !reader->ReadUInt16(&total_len_) ||
      !reader->ReadUInt16(&identification_) ||
      !reader->ReadUInt16(&flags_and_frag_offset) ||
      !reader->ReadUInt8(&ttl_) || !reader->ReadUInt8(&protocol_) ||
      !reader->ReadUInt16(&checksum_)) {
    RTC_LOG(INFO) << "Cannot the header.";
    return false;
  }
  RTC_DCHECK_EQ(total_len_, buffer_.size());
  flags_ = flags_and_frag_offset >> 13;
  frag_offset_ = flags_and_frag_offset & 0x1fff;

  uint32_t src;
  uint32_t dst;
  if (!reader->ReadUInt32(&src) || !reader->ReadUInt32(&dst)) {
    RTC_LOG(INFO) << "Cannot read src or dst address.";
    return false;
  }

  size_t header_bytes_consumed = buffer_.size() - reader->Length();
  // Consume the variable option field.
  if (!reader->Consume(4 * header_len_ - header_bytes_consumed)) {
    RTC_LOG(INFO) << "Cannot consume the option field.";
    return false;
  }

  src_ip_ = rtc::IPAddress(src);
  dst_ip_ = rtc::IPAddress(dst);
  RTC_LOG(INFO) << "src ip = " << src_ip_.ToString();
  RTC_LOG(INFO) << "dst ip = " << dst_ip_.ToString();
  return true;
}

bool SimPacket::ReadPayload(rtc::ByteBufferReader* reader) {
  RTC_DCHECK_GE(buffer_.size(), reader->Length());
  payload_offset_ = buffer_.size() - reader->Length();
  RTC_DCHECK_EQ(header_len_ * 4, payload_offset_);
  if (!reader->ReadUInt16(&src_port_) || !reader->ReadUInt16(&dst_port_)) {
    return false;
  }
  return true;
}

bool SimPacket::Write(rtc::ByteBufferWriter* writer) {
  return !WriteHeader(writer) || !WritePayload(writer);
}

bool SimPacket::WriteHeader(rtc::ByteBufferWriter* writer) {
  uint8_t ip_version = 0;
  switch (af_) {
    case AddressFamily::kIpv4:
      ip_version = 4;
      break;
    case AddressFamily::kIpv6:
    case AddressFamily::kUnsupported:
    default:
      RTC_NOTREACHED();
  }
  writer->WriteUInt8(ip_version << 4 | header_len_);
  writer->WriteUInt8(service_type_);
  writer->WriteUInt16(total_len_);
  writer->WriteUInt16(identification_);
  writer->WriteUInt16(flags_ << 13 | frag_offset_);
  writer->WriteUInt8(ttl_);
  writer->WriteUInt8(protocol_);
  writer->WriteUInt16(ComputeChecksum(writer));
  // TODO(qingsi): This is Posix-specific.
  writer->WriteUInt32(rtc::HostToNetwork32(src_ip_.ipv4_address().s_addr));
  writer->WriteUInt32(rtc::HostToNetwork32(dst_ip_.ipv4_address().s_addr));
  size_t option_length = 4 * header_len_ - writer->Length();
  writer->WriteBytes(reinterpret_cast<char*>(buffer_.data()) + writer->Length(),
                     option_length);
  return true;
}

bool SimPacket::WritePayload(rtc::ByteBufferWriter* writer) {
  writer->WriteUInt16(src_port_);
  writer->WriteUInt16(dst_port_);
  char* remaining_payload = reinterpret_cast<char*>(buffer_.data()) +
                            payload_offset_ + 2 * sizeof(uint16_t);
  writer->WriteBytes(remaining_payload,
                     buffer_.size() - payload_offset_ - 2 * sizeof(uint16_t));
  RTC_DCHECK_EQ(buffer_.size(), writer->Length());
  return true;
}

uint16_t SimPacket::ComputeChecksum(rtc::ByteBufferWriter* writer) {
  RTC_DCHECK(writer->Length() == 10);
  const uint16_t* data = reinterpret_cast<const uint16_t*>(writer->Data());
  uint32_t checksum = 0;
  for (size_t i = 0; i < 5; ++i) {
    checksum += rtc::NetworkToHost16(data[i]);
  }
  uint32_t raw_src_ip = rtc::HostToNetwork32(src_ip_.ipv4_address().s_addr);
  uint32_t raw_dst_ip = rtc::HostToNetwork32(dst_ip_.ipv4_address().s_addr);
  checksum += (raw_src_ip & 0x0000ffff) + (raw_src_ip >> 16);
  checksum += (raw_dst_ip & 0x0000ffff) + (raw_dst_ip >> 16);
  return ~((checksum & 0x0000ffff) + (checksum >> 16));
}

#else  // #if defined(USE_RAW_IP_PACKET)

SimPacket::SimPacket(const uint8_t* data, size_t len) : buffer_(data, len) {}

SimPacket::~SimPacket() = default;

#endif

}  // namespace webrtc
