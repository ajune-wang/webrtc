/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_UTILS_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_UTILS_H_

#include <string>

#include "rtc_base/ignore_wundef.h"

// Files generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log2.pb.h"
#else
#include "logging/rtc_event_log/rtc_event_log2.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()

namespace webrtc {

// rtclog2::IncomingRtpPackets and rtclog2::OutgoingRtpPackets are identical
// in contents. This class allows us to wrap either and avoid code duplication.
template <typename T>
class RtpPacketsProtoReader final {
 public:
  // |this| must outlive |proto|.
  explicit RtpPacketsProtoReader(const T& proto) : proto_(proto) {}

  bool has_timestamp_ms() const { return proto_.has_timestamp_ms(); }
  ::google::protobuf::int64 timestamp_ms() const {
    return proto_.timestamp_ms();
  }

  bool has_marker() const { return proto_.has_marker(); }
  bool marker() const { return proto_.marker(); }

  bool has_payload_type() const { return proto_.has_payload_type(); }
  ::google::protobuf::uint32 payload_type() const {
    return proto_.payload_type();
  }

  bool has_sequence_number() const { return proto_.has_sequence_number(); }
  ::google::protobuf::uint32 sequence_number() const {
    return proto_.sequence_number();
  }

  bool has_rtp_timestamp() const { return proto_.has_rtp_timestamp(); }
  ::google::protobuf::uint32 rtp_timestamp() const {
    return proto_.rtp_timestamp();
  }

  bool has_ssrc() const { return proto_.has_ssrc(); }
  ::google::protobuf::uint32 ssrc() const { return proto_.ssrc(); }

  bool has_payload_size() const { return proto_.has_payload_size(); }
  ::google::protobuf::uint32 payload_size() const {
    return proto_.payload_size();
  }

  bool has_header_size() const { return proto_.has_header_size(); }
  ::google::protobuf::uint32 header_size() const {
    return proto_.header_size();
  }

  bool has_padding_size() const { return proto_.has_padding_size(); }
  ::google::protobuf::uint32 padding_size() const {
    return proto_.padding_size();
  }

  bool has_number_of_deltas() const { return proto_.has_number_of_deltas(); }
  ::google::protobuf::uint32 number_of_deltas() const {
    return proto_.number_of_deltas();
  }

  bool has_transport_sequence_number() const {
    return proto_.has_transport_sequence_number();
  }
  ::google::protobuf::uint32 transport_sequence_number() const {
    return proto_.transport_sequence_number();
  }

  bool has_transmission_time_offset() const {
    return proto_.has_transmission_time_offset();
  }
  ::google::protobuf::int32 transmission_time_offset() const {
    return proto_.transmission_time_offset();
  }

  bool has_absolute_send_time() const {
    return proto_.has_absolute_send_time();
  }
  ::google::protobuf::uint32 absolute_send_time() const {
    return proto_.absolute_send_time();
  }

  bool has_video_rotation() const { return proto_.has_video_rotation(); }
  ::google::protobuf::uint32 video_rotation() const {
    return proto_.video_rotation();
  }

  bool has_audio_level() const { return proto_.has_audio_level(); }
  ::google::protobuf::uint32 audio_level() const {
    return proto_.audio_level();
  }

  bool has_timestamp_deltas_ms() const {
    return proto_.has_timestamp_deltas_ms();
  }
  const ::std::string& timestamp_deltas_ms() const {
    return proto_.timestamp_deltas_ms();
  }

  bool has_marker_deltas() const { return proto_.has_marker_deltas(); }
  const ::std::string& marker_deltas() const { return proto_.marker_deltas(); }

  bool has_payload_type_deltas() const {
    return proto_.has_payload_type_deltas();
  }
  const ::std::string& payload_type_deltas() const {
    return proto_.payload_type_deltas();
  }

  bool has_sequence_number_deltas() const {
    return proto_.has_sequence_number_deltas();
  }
  const ::std::string& sequence_number_deltas() const {
    return proto_.sequence_number_deltas();
  }

  bool has_rtp_timestamp_deltas() const {
    return proto_.has_rtp_timestamp_deltas();
  }
  const ::std::string& rtp_timestamp_deltas() const {
    return proto_.rtp_timestamp_deltas();
  }

  bool has_ssrc_deltas() const { return proto_.has_ssrc_deltas(); }
  const ::std::string& ssrc_deltas() const { return proto_.ssrc_deltas(); }

  bool has_payload_size_deltas() const {
    return proto_.has_payload_size_deltas();
  }
  const ::std::string& payload_size_deltas() const {
    return proto_.payload_size_deltas();
  }

  bool has_header_size_deltas() const {
    return proto_.has_header_size_deltas();
  }
  const ::std::string& header_size_deltas() const {
    return proto_.header_size_deltas();
  }

  bool has_padding_size_deltas() const {
    return proto_.has_padding_size_deltas();
  }
  const ::std::string& padding_size_deltas() const {
    return proto_.padding_size_deltas();
  }

  bool has_transport_sequence_number_deltas() const {
    return proto_.has_transport_sequence_number_deltas();
  }
  const ::std::string& transport_sequence_number_deltas() const {
    return proto_.transport_sequence_number_deltas();
  }

  bool has_transmission_time_offset_deltas() const {
    return proto_.has_transmission_time_offset_deltas();
  }
  const ::std::string& transmission_time_offset_deltas() const {
    return proto_.transmission_time_offset_deltas();
  }

  bool has_absolute_send_time_deltas() const {
    return proto_.has_absolute_send_time_deltas();
  }
  const ::std::string& absolute_send_time_deltas() const {
    return proto_.absolute_send_time_deltas();
  }

  bool has_video_rotation_deltas() const {
    return proto_.has_video_rotation_deltas();
  }
  const ::std::string& video_rotation_deltas() const {
    return proto_.video_rotation_deltas();
  }

  bool has_audio_level_deltas() const {
    return proto_.has_audio_level_deltas();
  }
  const ::std::string& audio_level_deltas() const {
    return proto_.audio_level_deltas();
  }

 private:
  const T& proto_;
};

// TODO: !!! 1. Needed? 2. If not, maybe the other one neither? 3. At the
// very least, I should be able to move it back to another file.

// rtclog2::IncomingRtpPackets and rtclog2::OutgoingRtpPackets are identical
// in contents. This class allows us to wrap either and avoid code duplication.
template <typename T>
class RtpPacketsProtoWriter final {
 public:
  // |this| must outlive |proto|.
  explicit RtpPacketsProtoWriter(const T& proto) : proto_(proto) {}

  void set_timestamp_ms(::google::protobuf::int64 value) {
    proto_.set_timestamp_ms(value);
  }

  void set_marker(bool value) { proto_.set_marker(value); }

  void set_payload_type(::google::protobuf::uint32 value) {
    proto_.set_payload_type(value);
  }

  void set_sequence_number(::google::protobuf::uint32 value) {
    proto_.set_sequence_number(value);
  }

  void set_rtp_timestamp(::google::protobuf::uint32 value) {
    proto_.set_rtp_timestamp(value);
  }

  void set_ssrc(::google::protobuf::uint32 value) { proto_.set_ssrc(value); }

  void set_payload_size(::google::protobuf::uint32 value) {
    proto_.set_payload_size(value);
  }

  void set_header_size(::google::protobuf::uint32 value) {
    proto_.set_header_size(value);
  }

  void set_padding_size(::google::protobuf::uint32 value) {
    proto_.set_padding_size(value);
  }

  void set_number_of_deltas(::google::protobuf::uint32 value) {
    proto_.set_number_of_deltas(value);
  }

  void set_transport_sequence_number(::google::protobuf::uint32 value) {
    proto_.set_transport_sequence_number(value);
  }

  void set_transmission_time_offset(::google::protobuf::int32 value) {
    proto_.set_transmission_time_offset(value);
  }

  void set_absolute_send_time(::google::protobuf::uint32 value) {
    proto_.set_absolute_send_time(value);
  }

  void set_video_rotation(::google::protobuf::uint32 value) {
    proto_.set_video_rotation(value);
  }

  void set_audio_level(::google::protobuf::uint32 value) {
    proto_.set_audio_level(value);
  }

  void set_timestamp_deltas_ms(const ::std::string& value) {
    proto_.set_timestamp_deltas_ms(value);
  }
  void set_timestamp_deltas_ms(::std::string&& value) {
    proto_.set_timestamp_deltas_ms(value);
  }
  void set_timestamp_deltas_ms(const char* value) {
    proto_.set_timestamp_deltas_ms(value);
  }
  void set_timestamp_deltas_ms(const void* value, size_t size) {
    proto_.set_timestamp_deltas_ms(value, size);
  }

  void set_marker_deltas(const ::std::string& value) {
    proto_.set_marker_deltas(value);
  }
  void set_marker_deltas(::std::string&& value) {
    proto_.set_marker_deltas(value);
  }
  void set_marker_deltas(const char* value) { proto_.set_marker_deltas(value); }
  void set_marker_deltas(const void* value, size_t size) {
    proto_.set_marker_deltas(value, size);
  }

  void set_payload_type_deltas(const ::std::string& value) {
    proto_.set_payload_type_deltas(value);
  }
  void set_payload_type_deltas(::std::string&& value) {
    proto_.set_payload_type_deltas(value);
  }
  void set_payload_type_deltas(const char* value) {
    proto_.set_payload_type_deltas(value);
  }
  void set_payload_type_deltas(const void* value, size_t size) {
    proto_.set_payload_type_deltas(value, size);
  }

  void set_sequence_number_deltas(const ::std::string& value) {
    proto_.set_sequence_number_deltas(value);
  }
  void set_sequence_number_deltas(::std::string&& value) {
    proto_.set_sequence_number_deltas(value);
  }
  void set_sequence_number_deltas(const char* value) {
    proto_.set_sequence_number_deltas(value);
  }
  void set_sequence_number_deltas(const void* value, size_t size) {
    proto_.set_sequence_number_deltas(value, size);
  }

  void set_rtp_timestamp_deltas(const ::std::string& value) {
    proto_.set_rtp_timestamp_deltas(value);
  }
  void set_rtp_timestamp_deltas(::std::string&& value) {
    proto_.set_rtp_timestamp_deltas(value);
  }
  void set_rtp_timestamp_deltas(const char* value) {
    proto_.set_rtp_timestamp_deltas(value);
  }
  void set_rtp_timestamp_deltas(const void* value, size_t size) {
    proto_.set_rtp_timestamp_deltas(value, size);
  }

  void set_ssrc_deltas(const ::std::string& value) {
    proto_.set_ssrc_deltas(value);
  }
  void set_ssrc_deltas(::std::string&& value) { proto_.set_ssrc_deltas(value); }
  void set_ssrc_deltas(const char* value) { proto_.set_ssrc_deltas(value); }
  void set_ssrc_deltas(const void* value, size_t size) {
    proto_.set_ssrc_deltas(value, size);
  }

  void set_payload_size_deltas(const ::std::string& value) {
    proto_.set_payload_size_deltas(value);
  }
  void set_payload_size_deltas(::std::string&& value) {
    proto_.set_payload_size_deltas(value);
  }
  void set_payload_size_deltas(const char* value) {
    proto_.set_payload_size_deltas(value);
  }
  void set_payload_size_deltas(const void* value, size_t size) {
    proto_.set_payload_size_deltas(value, size);
  }

  void set_header_size_deltas(const ::std::string& value) {
    proto_.set_header_size_deltas(value);
  }
  void set_header_size_deltas(::std::string&& value) {
    proto_.set_header_size_deltas(value);
  }
  void set_header_size_deltas(const char* value) {
    proto_.set_header_size_deltas(value);
  }
  void set_header_size_deltas(const void* value, size_t size) {
    proto_.set_header_size_deltas(value, size);
  }

  void set_padding_size_deltas(const ::std::string& value) {
    proto_.set_padding_size_deltas(value);
  }
  void set_padding_size_deltas(::std::string&& value) {
    proto_.set_padding_size_deltas(value);
  }
  void set_padding_size_deltas(const char* value) {
    proto_.set_padding_size_deltas(value);
  }
  void set_padding_size_deltas(const void* value, size_t size) {
    proto_.set_padding_size_deltas(value, size);
  }

  void set_transport_sequence_number_deltas(const ::std::string& value) {
    proto_.set_transport_sequence_number_deltas(value);
  }
  void set_transport_sequence_number_deltas(::std::string&& value) {
    proto_.set_transport_sequence_number_deltas(value);
  }
  void set_transport_sequence_number_deltas(const char* value) {
    proto_.set_transport_sequence_number_deltas(value);
  }
  void set_transport_sequence_number_deltas(const void* value, size_t size) {
    proto_.set_transport_sequence_number_deltas(value, size);
  }

  void set_transmission_time_offset_deltas(const ::std::string& value) {
    proto_.set_transmission_time_offset_deltas(value);
  }
  void set_transmission_time_offset_deltas(::std::string&& value) {
    proto_.set_transmission_time_offset_deltas(value);
  }
  void set_transmission_time_offset_deltas(const char* value) {
    proto_.set_transmission_time_offset_deltas(value);
  }
  void set_transmission_time_offset_deltas(const void* value, size_t size) {
    proto_.set_transmission_time_offset_deltas(value, size);
  }

  void set_absolute_send_time_deltas(const ::std::string& value) {
    proto_.set_absolute_send_time_deltas(value);
  }
  void set_absolute_send_time_deltas(::std::string&& value) {
    proto_.set_absolute_send_time_deltas(value);
  }
  void set_absolute_send_time_deltas(const char* value) {
    proto_.set_absolute_send_time_deltas(value);
  }
  void set_absolute_send_time_deltas(const void* value, size_t size) {
    proto_.set_absolute_send_time_deltas(value, size);
  }

  void set_video_rotation_deltas(const ::std::string& value) {
    proto_.set_video_rotation_deltas(value);
  }
  void set_video_rotation_deltas(::std::string&& value) {
    proto_.set_video_rotation_deltas(value);
  }
  void set_video_rotation_deltas(const char* value) {
    proto_.set_video_rotation_deltas(value);
  }
  void set_video_rotation_deltas(const void* value, size_t size) {
    proto_.set_video_rotation_deltas(value, size);
  }

  void set_audio_level_deltas(const ::std::string& value) {
    proto_.set_audio_level_deltas(value);
  }
  void set_audio_level_deltas(::std::string&& value) {
    proto_.set_audio_level_deltas(value);
  }
  void set_audio_level_deltas(const char* value) {
    proto_.set_audio_level_deltas(value);
  }
  void set_audio_level_deltas(const void* value, size_t size) {
    proto_.set_audio_level_deltas(value, size);
  }

 private:
  const T& proto_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_UTILS_H_
