#ifndef SIM_PACKET_H_
#define SIM_PACKET_H_

#include "rtc_base/buffer.h"
#include "rtc_base/bytebuffer.h"
#include "rtc_base/ipaddress.h"
#include "rtc_base/network.h"
#include "rtc_base/refcount.h"

//#define USE_RAW_IP_PACKET

namespace webrtc {

// Wraps an IP packet that encapsulates a UDP packet in the payload.
#if defined(USE_RAW_IP_PACKET)

class SimPacket : public rtc::RefCountInterface {
 public:
  enum class AddressFamily {
    kIpv4,
    kIpv6,
    kUnsupported,
  };
  // The data is memcopy'd.
  SimPacket(const uint8_t* raw_ip_packet, size_t len);
  ~SimPacket() override;

  bool IsValid() const { return !buffer_.empty(); }

  bool Read(rtc::ByteBufferReader* reader);
  bool Write(rtc::ByteBufferWriter* writer);

  const rtc::IPAddress src_ip() const { return src_ip_; }
  const rtc::IPAddress dst_ip() const { return dst_ip_; }
  uint16_t src_port() const { return src_port_; }
  uint16_t dst_port() const { return dst_port_; }

  void set_src_ip(const rtc::IPAddress& ip) { src_ip_ = ip; }
  void set_dst_ip(const rtc::IPAddress& ip) { dst_ip_ = ip; }
  void set_src_port(uint16_t port) { src_port_ = port; }
  void set_dst_port(uint16_t port) { dst_port_ = port; }

  const rtc::BufferT<uint8_t>& buffer() const { return buffer_; }
  // Note that |buffer_| stores uint8_t.
  size_t SizeInBytes() const { return buffer_.size(); }

  uint16_t payload_offset() { return payload_offset_; }

 private:
  bool ReadHeader(rtc::ByteBufferReader* reader);
  bool ReadPayload(rtc::ByteBufferReader* reader);

  bool WriteHeader(rtc::ByteBufferWriter* writer);
  bool WritePayload(rtc::ByteBufferWriter* writer);

  uint16_t ComputeChecksum(rtc::ByteBufferWriter* writer);

  const uint32_t creation_ts_;
  AddressFamily af_ = AddressFamily::kUnsupported;
  // In 32-bit word. Only the lowest 4 bits are valid.
  uint8_t header_len_ = 0;
  uint8_t service_type_ = 0;
  uint16_t total_len_ = 0;
  uint16_t identification_ = 0;
  uint16_t flags_ = 0;        // Only the lowest 3 bits are valid.
  uint16_t frag_offset_ = 0;  // Only the lowest 13 bits are valid.
  uint8_t ttl_ = 0;
  uint8_t protocol_ = 0;
  uint16_t checksum_ = 0;
  rtc::IPAddress src_ip_;
  rtc::IPAddress dst_ip_;
  uint16_t payload_offset_ = 0;
  uint16_t src_port_;
  uint16_t dst_port_;
  rtc::BufferT<uint8_t> buffer_;
};

#else  // #if defined(USE_RAW_IP_PACKET)

class SimPacket : public rtc::RefCountInterface {
 public:
  SimPacket(const uint8_t* data, size_t len);
  ~SimPacket() override;

  const rtc::BufferT<uint8_t>& buffer() const { return buffer_; }

 private:
  rtc::BufferT<uint8_t> buffer_;
};

#endif

}  // namespace webrtc

#endif  // SIM_PACKET_H_
