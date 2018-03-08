/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/analyzer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <utility>

#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "call/call.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "common_types.h"  // NOLINT(build/include)
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/audio_coding/neteq/tools/audio_sink.h"
#include "modules/audio_coding/neteq/tools/fake_decode_from_file.h"
#include "modules/audio_coding/neteq/tools/neteq_delay_analyzer.h"
#include "modules/audio_coding/neteq/tools/neteq_replacement_input.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/audio_coding/neteq/tools/resample_input_audio_file.h"
#include "modules/congestion_controller/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/bitrate_estimator.h"
#include "modules/congestion_controller/delay_based_bwe.h"
#include "modules/congestion_controller/include/receive_side_congestion_controller.h"
#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "modules/include/module_common_types.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/function_view.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/rate_statistics.h"

#ifndef BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
#define BWE_TEST_LOGGING_COMPILE_TIME_ENABLE 0
#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE

namespace webrtc {
namespace plotting {

namespace {

const int kNumMicrosecsPerSec = 1000000;

void SortPacketFeedbackVector(std::vector<PacketFeedback>* vec) {
  auto pred = [](const PacketFeedback& packet_feedback) {
    return packet_feedback.arrival_time_ms == PacketFeedback::kNotReceived;
  };
  vec->erase(std::remove_if(vec->begin(), vec->end(), pred), vec->end());
  std::sort(vec->begin(), vec->end(), PacketFeedbackComparator());
}

std::string SsrcToString(uint32_t ssrc) {
  std::stringstream ss;
  ss << "SSRC " << ssrc;
  return ss.str();
}

// Checks whether an SSRC is contained in the list of desired SSRCs.
// Note that an empty SSRC list matches every SSRC.
bool MatchingSsrc(uint32_t ssrc, const std::vector<uint32_t>& desired_ssrc) {
  if (desired_ssrc.size() == 0)
    return true;
  return std::find(desired_ssrc.begin(), desired_ssrc.end(), ssrc) !=
         desired_ssrc.end();
}

double AbsSendTimeToMicroseconds(int64_t abs_send_time) {
  // The timestamp is a fixed point representation with 6 bits for seconds
  // and 18 bits for fractions of a second. Thus, we divide by 2^18 to get the
  // time in seconds and then multiply by kNumMicrosecsPerSec to convert to
  // microseconds.
  static constexpr double kTimestampToMicroSec =
      static_cast<double>(kNumMicrosecsPerSec) / static_cast<double>(1ul << 18);
  return abs_send_time * kTimestampToMicroSec;
}

// Computes the difference |later| - |earlier| where |later| and |earlier|
// are counters that wrap at |modulus|. The difference is chosen to have the
// least absolute value. For example if |modulus| is 8, then the difference will
// be chosen in the range [-3, 4]. If |modulus| is 9, then the difference will
// be in [-4, 4].
int64_t WrappingDifference(uint32_t later, uint32_t earlier, int64_t modulus) {
  RTC_DCHECK_LE(1, modulus);
  RTC_DCHECK_LT(later, modulus);
  RTC_DCHECK_LT(earlier, modulus);
  int64_t difference =
      static_cast<int64_t>(later) - static_cast<int64_t>(earlier);
  int64_t max_difference = modulus / 2;
  int64_t min_difference = max_difference - modulus + 1;
  if (difference > max_difference) {
    difference -= modulus;
  }
  if (difference < min_difference) {
    difference += modulus;
  }
  if (difference > max_difference / 2 || difference < min_difference / 2) {
    RTC_LOG(LS_WARNING) << "Difference between" << later << " and " << earlier
                        << " expected to be in the range ("
                        << min_difference / 2 << "," << max_difference / 2
                        << ") but is " << difference
                        << ". Correct unwrapping is uncertain.";
  }
  return difference;
}

constexpr float kLeftMargin = 0.01f;
constexpr float kRightMargin = 0.02f;
constexpr float kBottomMargin = 0.02f;
constexpr float kTopMargin = 0.05f;

rtc::Optional<double> NetworkDelayDiff_AbsSendTime(
    const ParsedRtcEventLog::RtpPacketIncoming& old_packet,
    const ParsedRtcEventLog::RtpPacketIncoming& new_packet) {
  if (old_packet.header.extension.hasAbsoluteSendTime &&
      new_packet.header.extension.hasAbsoluteSendTime) {
    int64_t send_time_diff = WrappingDifference(
        new_packet.header.extension.absoluteSendTime,
        old_packet.header.extension.absoluteSendTime, 1ul << 24);
    int64_t recv_time_diff = new_packet.timestamp - old_packet.timestamp;
    double delay_change_us =
        recv_time_diff - AbsSendTimeToMicroseconds(send_time_diff);
    return delay_change_us / 1000;
  } else {
    return rtc::nullopt;
  }
}

rtc::Optional<double> NetworkDelayDiff_CaptureTime(
    const ParsedRtcEventLog::RtpPacketIncoming& old_packet,
    const ParsedRtcEventLog::RtpPacketIncoming& new_packet) {
  int64_t send_time_diff = WrappingDifference(
      new_packet.header.timestamp, old_packet.header.timestamp, 1ull << 32);
  int64_t recv_time_diff = new_packet.timestamp - old_packet.timestamp;

  const double kVideoSampleRate = 90000;
  // TODO(terelius): We treat all streams as video for now, even though
  // audio might be sampled at e.g. 16kHz, because it is really difficult to
  // figure out the true sampling rate of a stream. The effect is that the
  // delay will be scaled incorrectly for non-video streams.

  double delay_change =
      static_cast<double>(recv_time_diff) / 1000 -
      static_cast<double>(send_time_diff) / kVideoSampleRate * 1000;
  if (delay_change < -10000 || 10000 < delay_change) {
    RTC_LOG(LS_WARNING) << "Very large delay change. Timestamps correct?";
    RTC_LOG(LS_WARNING) << "Old capture time " << old_packet.header.timestamp
                        << ", received time " << old_packet.timestamp;
    RTC_LOG(LS_WARNING) << "New capture time " << new_packet.header.timestamp
                        << ", received time " << new_packet.timestamp;
    RTC_LOG(LS_WARNING) << "Receive time difference " << recv_time_diff << " = "
                        << static_cast<double>(recv_time_diff) /
                               kNumMicrosecsPerSec
                        << "s";
    RTC_LOG(LS_WARNING) << "Send time difference " << send_time_diff << " = "
                        << static_cast<double>(send_time_diff) /
                               kVideoSampleRate
                        << "s";
  }
  return delay_change;
}

// For each element in data, use |get_y()| to extract a y-coordinate and
// store the result in a TimeSeries.
template <typename DataType>
void ProcessPoints(
    rtc::FunctionView<rtc::Optional<float>(const DataType&)> get_y,
    const std::vector<DataType>& data,
    int64_t begin_time,
    TimeSeries* result) {
  for (size_t i = 0; i < data.size(); i++) {
    float x = static_cast<float>(data[i].timestamp - begin_time) /
              kNumMicrosecsPerSec;
    rtc::Optional<float> y = get_y(data[i]);
    if (y)
      result->points.emplace_back(x, *y);
  }
}

// For each pair of adjacent elements in |data|, use |get_y| to extract a
// y-coordinate and store the result in a TimeSeries. Note that the x-coordinate
// will be the time of the second element in the pair.
template <typename DataType, typename ResultType>
void ProcessPairs(
    rtc::FunctionView<rtc::Optional<ResultType>(const DataType&,
                                                const DataType&)> get_y,
    const std::vector<DataType>& data,
    int64_t begin_time,
    TimeSeries* result) {
  for (size_t i = 1; i < data.size(); i++) {
    float x = static_cast<float>(data[i].timestamp - begin_time) /
              kNumMicrosecsPerSec;
    rtc::Optional<ResultType> y = get_y(data[i - 1], data[i]);
    if (y)
      result->points.emplace_back(x, static_cast<float>(*y));
  }
}

// For each element in data, use |extract()| to extract a y-coordinate and
// store the result in a TimeSeries.
template <typename DataType, typename ResultType>
void AccumulatePoints(
    rtc::FunctionView<rtc::Optional<ResultType>(const DataType&)> extract,
    const std::vector<DataType>& data,
    int64_t begin_time,
    TimeSeries* result) {
  ResultType sum = 0;
  for (size_t i = 0; i < data.size(); i++) {
    float x = static_cast<float>(data[i].timestamp - begin_time) /
              kNumMicrosecsPerSec;
    rtc::Optional<ResultType> y = extract(data[i]);
    if (y) {
      sum += *y;
      result->points.emplace_back(x, static_cast<float>(sum));
    }
  }
}

// For each pair of adjacent elements in |data|, use |extract()| to extract a
// y-coordinate and store the result in a TimeSeries. Note that the x-coordinate
// will be the time of the second element in the pair.
template <typename DataType, typename ResultType>
void AccumulatePairs(
    rtc::FunctionView<rtc::Optional<ResultType>(const DataType&,
                                                const DataType&)> extract,
    const std::vector<DataType>& data,
    int64_t begin_time,
    TimeSeries* result) {
  ResultType sum = 0;
  for (size_t i = 1; i < data.size(); i++) {
    float x = static_cast<float>(data[i].timestamp - begin_time) /
              kNumMicrosecsPerSec;
    rtc::Optional<ResultType> y = extract(data[i - 1], data[i]);
    if (y)
      sum += *y;
    result->points.emplace_back(x, static_cast<float>(sum));
  }
}

// Calculates a moving average of |data| and stores the result in a TimeSeries.
// A data point is generated every |step| microseconds from |begin_time|
// to |end_time|. The value of each data point is the average of the data
// during the preceeding |window_duration_us| microseconds.
template <typename DataType, typename ResultType>
void MovingAverage(
    rtc::FunctionView<rtc::Optional<ResultType>(const DataType&)> extract,
    const std::vector<DataType>& data,
    int64_t begin_time,
    int64_t end_time,
    int64_t window_duration_us,
    int64_t step,
    webrtc::plotting::TimeSeries* result) {
  size_t window_index_begin = 0;
  size_t window_index_end = 0;
  ResultType sum_in_window = 0;

  for (int64_t t = begin_time; t < end_time + step; t += step) {
    while (window_index_end < data.size() &&
           data[window_index_end].timestamp < t) {
      rtc::Optional<ResultType> value = extract(data[window_index_end]);
      if (value)
        sum_in_window += *value;
      ++window_index_end;
    }
    while (window_index_begin < data.size() &&
           data[window_index_begin].timestamp < t - window_duration_us) {
      rtc::Optional<ResultType> value = extract(data[window_index_begin]);
      if (value)
        sum_in_window -= *value;
      ++window_index_begin;
    }
    float window_duration_s =
        static_cast<float>(window_duration_us) / kNumMicrosecsPerSec;
    float x = static_cast<float>(t - begin_time) / kNumMicrosecsPerSec;
    float y = sum_in_window / window_duration_s;
    result->points.emplace_back(x, y);
  }
}

const char kUnknownEnumValue[] = "unknown";

const char kIceCandidateTypeLocal[] = "local";
const char kIceCandidateTypeStun[] = "stun";
const char kIceCandidateTypePrflx[] = "prflx";
const char kIceCandidateTypeRelay[] = "relay";

const char kProtocolUdp[] = "udp";
const char kProtocolTcp[] = "tcp";
const char kProtocolSsltcp[] = "ssltcp";
const char kProtocolTls[] = "tls";

const char kAddressFamilyIpv4[] = "ipv4";
const char kAddressFamilyIpv6[] = "ipv6";

const char kNetworkTypeEthernet[] = "ethernet";
const char kNetworkTypeLoopback[] = "loopback";
const char kNetworkTypeWifi[] = "wifi";
const char kNetworkTypeVpn[] = "vpn";
const char kNetworkTypeCellular[] = "cellular";

std::string GetIceCandidateTypeAsString(webrtc::IceCandidateType type) {
  switch (type) {
    case webrtc::IceCandidateType::kLocal:
      return kIceCandidateTypeLocal;
    case webrtc::IceCandidateType::kStun:
      return kIceCandidateTypeStun;
    case webrtc::IceCandidateType::kPrflx:
      return kIceCandidateTypePrflx;
    case webrtc::IceCandidateType::kRelay:
      return kIceCandidateTypeRelay;
    default:
      return kUnknownEnumValue;
  }
}

std::string GetProtocolAsString(webrtc::IceCandidatePairProtocol protocol) {
  switch (protocol) {
    case webrtc::IceCandidatePairProtocol::kUdp:
      return kProtocolUdp;
    case webrtc::IceCandidatePairProtocol::kTcp:
      return kProtocolTcp;
    case webrtc::IceCandidatePairProtocol::kSsltcp:
      return kProtocolSsltcp;
    case webrtc::IceCandidatePairProtocol::kTls:
      return kProtocolTls;
    default:
      return kUnknownEnumValue;
  }
}

std::string GetAddressFamilyAsString(
    webrtc::IceCandidatePairAddressFamily family) {
  switch (family) {
    case webrtc::IceCandidatePairAddressFamily::kIpv4:
      return kAddressFamilyIpv4;
    case webrtc::IceCandidatePairAddressFamily::kIpv6:
      return kAddressFamilyIpv6;
    default:
      return kUnknownEnumValue;
  }
}

std::string GetNetworkTypeAsString(webrtc::IceCandidateNetworkType type) {
  switch (type) {
    case webrtc::IceCandidateNetworkType::kEthernet:
      return kNetworkTypeEthernet;
    case webrtc::IceCandidateNetworkType::kLoopback:
      return kNetworkTypeLoopback;
    case webrtc::IceCandidateNetworkType::kWifi:
      return kNetworkTypeWifi;
    case webrtc::IceCandidateNetworkType::kVpn:
      return kNetworkTypeVpn;
    case webrtc::IceCandidateNetworkType::kCellular:
      return kNetworkTypeCellular;
    default:
      return kUnknownEnumValue;
  }
}

std::string GetCandidatePairLogDescriptionAsString(
    const ParsedRtcEventLog::IceCandidatePairConfig& config) {
  // Example: stun:wifi->relay(tcp):cellular@udp:ipv4
  // represents a pair of a local server-reflexive candidate on a WiFi network
  // and a remote relay candidate using TCP as the relay protocol on a cell
  // network, when the candidate pair communicates over UDP using IPv4.
  std::stringstream ss;
  std::string local_candidate_type =
      GetIceCandidateTypeAsString(config.local_candidate_type);
  std::string remote_candidate_type =
      GetIceCandidateTypeAsString(config.remote_candidate_type);
  if (config.local_candidate_type == webrtc::IceCandidateType::kRelay) {
    local_candidate_type +=
        "(" + GetProtocolAsString(config.local_relay_protocol) + ")";
  }
  ss << local_candidate_type << ":"
     << GetNetworkTypeAsString(config.local_network_type) << ":"
     << GetAddressFamilyAsString(config.local_address_family) << "->"
     << remote_candidate_type << ":"
     << GetAddressFamilyAsString(config.remote_address_family) << "@"
     << GetProtocolAsString(config.candidate_pair_protocol);
  return ss.str();
}
}  // namespace

EventLogAnalyzer::EventLogAnalyzer(const ParsedRtcEventLog& log)
    : parsed_log_(log), window_duration_(250000), step_(10000) {
  begin_time_ = parsed_log_.first_timestamp();
  end_time_ = parsed_log_.last_timestamp();
  if (end_time_ < begin_time_) {
    RTC_LOG(LS_WARNING) << "No useful events in the log.";
    begin_time_ = end_time_ = 0;
  }
  call_duration_s_ = ToCallTime(end_time_);

  const auto& log_start_events = parsed_log_.start_log_events();
  const auto& log_end_events = parsed_log_.stop_log_events();
  auto start_iter = log_start_events.begin();
  auto end_iter = log_end_events.begin();
  while (start_iter != log_start_events.end()) {
    int64_t start = start_iter->timestamp;
    ++start_iter;
    rtc::Optional<int64_t> next_start;
    if (start_iter != log_start_events.end())
      next_start.emplace(start_iter->timestamp);
    if (end_iter != log_end_events.end() &&
        end_iter->timestamp <=
            next_start.value_or(std::numeric_limits<int64_t>::max())) {
      int64_t end = end_iter->timestamp;
      RTC_DCHECK_LE(start, end);
      log_segments_.push_back(std::make_pair(start, end));
      ++end_iter;
    } else {
      // we're missing an end event. Assume that it occurred just before the
      // next start.
      log_segments_.push_back(
          std::make_pair(start, next_start.value_or(end_time_)));
    }
  }
  RTC_LOG(LS_INFO) << "Found " << log_segments_.size()
               << " (LOG_START, LOG_END) segments in log.";
}

class BitrateObserver : public NetworkChangedObserver,
                        public RemoteBitrateObserver {
 public:
  BitrateObserver() : last_bitrate_bps_(0), bitrate_updated_(false) {}

  void OnNetworkChanged(uint32_t bitrate_bps,
                        uint8_t fraction_lost,
                        int64_t rtt_ms,
                        int64_t probing_interval_ms) override {
    last_bitrate_bps_ = bitrate_bps;
    bitrate_updated_ = true;
  }

  void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                               uint32_t bitrate) override {}

  uint32_t last_bitrate_bps() const { return last_bitrate_bps_; }
  bool GetAndResetBitrateUpdated() {
    bool bitrate_updated = bitrate_updated_;
    bitrate_updated_ = false;
    return bitrate_updated;
  }

 private:
  uint32_t last_bitrate_bps_;
  bool bitrate_updated_;
};

template <typename Direction>
// This is much more reliable for outgoing streams than for incoming streams.
rtc::Optional<uint32_t> EventLogAnalyzer::EstimateRtpClockFrequency(
    const std::vector<typename Direction::RtpPacketType>& packets) const {
  RTC_CHECK(packets.size() >= 2);
  int64_t end_time_us = log_segments_.empty()
                            ? std::numeric_limits<int64_t>::max()
                            : log_segments_.front().second;
  SeqNumUnwrapper<uint32_t> unwrapper;
  uint64_t first_rtp_timestamp = unwrapper.Unwrap(packets[0].header.timestamp);
  int64_t first_log_timestamp = packets[0].timestamp;
  uint64_t last_rtp_timestamp = first_rtp_timestamp;
  int64_t last_log_timestamp = first_log_timestamp;
  for (size_t i = 1; i < packets.size(); i++) {
    if (packets[i].timestamp > end_time_us)
      break;
    last_rtp_timestamp = unwrapper.Unwrap(packets[i].header.timestamp);
    last_log_timestamp = packets[i].timestamp;
  }
  if (last_log_timestamp - first_log_timestamp < kNumMicrosecsPerSec) {
    RTC_LOG(LS_WARNING)
        << "Failed to estimate RTP clock frequency: Stream too short. ("
        << packets.size() << " packets, "
        << last_log_timestamp - first_log_timestamp << " us)";
    return rtc::nullopt;
  }
  double duration =
      static_cast<double>(last_log_timestamp - first_log_timestamp) /
      kNumMicrosecsPerSec;
  double estimated_frequency =
      (last_rtp_timestamp - first_rtp_timestamp) / duration;
  for (uint32_t f : {8000, 16000, 32000, 48000, 90000}) {
    if (std::fabs(estimated_frequency - f) < 0.05 * f) {
      return f;
    }
  }
  RTC_LOG(LS_WARNING) << "Failed to estimate RTP clock frequency: Estimate "
                      << estimated_frequency
                      << "not close to any stardard RTP frequency.";
  return rtc::nullopt;
}

float EventLogAnalyzer::ToCallTime(int64_t timestamp) const {
  return static_cast<float>(timestamp - begin_time_) / kNumMicrosecsPerSec;
}

template <typename Direction>
void EventLogAnalyzer::CreatePacketGraph(Plot* plot) {
  using RtpPacketType = typename Direction::RtpPacketType;
  for (auto& kv : parsed_log_.rtp_packets<Direction>()) {
    uint32_t ssrc = kv.first;
    const std::vector<RtpPacketType>& packet_stream = kv.second;
    // Filter on SSRC.
    if (!MatchingSsrc(ssrc, desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(GetStreamName<Direction>(ssrc), LineStyle::kBar);
    ProcessPoints<RtpPacketType>(
        [](const RtpPacketType& packet) {
          return rtc::Optional<float>(packet.total_length);
        },
        packet_stream, begin_time_, &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Packet size (bytes)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle(std::string(Direction::full_name) + " RTP packets");
}
// Force instantiation of the template for both Incoming and Outgoing packets.
template void EventLogAnalyzer::CreatePacketGraph<Incoming>(Plot*);
template void EventLogAnalyzer::CreatePacketGraph<Outgoing>(Plot*);

template <typename T>
void EventLogAnalyzer::CreateAccumulatedPacketsTimeSeries(
    Plot* plot,
    const std::vector<T>& packets,
    const std::string& label) {
  TimeSeries time_series(label, LineStyle::kStep);
  for (size_t i = 0; i < packets.size(); i++) {
    float x = ToCallTime(packets[i].timestamp);
    time_series.points.emplace_back(x, i + 1);
  }
  plot->AppendTimeSeries(std::move(time_series));
}

template <typename Direction>
void EventLogAnalyzer::CreateAccumulatedPacketsGraph(Plot* plot) {
  using RtpPacketType = typename Direction::RtpPacketType;
  for (const auto& kv : parsed_log_.rtp_packets<Direction>()) {
    uint32_t ssrc = kv.first;
    const std::vector<RtpPacketType>& packets = kv.second;
    if (!MatchingSsrc(ssrc, desired_ssrc_))
      continue;
    std::string label = std::string("RTP ") + GetStreamName<Direction>(ssrc);
    CreateAccumulatedPacketsTimeSeries(plot, packets, label);
  }
  std::string label = std::string("RTCP ") + "(" + Direction::name + ")";
  CreateAccumulatedPacketsTimeSeries(
      plot, parsed_log_.rtcp_packets<Direction>(), label);

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Received Packets", kBottomMargin, kTopMargin);
  plot->SetTitle(std::string("Accumulated ") + Direction::full_name +
                 " RTP/RTCP packets");
}
// Force instantiation of the template for both Incoming and Outgoing packets.
template void EventLogAnalyzer::CreateAccumulatedPacketsGraph<Incoming>(Plot*);
template void EventLogAnalyzer::CreateAccumulatedPacketsGraph<Outgoing>(Plot*);

// For each SSRC, plot the time between the consecutive playouts.
void EventLogAnalyzer::CreatePlayoutGraph(Plot* plot) {
  for (const auto& playout_stream : parsed_log_.audio_playout_events()) {
    uint32_t ssrc = playout_stream.first;
    if (!MatchingSsrc(ssrc, desired_ssrc_))
      continue;
    rtc::Optional<int64_t> last_playout;
    TimeSeries time_series(SsrcToString(ssrc), LineStyle::kBar);
    for (const auto& playout_time : playout_stream.second) {
      float x = ToCallTime(playout_time);
      // If there were no previous playouts, place the point on the x-axis.
      float y = static_cast<float>(playout_time -
                                   last_playout.value_or(playout_time)) /
                1000;
      time_series.points.push_back(TimeSeriesPoint(x, y));
      last_playout.emplace(playout_time);
    }
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Time since last playout (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Audio playout");
}

// For audio SSRCs, plot the audio level.
template <typename Direction>
void EventLogAnalyzer::CreateAudioLevelGraph(Plot* plot) {
  using RtpPacketType = typename Direction::RtpPacketType;
  for (auto& kv : parsed_log_.rtp_packets<Direction>()) {
    uint32_t ssrc = kv.first;
    const std::vector<RtpPacketType>& packets = kv.second;
    if (!IsAudioSsrc<Direction>(ssrc))
      continue;
    TimeSeries time_series(GetStreamName<Direction>(ssrc), LineStyle::kLine);
    for (auto& packet : packets) {
      if (packet.header.extension.hasAudioLevel) {
        float x = ToCallTime(packet.timestamp);
        // The audio level is stored in -dBov (so e.g. -10 dBov is stored as 10)
        // Here we convert it to dBov.
        float y = static_cast<float>(-packet.header.extension.audioLevel);
        time_series.points.emplace_back(TimeSeriesPoint(x, y));
      }
    }
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetYAxis(-127, 0, "Audio level (dBov)", kBottomMargin,
                 kTopMargin);
  plot->SetTitle(std::string(Direction::full_name) + " audio level");
}
// Force instantiation of the template for both Incoming and Outgoing packets.
template void EventLogAnalyzer::CreateAudioLevelGraph<Incoming>(Plot*);
template void EventLogAnalyzer::CreateAudioLevelGraph<Outgoing>(Plot*);

// For each SSRC, plot the time between the consecutive playouts.
void EventLogAnalyzer::CreateSequenceNumberGraph(Plot* plot) {
  for (auto& kv : parsed_log_.rtp_packets<Incoming>()) {
    uint32_t ssrc = kv.first;
    const std::vector<ParsedRtcEventLog::RtpPacketIncoming>& packets =
        kv.second;
    // Filter on direction and SSRC.
    if (!MatchingSsrc(ssrc, desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(GetStreamName<Incoming>(ssrc), LineStyle::kBar);
    ProcessPairs<ParsedRtcEventLog::RtpPacketIncoming, float>(
        [](const ParsedRtcEventLog::RtpPacketIncoming& old_packet,
           const ParsedRtcEventLog::RtpPacketIncoming& new_packet) {
          int64_t diff =
              WrappingDifference(new_packet.header.sequenceNumber,
                                 old_packet.header.sequenceNumber, 1ul << 16);
          return diff;
        },
        packets, begin_time_, &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Difference since last packet", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Sequence number");
}

void EventLogAnalyzer::CreateIncomingPacketLossGraph(Plot* plot) {
  for (auto& kv : parsed_log_.rtp_packets<Incoming>()) {
    uint32_t ssrc = kv.first;
    const std::vector<ParsedRtcEventLog::RtpPacketIncoming>& packet_stream =
        kv.second;
    // Filter on direction and SSRC.
    if (!MatchingSsrc(ssrc, desired_ssrc_) || packet_stream.size() == 0) {
      continue;
    }

    TimeSeries time_series(GetStreamName<Incoming>(ssrc), LineStyle::kLine,
                           PointStyle::kHighlight);
    // TODO(terelius): Should the window and step size be read from the class
    // instead?
    const int64_t kWindowUs = 1000000;
    const int64_t kStep = 1000000;
    SeqNumUnwrapper<uint16_t> unwrapper_;
    SeqNumUnwrapper<uint16_t> prior_unwrapper_;
    size_t window_index_begin = 0;
    size_t window_index_end = 0;
    uint64_t highest_seq_number =
        unwrapper_.Unwrap(packet_stream[0].header.sequenceNumber) - 1;
    uint64_t highest_prior_seq_number =
        prior_unwrapper_.Unwrap(packet_stream[0].header.sequenceNumber) - 1;

    for (int64_t t = begin_time_; t < end_time_ + kStep; t += kStep) {
      while (window_index_end < packet_stream.size() &&
             packet_stream[window_index_end].timestamp < t) {
        uint64_t sequence_number = unwrapper_.Unwrap(
            packet_stream[window_index_end].header.sequenceNumber);
        highest_seq_number = std::max(highest_seq_number, sequence_number);
        ++window_index_end;
      }
      while (window_index_begin < packet_stream.size() &&
             packet_stream[window_index_begin].timestamp < t - kWindowUs) {
        uint64_t sequence_number = prior_unwrapper_.Unwrap(
            packet_stream[window_index_begin].header.sequenceNumber);
        highest_prior_seq_number =
            std::max(highest_prior_seq_number, sequence_number);
        ++window_index_begin;
      }
      float x = ToCallTime(t);
      uint64_t expected_packets = highest_seq_number - highest_prior_seq_number;
      if (expected_packets > 0) {
        int64_t received_packets = window_index_end - window_index_begin;
        int64_t lost_packets = expected_packets - received_packets;
        float y = static_cast<float>(lost_packets) / expected_packets * 100;
        time_series.points.emplace_back(x, y);
      }
    }
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Estimated loss rate (%)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Estimated incoming loss rate");
}

void EventLogAnalyzer::CreateIncomingDelayDeltaGraph(Plot* plot) {
  for (auto& kv : parsed_log_.rtp_packets<Incoming>()) {
    uint32_t ssrc = kv.first;
    const std::vector<ParsedRtcEventLog::RtpPacketIncoming>& packets =
        kv.second;
    // Filter on direction and SSRC.
    if (!MatchingSsrc(ssrc, desired_ssrc_) || IsAudioSsrc<Incoming>(ssrc) ||
        !IsVideoSsrc<Incoming>(ssrc) || IsRtxSsrc<Incoming>(ssrc)) {
      continue;
    }

    TimeSeries capture_time_data(
        GetStreamName<Incoming>(ssrc) + " capture-time", LineStyle::kBar);
    ProcessPairs<ParsedRtcEventLog::RtpPacketIncoming, double>(
        NetworkDelayDiff_CaptureTime, packets, begin_time_, &capture_time_data);
    plot->AppendTimeSeries(std::move(capture_time_data));

    TimeSeries send_time_data(GetStreamName<Incoming>(ssrc) + " abs-send-time",
                              LineStyle::kBar);
    ProcessPairs<ParsedRtcEventLog::RtpPacketIncoming, double>(
        NetworkDelayDiff_AbsSendTime, packets, begin_time_, &send_time_data);
    plot->AppendTimeSeries(std::move(send_time_data));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Latency change (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Network latency difference between consecutive packets");
}

void EventLogAnalyzer::CreateIncomingDelayGraph(Plot* plot) {
  for (auto& kv : parsed_log_.rtp_packets<Incoming>()) {
    uint32_t ssrc = kv.first;
    const std::vector<ParsedRtcEventLog::RtpPacketIncoming>& packets =
        kv.second;
    // Filter on direction and SSRC.
    if (!MatchingSsrc(ssrc, desired_ssrc_) || IsAudioSsrc<Incoming>(ssrc) ||
        !IsVideoSsrc<Incoming>(ssrc) || IsRtxSsrc<Incoming>(ssrc)) {
      continue;
    }

    TimeSeries capture_time_data(
        GetStreamName<Incoming>(ssrc) + " capture-time", LineStyle::kLine);
    AccumulatePairs<ParsedRtcEventLog::RtpPacketIncoming, double>(
        NetworkDelayDiff_CaptureTime, packets, begin_time_, &capture_time_data);
    plot->AppendTimeSeries(std::move(capture_time_data));

    TimeSeries send_time_data(GetStreamName<Incoming>(ssrc) + " abs-send-time",
                              LineStyle::kLine);
    AccumulatePairs<ParsedRtcEventLog::RtpPacketIncoming, double>(
        NetworkDelayDiff_AbsSendTime, packets, begin_time_, &send_time_data);
    plot->AppendTimeSeries(std::move(send_time_data));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Latency change (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Network latency (relative to first packet)");
}

// Plot the fraction of packets lost (as perceived by the loss-based BWE).
void EventLogAnalyzer::CreateFractionLossGraph(Plot* plot) {
  TimeSeries time_series("Fraction lost", LineStyle::kLine,
                         PointStyle::kHighlight);
  for (auto& bwe_update : parsed_log_.bwe_loss_updates()) {
    float x = ToCallTime(bwe_update.timestamp);
    float y = static_cast<float>(bwe_update.fraction_lost) / 255 * 100;
    time_series.points.emplace_back(x, y);
  }

  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Percent lost packets", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Reported packet loss");
}

// Plot the total bandwidth used by all RTP streams.
void EventLogAnalyzer::CreateTotalIncomingBitrateGraph(Plot* plot) {
  using RtpPacketType = Incoming::RtpPacketType;

  // TODO(terelius): This could be provided by the parser.
  std::multimap<int64_t, size_t> packets2;
  for (const auto& kv : parsed_log_.rtp_packets<Incoming>()) {
    for (const RtpPacketType& packet : kv.second)
      packets2.insert(std::make_pair(packet.timestamp, packet.total_length));
  }

  auto window_begin = packets2.begin();
  auto window_end = packets2.begin();
  size_t bytes_in_window = 0;

  // Calculate a moving average of the bitrate and store in a TimeSeries.
  TimeSeries bitrate_series("Bitrate", LineStyle::kLine);
  for (int64_t time = begin_time_; time < end_time_ + step_; time += step_) {
    while (window_end != packets2.end() && window_end->first < time) {
      bytes_in_window += window_end->second;
      ++window_end;
    }
    while (window_begin != packets2.end() &&
           window_begin->first < time - window_duration_) {
      RTC_DCHECK_LE(window_begin->second, bytes_in_window);
      bytes_in_window -= window_begin->second;
      ++window_begin;
    }
    float window_duration_in_seconds =
        static_cast<float>(window_duration_) / kNumMicrosecsPerSec;
    float x = ToCallTime(time);
    float y = bytes_in_window * 8 / window_duration_in_seconds / 1000;
    bitrate_series.points.emplace_back(x, y);
  }
  plot->AppendTimeSeries(std::move(bitrate_series));

  // Overlay the outgoing REMB over incoming bitrate.
  TimeSeries remb_series("Remb", LineStyle::kStep);
  for (const auto& rtcp : parsed_log_.remb<Outgoing>()) {
    float x = ToCallTime(rtcp.timestamp);
    float y = static_cast<float>(rtcp.remb.bitrate_bps()) / 1000;
    remb_series.points.emplace_back(x, y);
  }
  plot->AppendTimeSeriesIfNotEmpty(std::move(remb_series));

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Incoming RTP bitrate");
}

// Plot the total bandwidth used by all RTP streams.
void EventLogAnalyzer::CreateTotalOutgoingBitrateGraph(Plot* plot,
                                                       bool show_detector_state,
                                                       bool show_alr_state) {
  using RtpPacketType = Outgoing::RtpPacketType;

  // TODO(terelius): This could be provided by the parser.
  std::multimap<int64_t, size_t> packets2;
  for (const auto& kv : parsed_log_.rtp_packets<Outgoing>()) {
    for (const RtpPacketType& packet : kv.second)
      packets2.insert(std::make_pair(packet.timestamp, packet.total_length));
  }

  auto window_begin = packets2.begin();
  auto window_end = packets2.begin();
  size_t bytes_in_window = 0;

  // Calculate a moving average of the bitrate and store in a TimeSeries.
  TimeSeries bitrate_series("Bitrate", LineStyle::kLine);
  for (int64_t time = begin_time_; time < end_time_ + step_; time += step_) {
    while (window_end != packets2.end() && window_end->first < time) {
      bytes_in_window += window_end->second;
      ++window_end;
    }
    while (window_begin != packets2.end() &&
           window_begin->first < time - window_duration_) {
      RTC_DCHECK_LE(window_begin->second, bytes_in_window);
      bytes_in_window -= window_begin->second;
      ++window_begin;
    }
    float window_duration_in_seconds =
        static_cast<float>(window_duration_) / kNumMicrosecsPerSec;
    float x = ToCallTime(time);
    float y = bytes_in_window * 8 / window_duration_in_seconds / 1000;
    bitrate_series.points.emplace_back(x, y);
  }
  plot->AppendTimeSeries(std::move(bitrate_series));

  // Overlay the send-side bandwidth estimate over the outgoing bitrate.
  TimeSeries loss_series("Loss-based estimate", LineStyle::kStep);
  for (auto& loss_update : parsed_log_.bwe_loss_updates()) {
    float x = ToCallTime(loss_update.timestamp);
    float y = static_cast<float>(loss_update.new_bitrate) / 1000;
    loss_series.points.emplace_back(x, y);
  }

  TimeSeries delay_series("Delay-based estimate", LineStyle::kStep);
  IntervalSeries overusing_series("Overusing", "#ff8e82",
                                  IntervalSeries::kHorizontal);
  IntervalSeries underusing_series("Underusing", "#5092fc",
                                   IntervalSeries::kHorizontal);
  IntervalSeries normal_series("Normal", "#c4ffc4",
                               IntervalSeries::kHorizontal);
  IntervalSeries* last_series = &normal_series;
  double last_detector_switch = 0.0;

  BandwidthUsage last_detector_state = BandwidthUsage::kBwNormal;

  for (auto& delay_update : parsed_log_.bwe_delay_updates()) {
    float x = ToCallTime(delay_update.timestamp);
    float y = static_cast<float>(delay_update.bitrate_bps) / 1000;

    if (last_detector_state != delay_update.detector_state) {
      last_series->intervals.emplace_back(last_detector_switch, x);
      last_detector_state = delay_update.detector_state;
      last_detector_switch = x;

      switch (delay_update.detector_state) {
        case BandwidthUsage::kBwNormal:
          last_series = &normal_series;
          break;
        case BandwidthUsage::kBwUnderusing:
          last_series = &underusing_series;
          break;
        case BandwidthUsage::kBwOverusing:
          last_series = &overusing_series;
          break;
        case BandwidthUsage::kLast:
          RTC_NOTREACHED();
      }
    }

    delay_series.points.emplace_back(x, y);
  }

  RTC_CHECK(last_series);
  last_series->intervals.emplace_back(last_detector_switch, end_time_);

  TimeSeries created_series("Probe cluster created.", LineStyle::kNone,
                            PointStyle::kHighlight);
  for (auto& cluster : parsed_log_.bwe_probe_cluster_created_events()) {
    float x = ToCallTime(cluster.timestamp);
    float y = static_cast<float>(cluster.bitrate_bps) / 1000;
    created_series.points.emplace_back(x, y);
  }

  TimeSeries result_series("Probing results.", LineStyle::kNone,
                           PointStyle::kHighlight);
  for (auto& result : parsed_log_.bwe_probe_result_events()) {
    if (result.bitrate_bps) {
      float x = ToCallTime(result.timestamp);
      float y = static_cast<float>(*result.bitrate_bps) / 1000;
      result_series.points.emplace_back(x, y);
    }
  }

  IntervalSeries alr_state("ALR", "#555555", IntervalSeries::kHorizontal);
  bool previously_in_alr = false;
  int64_t alr_start = 0;
  for (auto& alr : parsed_log_.alr_state_events()) {
    float y = ToCallTime(alr.timestamp);
    if (!previously_in_alr && alr.in_alr) {
      alr_start = alr.timestamp;
      previously_in_alr = true;
    } else if (previously_in_alr && !alr.in_alr) {
      float x = ToCallTime(alr_start);
      alr_state.intervals.emplace_back(x, y);
      previously_in_alr = false;
    }
  }

  if (previously_in_alr) {
    float x = ToCallTime(alr_start);
    float y = ToCallTime(end_time_);
    alr_state.intervals.emplace_back(x, y);
  }

  if (show_detector_state) {
    plot->AppendIntervalSeries(std::move(overusing_series));
    plot->AppendIntervalSeries(std::move(underusing_series));
    plot->AppendIntervalSeries(std::move(normal_series));
  }

  if (show_alr_state) {
    plot->AppendIntervalSeries(std::move(alr_state));
  }
  plot->AppendTimeSeries(std::move(loss_series));
  plot->AppendTimeSeries(std::move(delay_series));
  plot->AppendTimeSeries(std::move(created_series));
  plot->AppendTimeSeries(std::move(result_series));

  // Overlay the incoming REMB over the outgoing bitrate.
  TimeSeries remb_series("Remb", LineStyle::kStep);
  for (const auto& rtcp : parsed_log_.remb<Incoming>()) {
    float x = ToCallTime(rtcp.timestamp);
    float y = static_cast<float>(rtcp.remb.bitrate_bps()) / 1000;
    remb_series.points.emplace_back(x, y);
  }
  plot->AppendTimeSeriesIfNotEmpty(std::move(remb_series));

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Outgoing RTP bitrate");
}

// For each SSRC, plot the bandwidth used by that stream.
template <typename Direction>
void EventLogAnalyzer::CreateStreamBitrateGraph(Plot* plot) {
  using RtpPacketType = typename Direction::RtpPacketType;
  for (auto& kv : parsed_log_.rtp_packets<Direction>()) {
    uint32_t ssrc = kv.first;
    const std::vector<RtpPacketType>& packet_stream = kv.second;
    // Filter on direction and SSRC.
    if (!MatchingSsrc(ssrc, desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series(GetStreamName<Direction>(ssrc), LineStyle::kLine);
    MovingAverage<RtpPacketType, double>(
        [](const RtpPacketType& packet) {
          return packet.total_length * 8.0 / 1000.0;
        },
        packet_stream, begin_time_, end_time_, window_duration_, step_,
        &time_series);
    plot->AppendTimeSeries(std::move(time_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle(std::string(Direction::full_name) + " bitrate per stream");
}
// Force instantiation of the template for both Incoming and Outgoing packets.
template void EventLogAnalyzer::CreateStreamBitrateGraph<Incoming>(Plot*);
template void EventLogAnalyzer::CreateStreamBitrateGraph<Outgoing>(Plot*);

void EventLogAnalyzer::CreateSendSideBweSimulationGraph(Plot* plot) {
  using RtpPacketType = typename Outgoing::RtpPacketType;
  using TransportFeedbackType =
      typename ParsedRtcEventLog::RtcpPacketTransportFeedback;

  // TODO(terelius): This could be provided by the parser.
  std::multimap<int64_t, const RtpPacketType*> outgoing_rtp;
  for (const auto& kv : parsed_log_.rtp_packets<Outgoing>()) {
    for (const RtpPacketType& rtp_packet : kv.second)
      outgoing_rtp.insert(std::make_pair(rtp_packet.timestamp, &rtp_packet));
  }

  const std::vector<TransportFeedbackType>& incoming_rtcp =
      parsed_log_.transport_feedbacks<Incoming>();

  SimulatedClock clock(0);
  BitrateObserver observer;
  RtcEventLogNullImpl null_event_log;
  PacketRouter packet_router;
  PacedSender pacer(&clock, &packet_router, &null_event_log);
  SendSideCongestionController cc(&clock, &observer, &null_event_log, &pacer);
  // TODO(holmer): Log the call config and use that here instead.
  static const uint32_t kDefaultStartBitrateBps = 300000;
  cc.SetBweBitrates(0, kDefaultStartBitrateBps, -1);

  TimeSeries time_series("Delay-based estimate", LineStyle::kStep,
                         PointStyle::kHighlight);
  TimeSeries acked_time_series("Acked bitrate", LineStyle::kLine,
                               PointStyle::kHighlight);
  TimeSeries acked_estimate_time_series(
      "Acked bitrate estimate", LineStyle::kLine, PointStyle::kHighlight);

  auto rtp_iterator = outgoing_rtp.begin();
  auto rtcp_iterator = incoming_rtcp.begin();

  auto NextRtpTime = [&]() {
    if (rtp_iterator != outgoing_rtp.end())
      return static_cast<int64_t>(rtp_iterator->first);
    return std::numeric_limits<int64_t>::max();
  };

  auto NextRtcpTime = [&]() {
    if (rtcp_iterator != incoming_rtcp.end())
      return static_cast<int64_t>(rtcp_iterator->timestamp);
    return std::numeric_limits<int64_t>::max();
  };

  auto NextProcessTime = [&]() {
    if (rtcp_iterator != incoming_rtcp.end() ||
        rtp_iterator != outgoing_rtp.end()) {
      return clock.TimeInMicroseconds() +
             std::max<int64_t>(cc.TimeUntilNextProcess() * 1000, 0);
    }
    return std::numeric_limits<int64_t>::max();
  };

  RateStatistics acked_bitrate(250, 8000);
#if !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
  // The event_log_visualizer should normally not be compiled with
  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE since the normal plots won't work.
  // However, compiling with BWE_TEST_LOGGING, runnning with --plot_sendside_bwe
  // and piping the output to plot_dynamics.py can be used as a hack to get the
  // internal state of various BWE components. In this case, it is important
  // we don't instantiate the AcknowledgedBitrateEstimator both here and in
  // SendSideCongestionController since that would lead to duplicate outputs.
  AcknowledgedBitrateEstimator acknowledged_bitrate_estimator(
      rtc::MakeUnique<BitrateEstimator>());
#endif  // !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
  int64_t time_us = std::min(NextRtpTime(), NextRtcpTime());
  int64_t last_update_us = 0;
  while (time_us != std::numeric_limits<int64_t>::max()) {
    clock.AdvanceTimeMicroseconds(time_us - clock.TimeInMicroseconds());
    if (clock.TimeInMicroseconds() >= NextRtcpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtcpTime());
      cc.OnTransportFeedback(rtcp_iterator->transport_feedback);
      std::vector<PacketFeedback> feedback = cc.GetTransportFeedbackVector();
      SortPacketFeedbackVector(&feedback);
      rtc::Optional<uint32_t> bitrate_bps;
      if (!feedback.empty()) {
#if !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
        acknowledged_bitrate_estimator.IncomingPacketFeedbackVector(feedback);
#endif  // !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
        for (const PacketFeedback& packet : feedback)
          acked_bitrate.Update(packet.payload_size, packet.arrival_time_ms);
        bitrate_bps = acked_bitrate.Rate(feedback.back().arrival_time_ms);
      }
      float x = ToCallTime(clock.TimeInMicroseconds());
      float y = bitrate_bps.value_or(0) / 1000;
      acked_time_series.points.emplace_back(x, y);
#if !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
      y = acknowledged_bitrate_estimator.bitrate_bps().value_or(0) / 1000;
      acked_estimate_time_series.points.emplace_back(x, y);
#endif  // !(BWE_TEST_LOGGING_COMPILE_TIME_ENABLE)
      ++rtcp_iterator;
    }
    if (clock.TimeInMicroseconds() >= NextRtpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtpTime());
      const RtpPacketType& rtp = *rtp_iterator->second;
      if (rtp.header.extension.hasTransportSequenceNumber) {
        RTC_DCHECK(rtp.header.extension.hasTransportSequenceNumber);
        cc.AddPacket(rtp.header.ssrc,
                     rtp.header.extension.transportSequenceNumber,
                     rtp.total_length, PacedPacketInfo());
        rtc::SentPacket sent_packet(
            rtp.header.extension.transportSequenceNumber, rtp.timestamp / 1000);
        cc.OnSentPacket(sent_packet);
      }
      ++rtp_iterator;
    }
    if (clock.TimeInMicroseconds() >= NextProcessTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextProcessTime());
      cc.Process();
    }
    if (observer.GetAndResetBitrateUpdated() ||
        time_us - last_update_us >= 1e6) {
      uint32_t y = observer.last_bitrate_bps() / 1000;
      float x = ToCallTime(clock.TimeInMicroseconds());
      time_series.points.emplace_back(x, y);
      last_update_us = time_us;
    }
    time_us = std::min({NextRtpTime(), NextRtcpTime(), NextProcessTime()});
  }
  // Add the data set to the plot.
  plot->AppendTimeSeries(std::move(time_series));
  plot->AppendTimeSeries(std::move(acked_time_series));
  plot->AppendTimeSeriesIfNotEmpty(std::move(acked_estimate_time_series));

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Simulated send-side BWE behavior");
}

void EventLogAnalyzer::CreateReceiveSideBweSimulationGraph(Plot* plot) {
  using RtpPacketType = typename Incoming::RtpPacketType;
  class RembInterceptingPacketRouter : public PacketRouter {
   public:
    void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                                 uint32_t bitrate_bps) override {
      last_bitrate_bps_ = bitrate_bps;
      bitrate_updated_ = true;
      PacketRouter::OnReceiveBitrateChanged(ssrcs, bitrate_bps);
    }
    uint32_t last_bitrate_bps() const { return last_bitrate_bps_; }
    bool GetAndResetBitrateUpdated() {
      bool bitrate_updated = bitrate_updated_;
      bitrate_updated_ = false;
      return bitrate_updated;
    }

   private:
    uint32_t last_bitrate_bps_;
    bool bitrate_updated_;
  };

  std::multimap<int64_t, const RtpPacketType*> incoming_rtp;

  for (const auto& kv : parsed_log_.rtp_packets<Incoming>()) {
    if (IsVideoSsrc<Incoming>(kv.first)) {
      for (const auto& rtp_packet : kv.second)
        incoming_rtp.insert(std::make_pair(rtp_packet.timestamp, &rtp_packet));
    }
  }

  SimulatedClock clock(0);
  RembInterceptingPacketRouter packet_router;
  // TODO(terelius): The PacketRouter is used as the RemoteBitrateObserver.
  // Is this intentional?
  ReceiveSideCongestionController rscc(&clock, &packet_router);
  // TODO(holmer): Log the call config and use that here instead.
  // static const uint32_t kDefaultStartBitrateBps = 300000;
  // rscc.SetBweBitrates(0, kDefaultStartBitrateBps, -1);

  TimeSeries time_series("Receive side estimate", LineStyle::kLine,
                         PointStyle::kHighlight);
  TimeSeries acked_time_series("Received bitrate", LineStyle::kLine);

  RateStatistics acked_bitrate(250, 8000);
  int64_t last_update_us = 0;
  for (const auto& kv : incoming_rtp) {
    const RtpPacketType& packet = *kv.second;
    int64_t arrival_time_ms = packet.timestamp / 1000;
    size_t payload = packet.total_length; /*Should subtract header?*/
    clock.AdvanceTimeMicroseconds(packet.timestamp -
                                  clock.TimeInMicroseconds());
    rscc.OnReceivedPacket(arrival_time_ms, payload, packet.header);
    acked_bitrate.Update(payload, arrival_time_ms);
    rtc::Optional<uint32_t> bitrate_bps = acked_bitrate.Rate(arrival_time_ms);
    if (bitrate_bps) {
      uint32_t y = *bitrate_bps / 1000;
      float x = ToCallTime(clock.TimeInMicroseconds());
      acked_time_series.points.emplace_back(x, y);
    }
    if (packet_router.GetAndResetBitrateUpdated() ||
        clock.TimeInMicroseconds() - last_update_us >= 1e6) {
      uint32_t y = packet_router.last_bitrate_bps() / 1000;
      float x = ToCallTime(clock.TimeInMicroseconds());
      time_series.points.emplace_back(x, y);
      last_update_us = clock.TimeInMicroseconds();
    }
  }
  // Add the data set to the plot.
  plot->AppendTimeSeries(std::move(time_series));
  plot->AppendTimeSeries(std::move(acked_time_series));

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Simulated receive-side BWE behavior");
}

void EventLogAnalyzer::CreateNetworkDelayFeedbackGraph(Plot* plot) {
  using RtpPacketType = typename Outgoing::RtpPacketType;
  using TransportFeedbackType =
      typename ParsedRtcEventLog::RtcpPacketTransportFeedback;

  // TODO(terelius): This could be provided by the parser.
  std::multimap<int64_t, const RtpPacketType*> outgoing_rtp;
  for (const auto& kv : parsed_log_.rtp_packets<Outgoing>()) {
    for (const RtpPacketType& rtp_packet : kv.second)
      outgoing_rtp.insert(std::make_pair(rtp_packet.timestamp, &rtp_packet));
  }

  const std::vector<TransportFeedbackType>& incoming_rtcp =
      parsed_log_.transport_feedbacks<Incoming>();

  SimulatedClock clock(0);
  TransportFeedbackAdapter feedback_adapter(&clock);

  TimeSeries late_feedback_series("Late feedback results.", LineStyle::kNone,
                                  PointStyle::kHighlight);
  TimeSeries time_series("Network Delay Change", LineStyle::kLine,
                         PointStyle::kHighlight);
  int64_t estimated_base_delay_ms = std::numeric_limits<int64_t>::max();

  auto rtp_iterator = outgoing_rtp.begin();
  auto rtcp_iterator = incoming_rtcp.begin();

  auto NextRtpTime = [&]() {
    if (rtp_iterator != outgoing_rtp.end())
      return static_cast<int64_t>(rtp_iterator->first);
    return std::numeric_limits<int64_t>::max();
  };

  auto NextRtcpTime = [&]() {
    if (rtcp_iterator != incoming_rtcp.end())
      return static_cast<int64_t>(rtcp_iterator->timestamp);
    return std::numeric_limits<int64_t>::max();
  };

  int64_t time_us = std::min(NextRtpTime(), NextRtcpTime());
  int64_t prev_y = 0;
  while (time_us != std::numeric_limits<int64_t>::max()) {
    clock.AdvanceTimeMicroseconds(time_us - clock.TimeInMicroseconds());
    if (clock.TimeInMicroseconds() >= NextRtcpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtcpTime());
      feedback_adapter.OnTransportFeedback(rtcp_iterator->transport_feedback);
      std::vector<PacketFeedback> feedback =
          feedback_adapter.GetTransportFeedbackVector();
      SortPacketFeedbackVector(&feedback);
      for (const PacketFeedback& packet : feedback) {
        float x = ToCallTime(clock.TimeInMicroseconds());
        if (packet.send_time_ms == PacketFeedback::kNoSendTime) {
          late_feedback_series.points.emplace_back(x, prev_y);
          continue;
        }
        int64_t y = packet.arrival_time_ms - packet.send_time_ms;
        prev_y = y;
        estimated_base_delay_ms = std::min(y, estimated_base_delay_ms);
        time_series.points.emplace_back(x, y);
      }
      ++rtcp_iterator;
    }
    if (clock.TimeInMicroseconds() >= NextRtpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtpTime());
      const RtpPacketType& rtp = *rtp_iterator->second;
      if (rtp.header.extension.hasTransportSequenceNumber) {
        feedback_adapter.AddPacket(rtp.header.ssrc,
                                   rtp.header.extension.transportSequenceNumber,
                                   rtp.total_length, PacedPacketInfo());
        feedback_adapter.OnSentPacket(
            rtp.header.extension.transportSequenceNumber, rtp.timestamp / 1000);
      }
      ++rtp_iterator;
    }
    time_us = std::min(NextRtpTime(), NextRtcpTime());
  }
  // We assume that the base network delay (w/o queues) is the min delay
  // observed during the call.
  for (TimeSeriesPoint& point : time_series.points)
    point.y -= estimated_base_delay_ms;
  for (TimeSeriesPoint& point : late_feedback_series.points)
    point.y -= estimated_base_delay_ms;
  // Add the data set to the plot.
  plot->AppendTimeSeriesIfNotEmpty(std::move(time_series));
  plot->AppendTimeSeriesIfNotEmpty(std::move(late_feedback_series));

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Delay (ms)", kBottomMargin, kTopMargin);
  plot->SetTitle("Network Delay Change.");
}

void EventLogAnalyzer::CreatePacerDelayGraph(Plot* plot) {
  using RtpPacketType = Outgoing::RtpPacketType;
  for (const auto& kv : parsed_log_.rtp_packets<Outgoing>()) {
    uint32_t ssrc = kv.first;
    const std::vector<RtpPacketType>& packets = kv.second;

    if (packets.size() < 2) {
      RTC_LOG(LS_WARNING)
          << "Can't estimate a the RTP clock frequency or the "
             "pacer delay with less than 2 packets in the stream";
      continue;
    }
    rtc::Optional<uint32_t> estimated_frequency =
        EstimateRtpClockFrequency<Outgoing>(packets);
    if (!estimated_frequency)
      continue;
    if (IsVideoSsrc<Outgoing>(ssrc) && *estimated_frequency != 90000) {
      RTC_LOG(LS_WARNING)
          << "Video stream should use a 90 kHz clock but appears to use "
          << *estimated_frequency / 1000 << ". Discarding.";
      continue;
    }

    TimeSeries pacer_delay_series(
        GetStreamName<Outgoing>(ssrc) + "(" +
            std::to_string(*estimated_frequency / 1000) + " kHz)",
        LineStyle::kLine, PointStyle::kHighlight);
    SeqNumUnwrapper<uint32_t> timestamp_unwrapper;
    uint64_t first_capture_timestamp =
        timestamp_unwrapper.Unwrap(packets.front().header.timestamp);
    uint64_t first_send_timestamp = packets.front().timestamp;
    for (const auto& packet : packets) {
      double capture_time_ms = (static_cast<double>(timestamp_unwrapper.Unwrap(
                                    packet.header.timestamp)) -
                                first_capture_timestamp) /
                               *estimated_frequency * 1000;
      double send_time_ms =
          static_cast<double>(packet.timestamp - first_send_timestamp) / 1000;
      float x = ToCallTime(packet.timestamp);
      float y = send_time_ms - capture_time_ms;
      pacer_delay_series.points.emplace_back(x, y);
    }
    plot->AppendTimeSeries(std::move(pacer_delay_series));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Pacer delay (ms)", kBottomMargin, kTopMargin);
  plot->SetTitle(
      "Delay from capture to send time. (First packet normalized to 0.)");
}

template <typename Direction>
void EventLogAnalyzer::CreateTimestampGraph(Plot* plot) {
  using RtpPacketType = typename Direction::RtpPacketType;
  for (const auto& kv : parsed_log_.rtp_packets<Direction>()) {
    uint32_t ssrc = kv.first;
    const std::vector<RtpPacketType>& rtp_packets = kv.second;

    TimeSeries rtp_timestamps(GetStreamName<Direction>(ssrc) + " capture-time",
                              LineStyle::kLine, PointStyle::kHighlight);
    for (const auto& packet : rtp_packets) {
      float x = ToCallTime(packet.timestamp);
      float y = packet.header.timestamp;
      rtp_timestamps.points.emplace_back(x, y);
    }
    plot->AppendTimeSeries(std::move(rtp_timestamps));

    TimeSeries rtcp_timestamps(
        GetStreamName<Direction>(ssrc) + " rtcp capture-time", LineStyle::kLine,
        PointStyle::kHighlight);
    for (const auto& rtcp : parsed_log_.sender_reports<Direction>()) {
      if (rtcp.sr.sender_ssrc() != ssrc)
        continue;
      float x = ToCallTime(rtcp.timestamp);
      float y = rtcp.sr.rtp_timestamp();
      rtcp_timestamps.points.emplace_back(x, y);
    }
    plot->AppendTimeSeriesIfNotEmpty(std::move(rtcp_timestamps));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "RTP timestamp", kBottomMargin, kTopMargin);
  plot->SetTitle(std::string(Direction::full_name) + " timestamps");
}
// Force instantiation of the template for both Incoming and Outgoing packets.
template void EventLogAnalyzer::CreateTimestampGraph<Incoming>(Plot*);
template void EventLogAnalyzer::CreateTimestampGraph<Outgoing>(Plot*);

void EventLogAnalyzer::CreateAudioEncoderTargetBitrateGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder target bitrate", LineStyle::kLine,
                         PointStyle::kHighlight);
  ProcessPoints<ParsedRtcEventLog::AudioNetworkAdaptationEvent>(
      [](const ParsedRtcEventLog::AudioNetworkAdaptationEvent& ana_event)
          -> rtc::Optional<float> {
        if (ana_event.config.bitrate_bps)
          return static_cast<float>(*ana_event.config.bitrate_bps);
        return rtc::nullopt;
      },
      parsed_log_.audio_network_adaptation_events(), begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (bps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder target bitrate");
}

void EventLogAnalyzer::CreateAudioEncoderFrameLengthGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder frame length", LineStyle::kLine,
                         PointStyle::kHighlight);
  ProcessPoints<ParsedRtcEventLog::AudioNetworkAdaptationEvent>(
      [](const ParsedRtcEventLog::AudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.frame_length_ms)
          return rtc::Optional<float>(
              static_cast<float>(*ana_event.config.frame_length_ms));
        return rtc::Optional<float>();
      },
      parsed_log_.audio_network_adaptation_events(), begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Frame length (ms)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder frame length");
}

void EventLogAnalyzer::CreateAudioEncoderPacketLossGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder uplink packet loss fraction",
                         LineStyle::kLine, PointStyle::kHighlight);
  ProcessPoints<ParsedRtcEventLog::AudioNetworkAdaptationEvent>(
      [](const ParsedRtcEventLog::AudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.uplink_packet_loss_fraction)
          return rtc::Optional<float>(static_cast<float>(
              *ana_event.config.uplink_packet_loss_fraction));
        return rtc::Optional<float>();
      },
      parsed_log_.audio_network_adaptation_events(), begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Percent lost packets", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Reported audio encoder lost packets");
}

void EventLogAnalyzer::CreateAudioEncoderEnableFecGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder FEC", LineStyle::kLine,
                         PointStyle::kHighlight);
  ProcessPoints<ParsedRtcEventLog::AudioNetworkAdaptationEvent>(
      [](const ParsedRtcEventLog::AudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.enable_fec)
          return rtc::Optional<float>(
              static_cast<float>(*ana_event.config.enable_fec));
        return rtc::Optional<float>();
      },
      parsed_log_.audio_network_adaptation_events(), begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "FEC (false/true)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder FEC");
}

void EventLogAnalyzer::CreateAudioEncoderEnableDtxGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder DTX", LineStyle::kLine,
                         PointStyle::kHighlight);
  ProcessPoints<ParsedRtcEventLog::AudioNetworkAdaptationEvent>(
      [](const ParsedRtcEventLog::AudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.enable_dtx)
          return rtc::Optional<float>(
              static_cast<float>(*ana_event.config.enable_dtx));
        return rtc::Optional<float>();
      },
      parsed_log_.audio_network_adaptation_events(), begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "DTX (false/true)", kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder DTX");
}

void EventLogAnalyzer::CreateAudioEncoderNumChannelsGraph(Plot* plot) {
  TimeSeries time_series("Audio encoder number of channels", LineStyle::kLine,
                         PointStyle::kHighlight);
  ProcessPoints<ParsedRtcEventLog::AudioNetworkAdaptationEvent>(
      [](const ParsedRtcEventLog::AudioNetworkAdaptationEvent& ana_event) {
        if (ana_event.config.num_channels)
          return rtc::Optional<float>(
              static_cast<float>(*ana_event.config.num_channels));
        return rtc::Optional<float>();
      },
      parsed_log_.audio_network_adaptation_events(), begin_time_, &time_series);
  plot->AppendTimeSeries(std::move(time_series));
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Number of channels (1 (mono)/2 (stereo))",
                          kBottomMargin, kTopMargin);
  plot->SetTitle("Reported audio encoder number of channels");
}

class NetEqStreamInput : public test::NetEqInput {
 public:
  // Does not take any ownership, and all pointers must refer to valid objects
  // that outlive the one constructed.
  NetEqStreamInput(const std::vector<Incoming::RtpPacketType>* packet_stream,
                   const std::vector<int64_t>* output_events_us,
                   rtc::Optional<int64_t> end_time_us)
      : packet_stream_(*packet_stream),
        packet_stream_it_(packet_stream_.begin()),
        output_events_us_it_(output_events_us->begin()),
        output_events_us_end_(output_events_us->end()),
        end_time_us_(end_time_us) {
    RTC_DCHECK(packet_stream);
    RTC_DCHECK(output_events_us);
  }

  rtc::Optional<int64_t> NextPacketTime() const override {
    if (packet_stream_it_ == packet_stream_.end()) {
      return rtc::nullopt;
    }
    if (end_time_us_ && packet_stream_it_->timestamp > *end_time_us_) {
      return rtc::nullopt;
    }
    // Convert from us to ms.
    return packet_stream_it_->timestamp / 1000;
  }

  rtc::Optional<int64_t> NextOutputEventTime() const override {
    if (output_events_us_it_ == output_events_us_end_) {
      return rtc::nullopt;
    }
    if (end_time_us_ && *output_events_us_it_ > *end_time_us_) {
      return rtc::nullopt;
    }
    // Convert from us to ms.
    return rtc::checked_cast<int64_t>(*output_events_us_it_ / 1000);
  }

  std::unique_ptr<PacketData> PopPacket() override {
    if (packet_stream_it_ == packet_stream_.end()) {
      return std::unique_ptr<PacketData>();
    }
    std::unique_ptr<PacketData> packet_data(new PacketData());
    packet_data->header = packet_stream_it_->header;
    // Convert from us to ms.
    packet_data->time_ms = packet_stream_it_->timestamp / 1000.0;

    // This is a header-only "dummy" packet. Set the payload to all zeros, with
    // length according to the virtual length.
    packet_data->payload.SetSize(packet_stream_it_->total_length);
    std::fill_n(packet_data->payload.data(), packet_data->payload.size(), 0);

    ++packet_stream_it_;
    return packet_data;
  }

  void AdvanceOutputEvent() override {
    if (output_events_us_it_ != output_events_us_end_) {
      ++output_events_us_it_;
    }
  }

  bool ended() const override { return !NextEventTime(); }

  rtc::Optional<RTPHeader> NextHeader() const override {
    if (packet_stream_it_ == packet_stream_.end()) {
      return rtc::nullopt;
    }
    return packet_stream_it_->header;
  }

 private:
  const std::vector<Incoming::RtpPacketType>& packet_stream_;
  std::vector<Incoming::RtpPacketType>::const_iterator packet_stream_it_;
  std::vector<int64_t>::const_iterator output_events_us_it_;
  const std::vector<int64_t>::const_iterator output_events_us_end_;
  const rtc::Optional<int64_t> end_time_us_;
};

namespace {
// Creates a NetEq test object and all necessary input and output helpers. Runs
// the test and returns the NetEqDelayAnalyzer object that was used to
// instrument the test.
std::unique_ptr<test::NetEqDelayAnalyzer> CreateNetEqTestAndRun(
    const std::vector<Incoming::RtpPacketType>* packet_stream,
    const std::vector<int64_t>* output_events_us,
    rtc::Optional<int64_t> end_time_us,
    const std::string& replacement_file_name,
    int file_sample_rate_hz) {
  std::unique_ptr<test::NetEqInput> input(
      new NetEqStreamInput(packet_stream, output_events_us, end_time_us));

  constexpr int kReplacementPt = 127;
  std::set<uint8_t> cn_types;
  std::set<uint8_t> forbidden_types;
  input.reset(new test::NetEqReplacementInput(std::move(input), kReplacementPt,
                                              cn_types, forbidden_types));

  NetEq::Config config;
  config.max_packets_in_buffer = 200;
  config.enable_fast_accelerate = true;

  std::unique_ptr<test::VoidAudioSink> output(new test::VoidAudioSink());

  test::NetEqTest::DecoderMap codecs;

  // Create a "replacement decoder" that produces the decoded audio by reading
  // from a file rather than from the encoded payloads.
  std::unique_ptr<test::ResampleInputAudioFile> replacement_file(
      new test::ResampleInputAudioFile(replacement_file_name,
                                       file_sample_rate_hz));
  replacement_file->set_output_rate_hz(48000);
  std::unique_ptr<AudioDecoder> replacement_decoder(
      new test::FakeDecodeFromFile(std::move(replacement_file), 48000, false));
  test::NetEqTest::ExtDecoderMap ext_codecs;
  ext_codecs[kReplacementPt] = {replacement_decoder.get(),
                                NetEqDecoder::kDecoderArbitrary,
                                "replacement codec"};

  std::unique_ptr<test::NetEqDelayAnalyzer> delay_cb(
      new test::NetEqDelayAnalyzer);
  test::DefaultNetEqTestErrorCallback error_cb;
  test::NetEqTest::Callbacks callbacks;
  callbacks.error_callback = &error_cb;
  callbacks.post_insert_packet = delay_cb.get();
  callbacks.get_audio_callback = delay_cb.get();

  test::NetEqTest test(config, codecs, ext_codecs, std::move(input),
                       std::move(output), callbacks);
  test.Run();
  return delay_cb;
}
}  // namespace

// Plots the jitter buffer delay profile. This will plot only for the first
// incoming audio SSRC. If the stream contains more than one incoming audio
// SSRC, all but the first will be ignored.
void EventLogAnalyzer::CreateAudioJitterBufferGraph(
    const std::string& replacement_file_name,
    int file_sample_rate_hz,
    Plot* plot) {
  const std::vector<Incoming::RtpPacketType>* audio_packets = nullptr;
  uint32_t ssrc;
  for (const auto& kv : parsed_log_.rtp_packets<Incoming>()) {
    if (IsAudioSsrc<Incoming>(kv.first)) {
      audio_packets = &kv.second;
      ssrc = kv.first;
      break;
    }
  }
  if (audio_packets == nullptr) {
    // No incoming audio stream found.
    return;
  }

  std::map<uint32_t, std::vector<int64_t>>::const_iterator output_events_it =
      parsed_log_.audio_playout_events().find(ssrc);
  if (output_events_it == parsed_log_.audio_playout_events().end()) {
    // Could not find output events with SSRC matching the input audio stream.
    // Using the first available stream of output events.
    output_events_it = parsed_log_.audio_playout_events().cbegin();
  }

  rtc::Optional<int64_t> end_time_us =
      log_segments_.empty()
          ? rtc::nullopt
          : rtc::Optional<int64_t>(log_segments_.front().second);

  auto delay_cb = CreateNetEqTestAndRun(
      audio_packets, &output_events_it->second, end_time_us,
      replacement_file_name, file_sample_rate_hz);

  std::vector<float> send_times_s;
  std::vector<float> arrival_delay_ms;
  std::vector<float> corrected_arrival_delay_ms;
  std::vector<rtc::Optional<float>> playout_delay_ms;
  std::vector<rtc::Optional<float>> target_delay_ms;
  delay_cb->CreateGraphs(&send_times_s, &arrival_delay_ms,
                         &corrected_arrival_delay_ms, &playout_delay_ms,
                         &target_delay_ms);
  RTC_DCHECK_EQ(send_times_s.size(), arrival_delay_ms.size());
  RTC_DCHECK_EQ(send_times_s.size(), corrected_arrival_delay_ms.size());
  RTC_DCHECK_EQ(send_times_s.size(), playout_delay_ms.size());
  RTC_DCHECK_EQ(send_times_s.size(), target_delay_ms.size());

  std::map<uint32_t, TimeSeries> time_series_packet_arrival;
  std::map<uint32_t, TimeSeries> time_series_relative_packet_arrival;
  std::map<uint32_t, TimeSeries> time_series_play_time;
  std::map<uint32_t, TimeSeries> time_series_target_time;
  float min_y_axis = 0.f;
  float max_y_axis = 0.f;
  for (size_t i = 0; i < send_times_s.size(); ++i) {
    time_series_packet_arrival[ssrc].points.emplace_back(
        TimeSeriesPoint(send_times_s[i], arrival_delay_ms[i]));
    time_series_relative_packet_arrival[ssrc].points.emplace_back(
        TimeSeriesPoint(send_times_s[i], corrected_arrival_delay_ms[i]));
    min_y_axis = std::min(min_y_axis, corrected_arrival_delay_ms[i]);
    max_y_axis = std::max(max_y_axis, corrected_arrival_delay_ms[i]);
    if (playout_delay_ms[i]) {
      time_series_play_time[ssrc].points.emplace_back(
          TimeSeriesPoint(send_times_s[i], *playout_delay_ms[i]));
      min_y_axis = std::min(min_y_axis, *playout_delay_ms[i]);
      max_y_axis = std::max(max_y_axis, *playout_delay_ms[i]);
    }
    if (target_delay_ms[i]) {
      time_series_target_time[ssrc].points.emplace_back(
          TimeSeriesPoint(send_times_s[i], *target_delay_ms[i]));
      min_y_axis = std::min(min_y_axis, *target_delay_ms[i]);
      max_y_axis = std::max(max_y_axis, *target_delay_ms[i]);
    }
  }

  // This code is adapted for a single stream. The creation of the streams above
  // guarantee that no more than one steam is included. If multiple streams are
  // to be plotted, they should likely be given distinct labels below.
  RTC_DCHECK_EQ(time_series_relative_packet_arrival.size(), 1);
  for (auto& series : time_series_relative_packet_arrival) {
    series.second.label = "Relative packet arrival delay";
    series.second.line_style = LineStyle::kLine;
    plot->AppendTimeSeries(std::move(series.second));
  }
  RTC_DCHECK_EQ(time_series_play_time.size(), 1);
  for (auto& series : time_series_play_time) {
    series.second.label = "Playout delay";
    series.second.line_style = LineStyle::kLine;
    plot->AppendTimeSeries(std::move(series.second));
  }
  RTC_DCHECK_EQ(time_series_target_time.size(), 1);
  for (auto& series : time_series_target_time) {
    series.second.label = "Target delay";
    series.second.line_style = LineStyle::kLine;
    series.second.point_style = PointStyle::kHighlight;
    plot->AppendTimeSeries(std::move(series.second));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetYAxis(min_y_axis, max_y_axis, "Relative delay (ms)", kBottomMargin,
                 kTopMargin);
  plot->SetTitle("NetEq timing");
}

void EventLogAnalyzer::CreateIceCandidatePairConfigGraph(Plot* plot) {
  std::map<uint32_t, TimeSeries> configs_by_cp_id;
  for (const auto& config : parsed_log_.ice_candidate_pair_configs()) {
    if (configs_by_cp_id.find(config.candidate_pair_id) ==
        configs_by_cp_id.end()) {
      const std::string candidate_pair_desc =
          GetCandidatePairLogDescriptionAsString(config);
      configs_by_cp_id[config.candidate_pair_id] =
          TimeSeries("[" + std::to_string(config.candidate_pair_id) + "]" +
                         candidate_pair_desc,
                     LineStyle::kNone, PointStyle::kHighlight);
      candidate_pair_desc_by_id_[config.candidate_pair_id] =
          candidate_pair_desc;
    }
    float x = ToCallTime(config.timestamp);
    float y = static_cast<float>(config.type);
    configs_by_cp_id[config.candidate_pair_id].points.emplace_back(x, y);
  }

  // TODO(qingsi): There can be a large number of candidate pairs generated by
  // certain calls and the frontend cannot render the chart in this case due to
  // the failure of generating a palette with the same number of colors.
  for (auto& kv : configs_by_cp_id) {
    plot->AppendTimeSeries(std::move(kv.second));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 3, "Numeric Config Type", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("[IceEventLog] ICE candidate pair configs");
}

std::string EventLogAnalyzer::GetCandidatePairLogDescriptionFromId(
    uint32_t candidate_pair_id) {
  if (candidate_pair_desc_by_id_.find(candidate_pair_id) !=
      candidate_pair_desc_by_id_.end()) {
    return candidate_pair_desc_by_id_[candidate_pair_id];
  }
  for (const auto& config : parsed_log_.ice_candidate_pair_configs()) {
    // TODO(qingsi): Add the handling of the "Updated" config event after the
    // visualization of property change for candidate pairs is introduced.
    if (candidate_pair_desc_by_id_.find(config.candidate_pair_id) ==
        candidate_pair_desc_by_id_.end()) {
      const std::string candidate_pair_desc =
          GetCandidatePairLogDescriptionAsString(config);
      candidate_pair_desc_by_id_[config.candidate_pair_id] =
          candidate_pair_desc;
    }
  }
  return candidate_pair_desc_by_id_[candidate_pair_id];
}

void EventLogAnalyzer::CreateIceConnectivityCheckGraph(Plot* plot) {
  std::map<uint32_t, TimeSeries> checks_by_cp_id;
  for (const auto& event : parsed_log_.ice_candidate_pair_events()) {
    if (checks_by_cp_id.find(event.candidate_pair_id) ==
        checks_by_cp_id.end()) {
      checks_by_cp_id[event.candidate_pair_id] = TimeSeries(
          "[" + std::to_string(event.candidate_pair_id) + "]" +
              GetCandidatePairLogDescriptionFromId(event.candidate_pair_id),
          LineStyle::kNone, PointStyle::kHighlight);
    }
    float x = ToCallTime(event.timestamp);
    float y = static_cast<float>(event.type);
    checks_by_cp_id[event.candidate_pair_id].points.emplace_back(x, y);
  }

  // TODO(qingsi): The same issue as in CreateIceCandidatePairConfigGraph.
  for (auto& kv : checks_by_cp_id) {
    plot->AppendTimeSeries(std::move(kv.second));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 4, "Numeric Connectivity State", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("[IceEventLog] ICE connectivity checks");
}

void EventLogAnalyzer::Notification(
    std::unique_ptr<TriageNotification> notification) {
  notifications_.push_back(std::move(notification));
}

void EventLogAnalyzer::PrintNotifications(FILE* file) {
  if (notifications_.size() == 0)
    return;
  fprintf(file, "========== TRIAGE NOTIFICATIONS ==========\n");
  for (const auto& notification : notifications_) {
    rtc::Optional<float> call_timestamp = notification->Time();
    if (call_timestamp.has_value()) {
      fprintf(file, "%3.3lf s : %s\n", call_timestamp.value(),
              notification->ToString().c_str());
    } else {
      fprintf(file, "          : %s\n", notification->ToString().c_str());
    }
  }
  fprintf(file, "========== END TRIAGE NOTIFICATIONS ==========\n");
}

template <typename T>
struct NotificationTraits {
  static_assert(sizeof(T) != sizeof(T),
                "Template argument must be either Incoming or Outgoing.");
};

template <>
struct NotificationTraits<Incoming> {
  using SeqNoJump = IncomingSeqNoJump;
  using CaptureTimeJump = IncomingCaptureTimeJump;
  using RtpTransmissionTimeGap = IncomingRtpReceiveTimeGap;
  using RtcpTransmissionTimeGap = IncomingRtcpReceiveTimeGap;
  static constexpr uint64_t kMaxSeqNoJump = 1000;
  static constexpr uint64_t kMaxCaptureTimeJump = 900000;
  static constexpr int64_t MaxRtpTransmissionGap = 500000;
  static constexpr int64_t kMaxRtcpTransmissionGap = 2000000;
};

template <>
struct NotificationTraits<Outgoing> {
  using SeqNoJump = OutgoingSeqNoJump;
  using CaptureTimeJump = OutgoingCaptureTimeJump;
  using RtpTransmissionTimeGap = OutgoingRtpSendTimeGap;
  using RtcpTransmissionTimeGap = OutgoingRtcpSendTimeGap;
  static constexpr uint64_t kMaxSeqNoJump = 1000;
  static constexpr uint64_t kMaxCaptureTimeJump = 900000;
  static constexpr int64_t kMaxRtpTransmissionGap = 500000;
  static constexpr int64_t kMaxRtcpTransmissionGap = 2000000;
};

template <typename Direction>
void EventLogAnalyzer::CreateStreamGapNotifications() {
  using RtpPacketType = typename Direction::RtpPacketType;
  using SeqNoJumpNotification =
      typename NotificationTraits<Direction>::SeqNoJump;
  using CaptureTimeJumpNotification =
      typename NotificationTraits<Direction>::CaptureTimeJump;
  int64_t end_time_us = log_segments_.empty()
                            ? std::numeric_limits<int64_t>::max()
                            : log_segments_.front().second;
  // Check for gaps in sequence numbers and capture timestamps.
  for (auto& kv : parsed_log_.rtp_packets<Direction>()) {
    const std::vector<RtpPacketType>& packet_stream = kv.second;

    SeqNumUnwrapper<uint16_t> seq_no_unwrapper;
    rtc::Optional<int64_t> last_seq_no;
    SeqNumUnwrapper<uint32_t> timestamp_unwrapper;
    rtc::Optional<int64_t> last_timestamp;
    for (const auto& packet : packet_stream) {
      if (packet.timestamp > end_time_us) {
        // Only process the first (LOG_START, LOG_END) segment.
        break;
      }
      int64_t seq_no = seq_no_unwrapper.Unwrap(packet.header.sequenceNumber);
      if (last_seq_no.has_value() &&
          std::abs(seq_no - last_seq_no.value()) > 1000) {
        // With roughly 100 packets per second (~800kbps), this would require 10
        // seconds without data to trigger incorrectly.
        Notification(rtc::MakeUnique<SeqNoJumpNotification>(
            ToCallTime(packet.timestamp), packet.header.ssrc));
      }
      last_seq_no.emplace(seq_no);
      int64_t timestamp = timestamp_unwrapper.Unwrap(packet.header.timestamp);
      if (last_timestamp.has_value() &&
          std::abs(timestamp - last_timestamp.value()) > 900000) {
        // With a 90 kHz clock, this would require 10 seconds without data to
        // trigger incorrectly.
        Notification(rtc::MakeUnique<CaptureTimeJumpNotification>(
            ToCallTime(packet.timestamp), packet.header.ssrc));
      }
      last_timestamp.emplace(timestamp);
    }
  }
}
// Force instantiation of the template for both Incoming and Outgoing packets.
template void EventLogAnalyzer::CreateStreamGapNotifications<Incoming>();
template void EventLogAnalyzer::CreateStreamGapNotifications<Outgoing>();

template <typename Direction>
void EventLogAnalyzer::CreateTransmissionGapNotifications() {
  using RtpPacketType = typename Direction::RtpPacketType;
  using RtcpPacketType = typename Direction::RtcpPacketType;
  using RtpGapNotification =
      typename NotificationTraits<Direction>::RtpTransmissionTimeGap;
  using RtcpGapNotification =
      typename NotificationTraits<Direction>::RtcpTransmissionTimeGap;
  int64_t end_time_us = log_segments_.empty()
                            ? std::numeric_limits<int64_t>::max()
                            : log_segments_.front().second;

  // TODO(terelius): The parser could provide a list of all packets, ordered
  // by time, for each direction.
  std::multimap<int64_t, const RtpPacketType*> rtp_in_direction;
  for (const auto& kv : parsed_log_.rtp_packets<Direction>()) {
    for (const RtpPacketType& rtp_packet : kv.second)
      rtp_in_direction.emplace(rtp_packet.timestamp, &rtp_packet);
  }
  rtc::Optional<int64_t> last_rtp_time;
  for (const auto& kv : rtp_in_direction) {
    int64_t timestamp = kv.first;
    if (timestamp > end_time_us) {
      // Only process the first (LOG_START, LOG_END) segment.
      break;
    }
    int64_t duration = timestamp - last_rtp_time.value_or(0);
    if (last_rtp_time.has_value() && duration > 500000) {
      // No packet sent/received for more than 500 ms.
      Notification(rtc::MakeUnique<RtpGapNotification>(ToCallTime(timestamp),
                                                       duration / 1000));
    }
    last_rtp_time.emplace(timestamp);
  }

  rtc::Optional<int64_t> last_rtcp_time;
  for (const RtcpPacketType& rtcp : parsed_log_.rtcp_packets<Direction>()) {
    if (rtcp.timestamp > end_time_us) {
      // Only process the first (LOG_START, LOG_END) segment.
      break;
    }
    int64_t duration = rtcp.timestamp - last_rtcp_time.value_or(0);
    if (last_rtcp_time.has_value() && duration > 2000000) {
      // No feedback sent/received for more than 2000 ms.
      Notification(rtc::MakeUnique<RtcpGapNotification>(
          ToCallTime(rtcp.timestamp), duration / 1000));
    }
    last_rtcp_time.emplace(rtcp.timestamp);
  }
}
template void EventLogAnalyzer::CreateTransmissionGapNotifications<Incoming>();
template void EventLogAnalyzer::CreateTransmissionGapNotifications<Outgoing>();

// TODO(terelius): Notifications could possibly be generated by the same code
// that produces the graphs. There is some code duplication that could be
// avoided, but that might be solved anyway when we move functionality from the
// analyzer to the parser.
void EventLogAnalyzer::CreateTriageNotifications() {
  CreateStreamGapNotifications<Incoming>();
  CreateStreamGapNotifications<Outgoing>();
  CreateTransmissionGapNotifications<Incoming>();
  CreateTransmissionGapNotifications<Outgoing>();

  int64_t end_time_us = log_segments_.empty()
                            ? std::numeric_limits<int64_t>::max()
                            : log_segments_.front().second;

  // Loss feedback
  int64_t total_lost_packets = 0;
  int64_t total_expected_packets = 0;
  for (auto& bwe_update : parsed_log_.bwe_loss_updates()) {
    if (bwe_update.timestamp > end_time_us) {
      // Only process the first (LOG_START, LOG_END) segment.
      break;
    }
    int64_t lost_packets = static_cast<double>(bwe_update.fraction_lost) / 255 *
                           bwe_update.expected_packets;
    total_lost_packets += lost_packets;
    total_expected_packets += bwe_update.expected_packets;
  }
  double avg_outgoing_loss =
      static_cast<double>(total_lost_packets) / total_expected_packets;
  if (avg_outgoing_loss > 0.05) {
    Notification(rtc::MakeUnique<OutgoingHighLoss>(avg_outgoing_loss));
  }
}

}  // namespace plotting
}  // namespace webrtc
