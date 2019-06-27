/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/rtcp_packet/network_estimate.h"

#include <algorithm>
#include <type_traits>
#include <vector>

#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"

namespace webrtc {
namespace rtcp {
namespace {

constexpr TimeDelta kTimestampResolution = TimeDelta::Millis<1>();

template <typename T>
T GetMaxVal() {
  return T::PlusInfinity();
}

template <>
double GetMaxVal<double>() {
  return INFINITY;
}
template <typename StructType>
class SerializerImpl : public Serializer<StructType> {
 public:
  static constexpr int kBytes = 3;
  static constexpr int kFieldSize = 1 + kBytes;
  struct FieldSerializer {
    uint8_t id;
    std::function<void(const uint8_t* src, StructType* target)> read;
    std::function<void(const StructType& src, uint8_t* target)> write;
  };
  template <typename T, typename Closure>
  static FieldSerializer Field(uint8_t id, T resolution, Closure field_getter) {
    constexpr uint32_t kMaxEncoded = (1 << (kBytes * 8)) - 1;
    return FieldSerializer{
        id,
        [=](const uint8_t* src, StructType* target) {
          uint32_t scaled =
              ByteReader<uint32_t, kBytes, false>::ReadBigEndian(src);
          if (scaled == kMaxEncoded) {
            *field_getter(target) = GetMaxVal<T>();
          } else {
            *field_getter(target) = resolution * static_cast<int64_t>(scaled);
          }
        },
        [=](const StructType& src, uint8_t* target) {
          auto value = *field_getter(const_cast<StructType*>(&src));
          uint32_t scaled =
              value == GetMaxVal<T>()
                  ? kMaxEncoded
                  : std::max<int64_t>(
                        0, std::min<int64_t>(value / resolution, kMaxEncoded));
          ByteWriter<uint32_t, kBytes, false>::WriteBigEndian(target, scaled);
        }};
  }
  static FieldSerializer TimestampField(
      uint8_t id,
      std::function<Timestamp*(StructType*)> field_getter) {
    constexpr Timestamp kTimeZero = Timestamp::Millis<0>();
    return FieldSerializer{
        id,
        [=](const uint8_t* src, StructType* target) {
          uint32_t scaled =
              ByteReader<uint32_t, kBytes, false>::ReadBigEndian(src);
          *field_getter(target) =
              kTimeZero + kTimestampResolution * static_cast<int64_t>(scaled);
        },
        [=](const StructType& src, uint8_t* target) {
          constexpr uint32_t kWraparound = 1 << (kBytes * 8);
          auto value = *field_getter(const_cast<StructType*>(&src)) - kTimeZero;
          uint32_t scaled =
              static_cast<uint32_t>(value / kTimestampResolution) &
              (kWraparound - 1);
          ByteWriter<uint32_t, kBytes, false>::WriteBigEndian(target, scaled);
        }};
  }

  explicit SerializerImpl(std::vector<FieldSerializer> fields)
      : fields_(fields) {}

  rtc::Buffer Serialize(const StructType& src) override {
    size_t size = fields_.size() * kFieldSize;
    rtc::Buffer buf(size);
    uint8_t* target_ptr = buf.data();
    for (const auto& field : fields_) {
      ByteWriter<uint8_t>::WriteBigEndian(target_ptr++, field.id);
      field.write(src, target_ptr);
      target_ptr += kBytes;
    }
    return buf;
  }

  void Parse(rtc::ArrayView<const uint8_t> src, StructType* target) override {
    RTC_DCHECK_EQ(src.size() % kFieldSize, 0);
    for (const uint8_t* data_ptr = src.data(); data_ptr < src.end();
         data_ptr += kFieldSize) {
      uint8_t field_id = ByteReader<uint8_t>::ReadBigEndian(data_ptr);
      for (const auto& field : fields_) {
        if (field.id == field_id) {
          field.read(data_ptr + 1, target);
          break;
        }
      }
    }
  }

 private:
  const std::vector<FieldSerializer> fields_;
};
Serializer<NetworkStateEstimate>* GetSerializer() {
  using E = NetworkStateEstimate;
  using S = SerializerImpl<NetworkStateEstimate>;
  static SerializerImpl<NetworkStateEstimate>* serializer =
      new SerializerImpl<NetworkStateEstimate>({
          S::TimestampField(1, [](E* e) { return &e->last_send_time; }),
          S::Field(2, 1e-2, [](E* e) { return &e->confidence; }),
          S::Field(3, DataRate::kbps(1),
                   [](E* e) { return &e->link_capacity; }),
          S::Field(4, DataRate::kbps(1),
                   [](E* e) { return &e->link_capacity_lower; }),
          S::Field(5, DataRate::kbps(1),
                   [](E* e) { return &e->available_capacity; }),
          S::Field(6, TimeDelta::ms(1),
                   [](E* e) { return &e->pre_link_buffer_delay; }),
      });
  return serializer;
}

}  // namespace

NetworkEstimate::NetworkEstimate() : serializer_(GetSerializer()) {
  SetSubType(kSubType);
  SetName(kName);
  SetSsrc(0);
}

TimeDelta NetworkEstimate::GetTimestampPeriod() {
  return kTimestampResolution *
         (1 << (SerializerImpl<NetworkStateEstimate>::kBytes * 8));
}

bool NetworkEstimate::IsNetworkEstimate(const CommonHeader& packet) {
  if (packet.fmt() != kSubType)
    return false;
  if (packet.packet_size() < 8)
    return false;
  if (ByteReader<uint32_t>::ReadBigEndian(&packet.payload()[4]) != kName)
    return false;
  return true;
}

bool NetworkEstimate::Parse(const CommonHeader& packet) {
  if (!App::Parse(packet))
    return false;
  serializer_->Parse({data(), data_size()}, &estimate_);
  return true;
}

void NetworkEstimate::SetEstimate(NetworkStateEstimate estimate) {
  estimate_ = estimate;
  auto buf = serializer_->Serialize(estimate);
  SetData(buf.data(), buf.size());
}

}  // namespace rtcp
}  // namespace webrtc
