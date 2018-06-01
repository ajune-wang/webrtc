/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_success.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_event_log_parser_new.h"
#include "logging/rtc_event_log/rtc_event_log_unittest_helper.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "rtc_base/checks.h"
#include "rtc_base/fakeclock.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/random.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

struct EventCounts {
  size_t audio_send_streams = 0;
  size_t audio_recv_streams = 0;
  size_t video_send_streams = 0;
  size_t video_recv_streams = 0;
  size_t alr_states = 0;
  size_t audio_playouts = 0;
  size_t ana_configs = 0;
  size_t bwe_loss_events = 0;
  size_t bwe_delay_events = 0;
  size_t probe_creations = 0;
  size_t probe_successes = 0;
  size_t probe_failures = 0;
  size_t ice_configs = 0;
  size_t ice_events = 0;
  size_t incoming_rtp_packets = 0;
  size_t outgoing_rtp_packets = 0;
  size_t incoming_rtcp_packets = 0;
  size_t outgoing_rtcp_packets = 0;

  size_t total_nonconfig_events() {
    return alr_states + audio_playouts + ana_configs + bwe_loss_events +
           bwe_delay_events + probe_creations + probe_successes +
           probe_failures + ice_configs + ice_events + incoming_rtp_packets +
           outgoing_rtp_packets + incoming_rtcp_packets + outgoing_rtcp_packets;
  }

  size_t total_events() {
    return total_nonconfig_events() + audio_send_streams + audio_recv_streams +
           video_send_streams + video_recv_streams;
  }
};

class RtcEventLogSession
    : public ::testing::TestWithParam<std::tuple<uint64_t, int64_t>> {
 public:
  RtcEventLogSession()
      : prng(std::get<0>(GetParam())),
        output_period_ms(std::get<1>(GetParam())) {}
  void WriteLog(EventCounts count, size_t num_events_before_log_start);
  void ReadAndVerifyLog();

 private:
  // Configs.
  std::vector<std::unique_ptr<RtcEventAudioSendStreamConfig>>
      audio_send_config_list;
  std::vector<std::unique_ptr<RtcEventAudioReceiveStreamConfig>>
      audio_recv_config_list;
  std::vector<std::unique_ptr<RtcEventVideoSendStreamConfig>>
      video_send_config_list;
  std::vector<std::unique_ptr<RtcEventVideoReceiveStreamConfig>>
      video_recv_config_list;

  // Regular events.
  std::vector<std::unique_ptr<RtcEventAlrState>> alr_state_list;
  std::map<uint32_t, std::vector<std::unique_ptr<RtcEventAudioPlayout>>>
      audio_playout_map;
  std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>> ana_configs_list;
  std::vector<std::unique_ptr<RtcEventBweUpdateLossBased>> bwe_loss_list;
  std::vector<std::unique_ptr<RtcEventBweUpdateDelayBased>> bwe_delay_list;
  std::vector<std::unique_ptr<RtcEventProbeClusterCreated>> probe_creation_list;
  std::vector<std::unique_ptr<RtcEventProbeResultSuccess>> probe_success_list;
  std::vector<std::unique_ptr<RtcEventProbeResultFailure>> probe_failure_list;
  std::vector<std::unique_ptr<RtcEventIceCandidatePairConfig>> ice_config_list;
  std::vector<std::unique_ptr<RtcEventIceCandidatePair>> ice_event_list;
  std::map<uint32_t, std::vector<std::unique_ptr<RtcEventRtpPacketIncoming>>>
      incoming_rtp_map;
  std::map<uint32_t, std::vector<std::unique_ptr<RtcEventRtpPacketOutgoing>>>
      outgoing_rtp_map;
  std::vector<std::unique_ptr<RtcEventRtcpPacketIncoming>> incoming_rtcp_list;
  std::vector<std::unique_ptr<RtcEventRtcpPacketOutgoing>> outgoing_rtcp_list;

  int64_t start_time_us;
  int64_t stop_time_us;

  Random prng;
  int64_t output_period_ms;
};

bool SsrcUsed(
    uint32_t ssrc,
    const std::vector<std::pair<uint32_t, RtpHeaderExtensionMap>>& streams) {
  for (const auto& kv : streams) {
    if (kv.first == ssrc)
      return true;
  }
  return false;
}

void RtcEventLogSession::WriteLog(EventCounts count,
                                  size_t num_events_before_start) {
  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  std::string test_name = test_info->name();
  std::replace(test_name.begin(), test_name.end(), '/', '_');
  const std::string temp_filename =
      test::OutputPath() + "RtcEventLogTest_" + test_name;

  // TODO(terelius): Allow test to run wth either a real or a fake clock.
  // Maybe always use the ScopedFakeClock, but conditionally SleepMs()?
  rtc::ScopedFakeClock clock;
  clock.SetTimeMicros(prng.Rand<uint32_t>());

  // Then log file will be flushed to disk when the event_log goes out of scope.
  std::unique_ptr<RtcEventLog> event_log(
      RtcEventLog::Create(RtcEventLog::EncodingType::Legacy));

  // We can't send or receive packets without configured streams.
  count.video_recv_streams = std::max<size_t>(count.video_recv_streams, 1);
  count.video_send_streams = std::max<size_t>(count.video_send_streams, 1);

  RtpHeaderExtensionMap all_extensions;
  all_extensions.Register<AudioLevel>(RtpExtension::kAudioLevelDefaultId);
  all_extensions.Register<TransmissionOffset>(
      RtpExtension::kTimestampOffsetDefaultId);
  all_extensions.Register<AbsoluteSendTime>(
      RtpExtension::kAbsSendTimeDefaultId);
  all_extensions.Register<VideoOrientation>(
      RtpExtension::kVideoRotationDefaultId);
  all_extensions.Register<TransportSequenceNumber>(
      RtpExtension::kTransportSequenceNumberDefaultId);

  std::vector<std::pair<uint32_t, RtpHeaderExtensionMap>> incoming_extensions;
  std::vector<std::pair<uint32_t, RtpHeaderExtensionMap>> outgoing_extensions;

  // Receive streams.
  {
    uint32_t ssrc = prng.Rand<uint32_t>();
    incoming_extensions.emplace_back(prng.Rand<uint32_t>(), all_extensions);
    auto event = test::GenerateRtcEventVideoReceiveStreamConfig(
        ssrc, all_extensions, &prng);
    event_log->Log(event->Copy());
    video_recv_config_list.push_back(std::move(event));
    for (size_t i = 1; i < count.video_recv_streams; i++) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      do {
        ssrc = prng.Rand<uint32_t>();
      } while (SsrcUsed(ssrc, incoming_extensions));
      RtpHeaderExtensionMap extensions =
          test::GenerateRtpHeaderExtensionMap(&prng);
      incoming_extensions.emplace_back(ssrc, extensions);
      auto event = test::GenerateRtcEventVideoReceiveStreamConfig(
          ssrc, extensions, &prng);
      event_log->Log(event->Copy());
      video_recv_config_list.push_back(std::move(event));
    }
    for (size_t i = 0; i < count.audio_recv_streams; i++) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      do {
        ssrc = prng.Rand<uint32_t>();
      } while (SsrcUsed(ssrc, incoming_extensions));
      RtpHeaderExtensionMap extensions =
          test::GenerateRtpHeaderExtensionMap(&prng);
      incoming_extensions.emplace_back(ssrc, extensions);
      auto event = test::GenerateRtcEventAudioReceiveStreamConfig(
          ssrc, extensions, &prng);
      event_log->Log(event->Copy());
      audio_recv_config_list.push_back(std::move(event));
    }
  }

  // Send streams.
  {
    uint32_t ssrc = prng.Rand<uint32_t>();
    outgoing_extensions.emplace_back(prng.Rand<uint32_t>(), all_extensions);
    auto event = test::GenerateRtcEventVideoSendStreamConfig(
        ssrc, all_extensions, &prng);
    event_log->Log(event->Copy());
    video_send_config_list.push_back(std::move(event));
    for (size_t i = 1; i < count.video_send_streams; i++) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      do {
        ssrc = prng.Rand<uint32_t>();
      } while (SsrcUsed(ssrc, outgoing_extensions));
      RtpHeaderExtensionMap extensions =
          test::GenerateRtpHeaderExtensionMap(&prng);
      outgoing_extensions.emplace_back(ssrc, extensions);
      auto event =
          test::GenerateRtcEventVideoSendStreamConfig(ssrc, extensions, &prng);
      event_log->Log(event->Copy());
      video_send_config_list.push_back(std::move(event));
    }
    for (size_t i = 0; i < count.audio_send_streams; i++) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      do {
        ssrc = prng.Rand<uint32_t>();
      } while (SsrcUsed(ssrc, outgoing_extensions));
      RtpHeaderExtensionMap extensions =
          test::GenerateRtpHeaderExtensionMap(&prng);
      outgoing_extensions.emplace_back(ssrc, extensions);
      auto event =
          test::GenerateRtcEventAudioSendStreamConfig(ssrc, extensions, &prng);
      event_log->Log(event->Copy());
      audio_send_config_list.push_back(std::move(event));
    }
  }

  size_t remaining_events = count.total_nonconfig_events();
  ASSERT_LE(num_events_before_start, remaining_events);
  size_t remaining_events_at_start = remaining_events - num_events_before_start;
  for (; remaining_events > 0; remaining_events--) {
    if (remaining_events == remaining_events_at_start) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      event_log->StartLogging(
          rtc::MakeUnique<RtcEventLogOutputFile>(temp_filename, 10000000),
          output_period_ms);
      start_time_us = rtc::TimeMicros();
    }

    size_t selection = prng.Rand(remaining_events - 1);

    if (selection < count.alr_states) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      auto event = test::GenerateRtcEventAlrState(&prng);
      event_log->Log(event->Copy());
      alr_state_list.push_back(std::move(event));
      count.alr_states--;
      continue;
    }
    selection -= count.alr_states;

    if (selection < count.audio_playouts) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      size_t stream = prng.Rand(incoming_extensions.size() - 1);
      // This might be a video SSRC, but the parser does not use the config.
      uint32_t ssrc = incoming_extensions[stream].first;
      auto event = test::GenerateRtcEventAudioPlayout(ssrc, &prng);
      event_log->Log(event->Copy());
      audio_playout_map[ssrc].push_back(std::move(event));
      count.audio_playouts--;
      continue;
    }
    selection -= count.audio_playouts;

    if (selection < count.ana_configs) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      auto event = test::GenerateRtcEventAudioNetworkAdaptation(&prng);
      event_log->Log(event->Copy());
      ana_configs_list.push_back(std::move(event));
      count.ana_configs--;
      continue;
    }
    selection -= count.ana_configs;

    if (selection < count.bwe_loss_events) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      auto event = test::GenerateRtcEventBweUpdateLossBased(&prng);
      event_log->Log(event->Copy());
      bwe_loss_list.push_back(std::move(event));
      count.bwe_loss_events--;
      continue;
    }
    selection -= count.bwe_loss_events;

    if (selection < count.bwe_delay_events) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      auto event = test::GenerateRtcEventBweUpdateDelayBased(&prng);
      event_log->Log(event->Copy());
      bwe_delay_list.push_back(std::move(event));
      count.bwe_delay_events--;
      continue;
    }
    selection -= count.bwe_delay_events;

    if (selection < count.probe_creations) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      auto event = test::GenerateRtcEventProbeClusterCreated(&prng);
      event_log->Log(event->Copy());
      probe_creation_list.push_back(std::move(event));
      count.probe_creations--;
      continue;
    }
    selection -= count.probe_creations;

    if (selection < count.probe_successes) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      auto event = test::GenerateRtcEventProbeResultSuccess(&prng);
      event_log->Log(event->Copy());
      probe_success_list.push_back(std::move(event));
      count.probe_successes--;
      continue;
    }
    selection -= count.probe_successes;

    if (selection < count.probe_failures) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      auto event = test::GenerateRtcEventProbeResultFailure(&prng);
      event_log->Log(event->Copy());
      probe_failure_list.push_back(std::move(event));
      count.probe_failures--;
      continue;
    }
    selection -= count.probe_failures;

    if (selection < count.ice_configs) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      auto event = test::GenerateRtcEventIceCandidatePairConfig(&prng);
      event_log->Log(event->Copy());
      ice_config_list.push_back(std::move(event));
      count.ice_configs--;
      continue;
    }
    selection -= count.ice_configs;

    if (selection < count.ice_events) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      auto event = test::GenerateRtcEventIceCandidatePair(&prng);
      event_log->Log(event->Copy());
      ice_event_list.push_back(std::move(event));
      count.ice_events--;
      continue;
    }
    selection -= count.ice_events;

    if (selection < count.incoming_rtp_packets) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      size_t stream = prng.Rand(incoming_extensions.size() - 1);
      uint32_t ssrc = incoming_extensions[stream].first;
      auto event = test::GenerateRtcEventRtpPacketIncoming(
          ssrc, incoming_extensions[stream].second, &prng);
      event_log->Log(event->Copy());
      incoming_rtp_map[ssrc].push_back(std::move(event));
      count.incoming_rtp_packets--;
      continue;
    }
    selection -= count.incoming_rtp_packets;

    if (selection < count.outgoing_rtp_packets) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      size_t stream = prng.Rand(outgoing_extensions.size() - 1);
      uint32_t ssrc = outgoing_extensions[stream].first;
      auto event = test::GenerateRtcEventRtpPacketOutgoing(
          ssrc, outgoing_extensions[stream].second, &prng);
      event_log->Log(event->Copy());
      outgoing_rtp_map[ssrc].push_back(std::move(event));
      count.outgoing_rtp_packets--;
      continue;
    }
    selection -= count.outgoing_rtp_packets;

    if (selection < count.incoming_rtcp_packets) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      auto event = test::GenerateRtcEventRtcpPacketIncoming(&prng);
      event_log->Log(event->Copy());
      incoming_rtcp_list.push_back(std::move(event));
      count.incoming_rtcp_packets--;
      continue;
    }
    selection -= count.incoming_rtcp_packets;

    if (selection < count.outgoing_rtcp_packets) {
      clock.AdvanceTimeMicros(prng.Rand(20) * 1000);
      auto event = test::GenerateRtcEventRtcpPacketOutgoing(&prng);
      event_log->Log(event->Copy());
      outgoing_rtcp_list.push_back(std::move(event));
      count.outgoing_rtcp_packets--;
      continue;
    }
    selection -= count.outgoing_rtcp_packets;

    RTC_NOTREACHED();
  }

  event_log->StopLogging();
  stop_time_us = rtc::TimeMicros();

  EXPECT_EQ(count.total_nonconfig_events(), static_cast<size_t>(0));
}

// Read the file and verify that what we read back from the event log is the
// same as what we wrote down.
void RtcEventLogSession::ReadAndVerifyLog() {
  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  std::string test_name = test_info->name();
  std::replace(test_name.begin(), test_name.end(), '/', '_');
  const std::string temp_filename =
      test::OutputPath() + "RtcEventLogTest_" + test_name;

  // Read the generated file from disk.
  ParsedRtcEventLogNew parsed_log;
  ASSERT_TRUE(parsed_log.ParseFile(temp_filename));

  // Start and stop events.
  auto& parsed_start_log_events = parsed_log.start_log_events();
  ASSERT_EQ(parsed_start_log_events.size(), static_cast<size_t>(1));
  test::VerifyLoggedStartEvent(start_time_us, parsed_start_log_events[0]);

  auto& parsed_stop_log_events = parsed_log.stop_log_events();
  ASSERT_EQ(parsed_stop_log_events.size(), static_cast<size_t>(1));
  test::VerifyLoggedStopEvent(stop_time_us, parsed_stop_log_events[0]);

  auto& parsed_alr_state_events = parsed_log.alr_state_events();
  ASSERT_EQ(parsed_alr_state_events.size(), alr_state_list.size());
  for (size_t i = 0; i < parsed_alr_state_events.size(); i++) {
    test::VerifyLoggedAlrStateEvent(*alr_state_list[i],
                                    parsed_alr_state_events[i]);
  }

  const auto& parsed_audio_playout_map = parsed_log.audio_playout_events();
  ASSERT_EQ(parsed_audio_playout_map.size(), audio_playout_map.size());
  for (const auto& kv : parsed_audio_playout_map) {
    uint32_t ssrc = kv.first;
    const auto& parsed_audio_playout_stream = kv.second;
    const auto& audio_playout_stream = audio_playout_map[ssrc];
    ASSERT_EQ(parsed_audio_playout_stream.size(), audio_playout_stream.size());
    for (size_t i = 0; i < parsed_audio_playout_map.size(); i++) {
      test::VerifyLoggedAudioPlayoutEvent(*audio_playout_stream[i],
                                          parsed_audio_playout_stream[i]);
    }
  }

  auto& parsed_audio_network_adaptation_events =
      parsed_log.audio_network_adaptation_events();
  ASSERT_EQ(parsed_audio_network_adaptation_events.size(),
            ana_configs_list.size());
  for (size_t i = 0; i < parsed_audio_network_adaptation_events.size(); i++) {
    test::VerifyLoggedAudioNetworkAdaptationEvent(
        *ana_configs_list[i], parsed_audio_network_adaptation_events[i]);
  }

  auto& parsed_bwe_delay_updates = parsed_log.bwe_delay_updates();
  ASSERT_EQ(parsed_bwe_delay_updates.size(), bwe_delay_list.size());
  for (size_t i = 0; i < parsed_bwe_delay_updates.size(); i++) {
    test::VerifyLoggedBweDelayBasedUpdate(*bwe_delay_list[i],
                                          parsed_bwe_delay_updates[i]);
  }

  auto& parsed_bwe_loss_updates = parsed_log.bwe_loss_updates();
  ASSERT_EQ(parsed_bwe_loss_updates.size(), bwe_loss_list.size());
  for (size_t i = 0; i < parsed_bwe_loss_updates.size(); i++) {
    test::VerifyLoggedBweLossBasedUpdate(*bwe_loss_list[i],
                                         parsed_bwe_loss_updates[i]);
  }

  auto& parsed_bwe_probe_cluster_created_events =
      parsed_log.bwe_probe_cluster_created_events();
  ASSERT_EQ(parsed_bwe_probe_cluster_created_events.size(),
            probe_creation_list.size());
  for (size_t i = 0; i < parsed_bwe_probe_cluster_created_events.size(); i++) {
    test::VerifyLoggedBweProbeClusterCreatedEvent(
        *probe_creation_list[i], parsed_bwe_probe_cluster_created_events[i]);
  }

  auto& parsed_bwe_probe_failure_events = parsed_log.bwe_probe_failure_events();
  ASSERT_EQ(parsed_bwe_probe_failure_events.size(), probe_failure_list.size());
  for (size_t i = 0; i < parsed_bwe_probe_failure_events.size(); i++) {
    test::VerifyLoggedBweProbeFailureEvent(*probe_failure_list[i],
                                           parsed_bwe_probe_failure_events[i]);
  }

  auto& parsed_bwe_probe_success_events = parsed_log.bwe_probe_success_events();
  ASSERT_EQ(parsed_bwe_probe_success_events.size(), probe_success_list.size());
  for (size_t i = 0; i < parsed_bwe_probe_success_events.size(); i++) {
    test::VerifyLoggedBweProbeSuccessEvent(*probe_success_list[i],
                                           parsed_bwe_probe_success_events[i]);
  }

  auto& parsed_ice_candidate_pair_configs =
      parsed_log.ice_candidate_pair_configs();
  ASSERT_EQ(parsed_ice_candidate_pair_configs.size(), ice_config_list.size());
  for (size_t i = 0; i < parsed_ice_candidate_pair_configs.size(); i++) {
    test::VerifyLoggedIceCandidatePairConfig(
        *ice_config_list[i], parsed_ice_candidate_pair_configs[i]);
  }

  auto& parsed_ice_candidate_pair_events =
      parsed_log.ice_candidate_pair_events();
  ASSERT_EQ(parsed_ice_candidate_pair_events.size(),
            parsed_ice_candidate_pair_events.size());
  for (size_t i = 0; i < parsed_ice_candidate_pair_events.size(); i++) {
    test::VerifyLoggedIceCandidatePairEvent(
        *ice_event_list[i], parsed_ice_candidate_pair_events[i]);
  }

  auto& parsed_incoming_rtp_packets_by_ssrc =
      parsed_log.incoming_rtp_packets_by_ssrc();
  ASSERT_EQ(parsed_incoming_rtp_packets_by_ssrc.size(),
            incoming_rtp_map.size());
  for (const auto& kv : parsed_incoming_rtp_packets_by_ssrc) {
    uint32_t ssrc = kv.ssrc;
    const auto& parsed_rtp_stream = kv.incoming_packets;
    const auto& rtp_stream = incoming_rtp_map[ssrc];
    ASSERT_EQ(parsed_rtp_stream.size(), rtp_stream.size());
    for (size_t i = 0; i < parsed_rtp_stream.size(); i++) {
      test::VerifyLoggedRtpPacketIncoming(*rtp_stream[i], parsed_rtp_stream[i]);
    }
  }

  auto& parsed_outgoing_rtp_packets_by_ssrc =
      parsed_log.outgoing_rtp_packets_by_ssrc();
  ASSERT_EQ(parsed_outgoing_rtp_packets_by_ssrc.size(),
            outgoing_rtp_map.size());
  for (const auto& kv : parsed_outgoing_rtp_packets_by_ssrc) {
    uint32_t ssrc = kv.ssrc;
    const auto& parsed_rtp_stream = kv.outgoing_packets;
    const auto& rtp_stream = outgoing_rtp_map[ssrc];
    ASSERT_EQ(parsed_rtp_stream.size(), rtp_stream.size());
    for (size_t i = 0; i < parsed_rtp_stream.size(); i++) {
      test::VerifyLoggedRtpPacketOutgoing(*rtp_stream[i], parsed_rtp_stream[i]);
    }
  }

  auto& parsed_incoming_rtcp_packets = parsed_log.incoming_rtcp_packets();
  ASSERT_EQ(parsed_incoming_rtcp_packets.size(), incoming_rtcp_list.size());
  for (size_t i = 0; i < parsed_incoming_rtcp_packets.size(); i++) {
    test::VerifyLoggedRtcpPacketIncoming(*incoming_rtcp_list[i],
                                         parsed_incoming_rtcp_packets[i]);
  }

  auto& parsed_outgoing_rtcp_packets = parsed_log.outgoing_rtcp_packets();
  ASSERT_EQ(parsed_outgoing_rtcp_packets.size(), outgoing_rtcp_list.size());
  for (size_t i = 0; i < parsed_outgoing_rtcp_packets.size(); i++) {
    test::VerifyLoggedRtcpPacketOutgoing(*outgoing_rtcp_list[i],
                                         parsed_outgoing_rtcp_packets[i]);
  }
  auto& parsed_audio_recv_configs = parsed_log.audio_recv_configs();
  ASSERT_EQ(parsed_audio_recv_configs.size(), audio_recv_config_list.size());
  for (size_t i = 0; i < parsed_audio_recv_configs.size(); i++) {
    test::VerifyLoggedAudioRecvConfig(*audio_recv_config_list[i],
                                      parsed_audio_recv_configs[i]);
  }
  auto& parsed_audio_send_configs = parsed_log.audio_send_configs();
  ASSERT_EQ(parsed_audio_send_configs.size(), audio_send_config_list.size());
  for (size_t i = 0; i < parsed_audio_send_configs.size(); i++) {
    test::VerifyLoggedAudioSendConfig(*audio_send_config_list[i],
                                      parsed_audio_send_configs[i]);
  }
  auto& parsed_video_recv_configs = parsed_log.video_recv_configs();
  ASSERT_EQ(parsed_video_recv_configs.size(), video_recv_config_list.size());
  for (size_t i = 0; i < parsed_video_recv_configs.size(); i++) {
    test::VerifyLoggedVideoRecvConfig(*video_recv_config_list[i],
                                      parsed_video_recv_configs[i]);
  }
  auto& parsed_video_send_configs = parsed_log.video_send_configs();
  ASSERT_EQ(parsed_video_send_configs.size(), video_send_config_list.size());
  for (size_t i = 0; i < parsed_video_send_configs.size(); i++) {
    test::VerifyLoggedVideoSendConfig(*video_send_config_list[i],
                                      parsed_video_send_configs[i]);
  }

  // Clean up temporary file - can be pretty slow.
  remove(temp_filename.c_str());
}

TEST_P(RtcEventLogSession, LogSessionAndReadBack) {
  EventCounts count;
  count.audio_send_streams = 2;
  count.audio_recv_streams = 2;
  count.video_send_streams = 3;
  count.video_recv_streams = 4;
  count.alr_states = 4;
  count.audio_playouts = 100;
  count.ana_configs = 3;
  count.bwe_loss_events = 20;
  count.bwe_delay_events = 20;
  count.probe_creations = 4;
  count.probe_successes = 2;
  count.probe_failures = 2;
  count.ice_configs = 3;
  count.ice_events = 10;
  count.incoming_rtp_packets = 100;
  count.outgoing_rtp_packets = 100;
  count.incoming_rtcp_packets = 20;
  count.outgoing_rtcp_packets = 20;
  WriteLog(count, 0);
  ReadAndVerifyLog();
}

// TEST_P(RtcEventLogSession, LogSessionAndReadBackWith2Extensions) {
//   RtpHeaderExtensionMap extensions;
//   extensions.Register(kRtpExtensionAbsoluteSendTime,
//                       kAbsoluteSendTimeExtensionId);
//   extensions.Register(kRtpExtensionTransportSequenceNumber,
//                       kTransportSequenceNumberExtensionId);
//   GenerateSessionDescription(4, 4, 1, 1, 0, 0, 0, extensions, 0);
//   WriteSession();
//   ReadAndVerifySession();
// }

// TEST_P(RtcEventLogSession, LogSessionAndReadBackWithAllExtensions) {
//   RtpHeaderExtensionMap extensions;
//   for (uint32_t i = 0; i < kNumExtensions; i++) {
//     extensions.Register(kExtensionTypes[i], kExtensionIds[i]);
//   }
//   GenerateSessionDescription(5, 4, 1, 1, 3, 2, 2, extensions, 2);
//   WriteSession();
//   ReadAndVerifySession();
// }

// TEST_P(RtcEventLogSession, LogLongSessionAndReadBack) {
//   RtpHeaderExtensionMap extensions;
//   for (uint32_t i = 0; i < kNumExtensions; i++) {
//     extensions.Register(kExtensionTypes[i], kExtensionIds[i]);
//   }
//   GenerateSessionDescription(1000, 1000, 250, 250, 200, 100, 100, extensions,
//                              1);
//   WriteSession();
//   ReadAndVerifySession();
// }

// TEST(RtcEventLogTest, CircularBufferKeepsMostRecentEvents) {
//   constexpr size_t kNumEvents = 20000;
//   constexpr int64_t kStartTime = 1000000;

//   auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
//   std::string test_name = test_info->name();
//   std::replace(test_name.begin(), test_name.end(), '/', '_');
//   const std::string temp_filename =
//       test::OutputPath() + "RtcEventLogTest_" + test_name;

//   rtc::ScopedFakeClock fake_clock;
//   fake_clock.SetTimeMicros(kStartTime);

//   // When log_dumper goes out of scope, it causes the log file to be flushed
//   // to disk.
//   std::unique_ptr<RtcEventLog> log_dumper(
//       RtcEventLog::Create(RtcEventLog::EncodingType::Legacy));

//   for (size_t i = 0; i < kNumEvents; i++) {
//     // The purpose of the test is to verify that the log can handle
//     // more events than what fits in the internal circular buffer. The exact
//     // type of events does not matter so we chose AudioPlayouts for
//     simplicity.
//     // We use the index as an ssrc to get a strict relationship between the
//     ssrc
//     // and the timestamp. We use this for some basic consistency checks when
//     we
//     // read back.
//     log_dumper->Log(rtc::MakeUnique<RtcEventAudioPlayout>(i));
//     fake_clock.AdvanceTimeMicros(10000);
//   }
//   log_dumper->StartLogging(
//       rtc::MakeUnique<RtcEventLogOutputFile>(temp_filename, 10000000),
//       RtcEventLog::kImmediateOutput);
//   log_dumper->StopLogging();

//   // Read the generated file from disk.
//   ParsedRtcEventLogNew parsed_log;
//   ASSERT_TRUE(parsed_log.ParseFile(temp_filename));
//   // If the following fails, it probably means that kNumEvents isn't larger
//   // than the size of the cyclic buffer in the event log. Try increasing
//   // kNumEvents.
//   EXPECT_LT(parsed_log.GetNumberOfEvents(), kNumEvents);
//   // We expect a start event, some number of playouts events and a stop
//   event. EXPECT_GT(parsed_log.GetNumberOfEvents(), 2u);

//   test::VerifyLogStartEvent(parsed_log, 0);
//   rtc::Optional<int64_t> last_timestamp;
//   rtc::Optional<uint32_t> last_ssrc;
//   for (size_t i = 1; i < parsed_log.GetNumberOfEvents() - 1; i++) {
//     EXPECT_EQ(parsed_log.GetEventType(i),
//               ParsedRtcEventLogNew::EventType::AUDIO_PLAYOUT_EVENT);
//     LoggedAudioPlayoutEvent playout_event = parsed_log.GetAudioPlayout(i);
//     EXPECT_LT(playout_event.ssrc, kNumEvents);
//     EXPECT_EQ(static_cast<int64_t>(kStartTime + 10000 * playout_event.ssrc),
//               playout_event.timestamp_us);
//     if (last_ssrc)
//       EXPECT_EQ(playout_event.ssrc, *last_ssrc + 1);
//     if (last_timestamp)
//       EXPECT_EQ(playout_event.timestamp_us, *last_timestamp + 10000);
//     last_ssrc = playout_event.ssrc;
//     last_timestamp = playout_event.timestamp_us;
//   }
//   test::VerifyLogEndEvent(parsed_log,
//                                            parsed_log.GetNumberOfEvents() -
//                                            1);
// }

INSTANTIATE_TEST_CASE_P(
    RtcEventLogTest,
    RtcEventLogSession,
    ::testing::Combine(::testing::Values(1234567, 7654321),
                       ::testing::Values(RtcEventLog::kImmediateOutput, 1, 5)));

}  // namespace webrtc
