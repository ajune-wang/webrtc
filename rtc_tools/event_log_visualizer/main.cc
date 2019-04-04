/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/audio_coding/neteq/include/neteq.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"
#include "rtc_tools/event_log_visualizer/analyzer.h"
#include "rtc_tools/event_log_visualizer/plot_base.h"
#include "rtc_tools/event_log_visualizer/plot_protobuf.h"
#include "rtc_tools/event_log_visualizer/plot_python.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/testsupport/file_utils.h"

WEBRTC_DEFINE_int(
  analyze_ssrc,
  0,
  "Which SSRC to analyze.");

WEBRTC_DEFINE_string(
    plot_profile,
    "default",
    "A profile that selects a certain subset of the plots. Currently "
    "defined profiles are \"all\", \"none\", \"sendside_bwe\","
    "\"receiveside_bwe\" and \"default\"");

WEBRTC_DEFINE_bool(plot_incoming_packet_sizes,
                   false,
                   "Plot bar graph showing the size of each incoming packet.");
WEBRTC_DEFINE_bool(plot_outgoing_packet_sizes,
                   false,
                   "Plot bar graph showing the size of each outgoing packet.");
WEBRTC_DEFINE_bool(plot_incoming_rtcp_types,
                   false,
                   "Plot the RTCP block types for incoming RTCP packets.");
WEBRTC_DEFINE_bool(plot_outgoing_rtcp_types,
                   false,
                   "Plot the RTCP block types for outgoing RTCP packets.");
WEBRTC_DEFINE_bool(
    plot_incoming_packet_count,
    false,
    "Plot the accumulated number of packets for each incoming stream.");
WEBRTC_DEFINE_bool(
    plot_outgoing_packet_count,
    false,
    "Plot the accumulated number of packets for each outgoing stream.");
WEBRTC_DEFINE_bool(
    plot_audio_playout,
    false,
    "Plot bar graph showing the time between each audio playout.");
WEBRTC_DEFINE_bool(
    plot_audio_level,
    false,
    "Plot line graph showing the audio level of incoming audio.");
WEBRTC_DEFINE_bool(
    plot_incoming_sequence_number_delta,
    false,
    "Plot the sequence number difference between consecutive incoming "
    "packets.");
WEBRTC_DEFINE_bool(
    plot_incoming_delay,
    true,
    "Plot the 1-way path delay for incoming packets, normalized so "
    "that the first packet has delay 0.");
WEBRTC_DEFINE_bool(
    plot_incoming_loss_rate,
    true,
    "Compute the loss rate for incoming packets using a method that's "
    "similar to the one used for RTCP SR and RR fraction lost. Note "
    "that the loss rate can be negative if packets are duplicated or "
    "reordered.");
WEBRTC_DEFINE_bool(plot_incoming_bitrate,
                   true,
                   "Plot the total bitrate used by all incoming streams.");
WEBRTC_DEFINE_bool(plot_outgoing_bitrate,
                   true,
                   "Plot the total bitrate used by all outgoing streams.");
WEBRTC_DEFINE_bool(plot_incoming_stream_bitrate,
                   true,
                   "Plot the bitrate used by each incoming stream.");
WEBRTC_DEFINE_bool(plot_outgoing_stream_bitrate,
                   true,
                   "Plot the bitrate used by each outgoing stream.");
WEBRTC_DEFINE_bool(plot_incoming_layer_bitrate_allocation,
                   false,
                   "Plot the target bitrate for each incoming layer. Requires "
                   "incoming RTCP XR with target bitrate to be populated.");
WEBRTC_DEFINE_bool(plot_outgoing_layer_bitrate_allocation,
                   false,
                   "Plot the target bitrate for each outgoing layer. Requires "
                   "outgoing RTCP XR with target bitrate to be populated.");
WEBRTC_DEFINE_bool(
    plot_simulated_receiveside_bwe,
    false,
    "Run the receive-side bandwidth estimator with the incoming rtp "
    "packets and plot the resulting estimate.");
WEBRTC_DEFINE_bool(
    plot_simulated_sendside_bwe,
    false,
    "Run the send-side bandwidth estimator with the outgoing rtp and "
    "incoming rtcp and plot the resulting estimate.");
WEBRTC_DEFINE_bool(plot_simulated_goog_cc,
                   false,
                   "Run the GoogCC congestion controller based on the logged "
                   "events and plot the target bitrate.");
WEBRTC_DEFINE_bool(
    plot_network_delay_feedback,
    true,
    "Compute network delay based on sent packets and the received "
    "transport feedback.");
WEBRTC_DEFINE_bool(
    plot_fraction_loss_feedback,
    true,
    "Plot packet loss in percent for outgoing packets (as perceived by "
    "the send-side bandwidth estimator).");
WEBRTC_DEFINE_bool(
    plot_pacer_delay,
    false,
    "Plot the time each sent packet has spent in the pacer (based on "
    "the difference between the RTP timestamp and the send "
    "timestamp).");
WEBRTC_DEFINE_bool(
    plot_timestamps,
    false,
    "Plot the rtp timestamps of all rtp and rtcp packets over time.");
WEBRTC_DEFINE_bool(
    plot_rtcp_details,
    false,
    "Plot the contents of all report blocks in all sender and receiver "
    "reports. This includes fraction lost, cumulative number of lost "
    "packets, extended highest sequence number and time since last "
    "received SR.");
WEBRTC_DEFINE_bool(plot_audio_encoder_bitrate_bps,
                   false,
                   "Plot the audio encoder target bitrate.");
WEBRTC_DEFINE_bool(plot_audio_encoder_frame_length_ms,
                   false,
                   "Plot the audio encoder frame length.");
WEBRTC_DEFINE_bool(
    plot_audio_encoder_packet_loss,
    false,
    "Plot the uplink packet loss fraction which is sent to the audio encoder.");
WEBRTC_DEFINE_bool(plot_audio_encoder_fec,
                   false,
                   "Plot the audio encoder FEC.");
WEBRTC_DEFINE_bool(plot_audio_encoder_dtx,
                   false,
                   "Plot the audio encoder DTX.");
WEBRTC_DEFINE_bool(plot_audio_encoder_num_channels,
                   false,
                   "Plot the audio encoder number of channels.");
WEBRTC_DEFINE_bool(plot_neteq_stats, false, "Plot the NetEq statistics.");
WEBRTC_DEFINE_bool(plot_ice_candidate_pair_config,
                   false,
                   "Plot the ICE candidate pair config events.");
WEBRTC_DEFINE_bool(plot_ice_connectivity_check,
                   false,
                   "Plot the ICE candidate pair connectivity checks.");
WEBRTC_DEFINE_bool(plot_dtls_transport_state,
                   false,
                   "Plot DTLS transport state changes.");
WEBRTC_DEFINE_bool(plot_dtls_writable_state,
                   false,
                   "Plot DTLS writable state changes.");

WEBRTC_DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enabled/"
    " will assign the group Enabled to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");
WEBRTC_DEFINE_string(wav_filename,
                     "",
                     "Path to wav file used for simulation of jitter buffer");
WEBRTC_DEFINE_bool(help, false, "prints this message");

WEBRTC_DEFINE_bool(
    show_detector_state,
    false,
    "Show the state of the delay based BWE detector on the total "
    "bitrate graph");

WEBRTC_DEFINE_bool(show_alr_state,
                   false,
                   "Show the state ALR state on the total bitrate graph");

WEBRTC_DEFINE_bool(
    parse_unconfigured_header_extensions,
    true,
    "Attempt to parse unconfigured header extensions using the default "
    "WebRTC mapping. This can give very misleading results if the "
    "application negotiates a different mapping.");

WEBRTC_DEFINE_bool(print_triage_alerts,
                   false,
                   "Print triage alerts, i.e. a list of potential problems.");

WEBRTC_DEFINE_bool(
    normalize_time,
    true,
    "Normalize the log timestamps so that the call starts at time 0.");

WEBRTC_DEFINE_bool(protobuf_output,
                   false,
                   "Output charts as protobuf instead of python code.");

void SetAllPlotFlags(bool setting);

int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "A tool for visualizing WebRTC event logs.\n"
      "Example usage:\n" +
      program_name + " <logfile> | python\n" + "Run " + program_name +
      " --help for a list of command line options\n";

  // Parse command line flags without removing them. We're only interested in
  // the |plot_profile| flag.
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, false);
  if (strcmp(FLAG_plot_profile, "all") == 0) {
    SetAllPlotFlags(true);
  } else if (strcmp(FLAG_plot_profile, "none") == 0) {
    SetAllPlotFlags(false);
  } else if (strcmp(FLAG_plot_profile, "sendside_bwe") == 0) {
    SetAllPlotFlags(false);
    FLAG_plot_outgoing_packet_sizes = true;
    FLAG_plot_outgoing_bitrate = true;
    FLAG_plot_outgoing_stream_bitrate = true;
    FLAG_plot_simulated_sendside_bwe = true;
    FLAG_plot_network_delay_feedback = true;
    FLAG_plot_fraction_loss_feedback = true;
  } else if (strcmp(FLAG_plot_profile, "receiveside_bwe") == 0) {
    SetAllPlotFlags(false);
    FLAG_plot_incoming_packet_sizes = true;
    FLAG_plot_incoming_delay = true;
    FLAG_plot_incoming_loss_rate = true;
    FLAG_plot_incoming_bitrate = true;
    FLAG_plot_incoming_stream_bitrate = true;
    FLAG_plot_simulated_receiveside_bwe = true;
  } else if (strcmp(FLAG_plot_profile, "default") == 0) {
    // Do nothing.
  } else {
    rtc::Flag* plot_profile_flag = rtc::FlagList::Lookup("plot_profile");
    RTC_CHECK(plot_profile_flag);
    plot_profile_flag->Print(false);
  }
  // Parse the remaining flags. They are applied relative to the chosen profile.
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);

  if (argc != 2 || FLAG_help) {
    // Print usage information.
    std::cout << usage;
    if (FLAG_help)
      rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  webrtc::test::ValidateFieldTrialsStringOrDie(FLAG_force_fieldtrials);
  // InitFieldTrialsFromString stores the char*, so the char array must outlive
  // the application.
  webrtc::field_trial::InitFieldTrialsFromString(FLAG_force_fieldtrials);

  std::string filename = argv[1];

  webrtc::ParsedRtcEventLog::UnconfiguredHeaderExtensions header_extensions =
      webrtc::ParsedRtcEventLog::UnconfiguredHeaderExtensions::kDontParse;
  if (FLAG_parse_unconfigured_header_extensions) {
    header_extensions = webrtc::ParsedRtcEventLog::
        UnconfiguredHeaderExtensions::kAttemptWebrtcDefaultConfig;
  }
  webrtc::ParsedRtcEventLog parsed_log(header_extensions);

  if (!parsed_log.ParseFile(filename)) {
    std::cerr << "Could not parse the entire log file." << std::endl;
    std::cerr << "Only the parsable events will be analyzed." << std::endl;
  }

  for (const auto& logged_rtp_stream_incoming : parsed_log.incoming_rtp_packets_by_ssrc()) {
    if (logged_rtp_stream_incoming.ssrc == static_cast<uint32_t>(FLAG_analyze_ssrc)) {
      for (const auto& incoming_packet : logged_rtp_stream_incoming.incoming_packets) {
        std::cerr << incoming_packet.rtp.header.timestamp << std::endl;
      }
    }
  }

  return 0;
}

void SetAllPlotFlags(bool setting) {
  FLAG_plot_incoming_packet_sizes = setting;
  FLAG_plot_outgoing_packet_sizes = setting;
  FLAG_plot_incoming_rtcp_types = setting;
  FLAG_plot_outgoing_rtcp_types = setting;
  FLAG_plot_incoming_packet_count = setting;
  FLAG_plot_outgoing_packet_count = setting;
  FLAG_plot_audio_playout = setting;
  FLAG_plot_audio_level = setting;
  FLAG_plot_incoming_sequence_number_delta = setting;
  FLAG_plot_incoming_delay = setting;
  FLAG_plot_incoming_loss_rate = setting;
  FLAG_plot_incoming_bitrate = setting;
  FLAG_plot_outgoing_bitrate = setting;
  FLAG_plot_incoming_stream_bitrate = setting;
  FLAG_plot_outgoing_stream_bitrate = setting;
  FLAG_plot_incoming_layer_bitrate_allocation = setting;
  FLAG_plot_outgoing_layer_bitrate_allocation = setting;
  FLAG_plot_simulated_receiveside_bwe = setting;
  FLAG_plot_simulated_sendside_bwe = setting;
  FLAG_plot_simulated_goog_cc = setting;
  FLAG_plot_network_delay_feedback = setting;
  FLAG_plot_fraction_loss_feedback = setting;
  FLAG_plot_timestamps = setting;
  FLAG_plot_rtcp_details = setting;
  FLAG_plot_audio_encoder_bitrate_bps = setting;
  FLAG_plot_audio_encoder_frame_length_ms = setting;
  FLAG_plot_audio_encoder_packet_loss = setting;
  FLAG_plot_audio_encoder_fec = setting;
  FLAG_plot_audio_encoder_dtx = setting;
  FLAG_plot_audio_encoder_num_channels = setting;
  FLAG_plot_neteq_stats = setting;
  FLAG_plot_ice_candidate_pair_config = setting;
  FLAG_plot_ice_connectivity_check = setting;
  FLAG_plot_pacer_delay = setting;
}
