/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "video/end_to_end_tests/congestion_controller_test.h"

#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"

namespace webrtc {
namespace test {

CallStatsPrinter::CallStatsPrinter(std::string filename) {
  output_file_ = fopen(filename.c_str(), "w");
  RTC_CHECK(output_file_);
  output_ = output_file_;
}

CallStatsPrinter::CallStatsPrinter() {
  output_ = stdout;
}

CallStatsPrinter::~CallStatsPrinter() {
  if (output_file_)
    fclose(output_file_);
}

void CallStatsPrinter::PrintHeaders() {
  fprintf(output_, "time pacer_delay target_bitrate media_bitrate\n");
}

void CallStatsPrinter::PrintStats(int64_t time_ms,
                                  int64_t pacer_delay_ms,
                                  int64_t target_bitrate_bps,
                                  int64_t media_bitrate_bps) {
  fprintf(output_, "%.3lf %.3lf %.0lf %.0lf\n", time_ms / 1000.0,
          pacer_delay_ms / 1000.0, target_bitrate_bps / 8.0,
          media_bitrate_bps / 8.0);
}

BaseCongestionControllerTest::BaseCongestionControllerTest(
    unsigned int timeout_ms,
    std::string filepath_base)
    : EndToEndTest(timeout_ms), filepath_base_(filepath_base) {
  send_event_log_ = RtcEventLog::Create(RtcEventLog::EncodingType::Legacy);
  bool send_ok = send_event_log_->StartLogging(
      rtc::MakeUnique<RtcEventLogOutputFile>(filepath_base_ + "_send",
                                             RtcEventLog::kUnlimitedOutput),
      RtcEventLog::kImmediateOutput);
  RTC_CHECK(send_ok);

  recv_event_log_ = RtcEventLog::Create(RtcEventLog::EncodingType::Legacy);
  bool recv_ok = recv_event_log_->StartLogging(
      rtc::MakeUnique<RtcEventLogOutputFile>(filepath_base_ + "_recv",
                                             RtcEventLog::kUnlimitedOutput),
      RtcEventLog::kImmediateOutput);
  RTC_CHECK(recv_ok);
}

BaseCongestionControllerTest::~BaseCongestionControllerTest() {
  if (send_state_out_)
    fclose(send_state_out_);
  if (return_state_out_)
    fclose(return_state_out_);
}

void BaseCongestionControllerTest::OnCallsCreated(Call* sender_call,
                                                  Call* receiver_call) {
  sender_call_ = sender_call;
  return_call_ = receiver_call;
}

void BaseCongestionControllerTest::OnVideoStreamsCreated(
    VideoSendStream* send_stream,
    const std::vector<VideoReceiveStream*>& receive_streams) {
  send_stream_ = send_stream;
  receive_streams_ = receive_streams;
}

std::pair<std::unique_ptr<NetworkControllerFactoryInterface>,
          std::unique_ptr<DebugStatePrinter> >
BaseCongestionControllerTest::CreateSendCCFactory(RtcEventLog* event_log) {
  return {nullptr, nullptr};
}

std::pair<std::unique_ptr<NetworkControllerFactoryInterface>,
          std::unique_ptr<DebugStatePrinter> >
BaseCongestionControllerTest::CreateReturnCCFactory(RtcEventLog* event_log) {
  return {nullptr, nullptr};
}

Call::Config BaseCongestionControllerTest::GetSenderCallConfig() {
  Call::Config config(send_event_log_.get());
  RTC_DCHECK(send_cc_factory_ == nullptr);
  std::unique_ptr<DebugStatePrinter> cc_printer;
  std::tie(send_cc_factory_, cc_printer) =
      CreateSendCCFactory(send_event_log_.get());
  if (send_cc_factory_) {
    config.network_controller_factory = send_cc_factory_.get();
    send_state_out_ = fopen((filepath_base_ + "_send.state.txt").c_str(), "w");
    send_printer_.reset(
        new ControlStatePrinter(send_state_out_, std::move(cc_printer)));
  }
  send_stats_printer_ =
      rtc::MakeUnique<CallStatsPrinter>(filepath_base_ + "_send.stats.txt");
  send_stats_printer_->PrintHeaders();
  return config;
}

Call::Config BaseCongestionControllerTest::GetReceiverCallConfig() {
  Call::Config config(recv_event_log_.get());
  RTC_DCHECK(return_cc_factory_ == nullptr);
  std::unique_ptr<DebugStatePrinter> cc_printer;
  std::tie(return_cc_factory_, cc_printer) =
      CreateReturnCCFactory(recv_event_log_.get());
  if (return_cc_factory_) {
    config.network_controller_factory = return_cc_factory_.get();
    return_state_out_ =
        fopen((filepath_base_ + "_return.state.txt").c_str(), "w");
    return_printer_.reset(
        new ControlStatePrinter(return_state_out_, std::move(cc_printer)));
  }
  return_stats_printer_ =
      rtc::MakeUnique<CallStatsPrinter>(filepath_base_ + "_return.stats.txt");
  return_stats_printer_->PrintHeaders();
  return config;
}

void BaseCongestionControllerTest::PrintStates(int64_t timestamp_ms) {
  Timestamp now = Timestamp::ms(timestamp_ms);
  if (send_printer_) {
    send_printer_->PrintState(now);
  }
  if (send_printer_) {
    return_printer_->PrintState(now);
  }
}

void BaseCongestionControllerTest::PrintStats(int64_t timestamp_ms) {
  if (send_stream_ && sender_call_) {
    VideoSendStream::Stats video_stats = send_stream_->GetStats();
    Call::Stats call_stats = sender_call_->GetStats();
    send_stats_printer_->PrintStats(timestamp_ms, call_stats.pacer_delay_ms,
                                    video_stats.target_media_bitrate_bps,
                                    video_stats.media_bitrate_bps);
  }
}
}  // namespace test
}  // namespace webrtc
