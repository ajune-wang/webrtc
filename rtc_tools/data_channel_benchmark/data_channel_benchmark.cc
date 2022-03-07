/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <inttypes.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "rtc_base/event.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_tools/data_channel_benchmark/grpc_signaling.h"
#include "rtc_tools/data_channel_benchmark/peer_connection_client.h"
#include "system_wrappers/include/field_trial.h"

ABSL_FLAG(bool, server, false, "Server mode");
ABSL_FLAG(bool, oneshot, true, "Terminate after serving a client");
ABSL_FLAG(std::string, address, "localhost", "Connect to server address");
ABSL_FLAG(uint16_t, port, 0, "Connect to port (0 for random)");
ABSL_FLAG(uint64_t, transfer_size, 2, "Transfer size (MiB)");
ABSL_FLAG(uint64_t, packet_size, 256 * 1024, "Packet size");
ABSL_FLAG(std::string,
          force_fieldtrials,
          "",
          "Field trials control experimental feature code which can be forced. "
          "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
          " will assign the group Enable to field trial WebRTC-FooFeature.");

class DataChannelObserverImpl : public webrtc::DataChannelObserver {
 public:
  explicit DataChannelObserverImpl(webrtc::DataChannelInterface* dc)
      : dc_(dc),
        bytes_received_(0),
        want_low_buffered_threshold_notification_(false) {}
  void OnStateChange() override {
    RTC_LOG(LS_INFO) << "State changed to " << dc_->state();
    switch (dc_->state()) {
      case webrtc::DataChannelInterface::DataState::kOpen:
        open_notification_.Set();
        break;
      case webrtc::DataChannelInterface::DataState::kClosed:
        closed_notification_.Set();
        break;
      default:
        break;
    }
  }
  void OnMessage(const webrtc::DataBuffer& buffer) override {
    bytes_received_ += buffer.data.size();
    if (bytes_received_threshold_ &&
        bytes_received_ >= bytes_received_threshold_) {
      bytes_received_notification_.Set();
    }
  }
  void OnBufferedAmountChange(uint64_t sent_data_size) override {
    if (want_low_buffered_threshold_notification_ &&
        dc_->buffered_amount() <
            webrtc::DataChannelInterface::MaxSendQueueSize() / 2) {
      want_low_buffered_threshold_notification_ = false;
      low_buffered_threshold_notification_.Set();
    }
  }

  bool WaitForOpenState(int duration_ms) {
    return dc_->state() == webrtc::DataChannelInterface::DataState::kOpen ||
           open_notification_.Wait(duration_ms);
  }
  bool WaitForClosedState(int duration_ms) {
    return dc_->state() == webrtc::DataChannelInterface::DataState::kClosed ||
           closed_notification_.Wait(duration_ms);
  }

  void SetBytesReceivedThreshold(uint64_t bytes_received_threshold) {
    bytes_received_threshold_ = bytes_received_threshold;
    if (bytes_received_ >= bytes_received_threshold_)
      bytes_received_notification_.Set();
  }
  bool WaitForBytesReceivedThreshold(int duration_ms) {
    return (bytes_received_threshold_ &&
            bytes_received_ >= bytes_received_threshold_) ||
           bytes_received_notification_.Wait(duration_ms);
  }

  void SetWantLowbufferedThreshold(bool set) {
    if (set) {
      want_low_buffered_threshold_notification_ = true;
    } else {
      low_buffered_threshold_notification_.Reset();
    }
  }
  bool WaitForLowbufferedThreshold(int duration_ms) {
    return low_buffered_threshold_notification_.Wait(duration_ms);
  }

 private:
  webrtc::DataChannelInterface* dc_;
  rtc::Event open_notification_;
  rtc::Event closed_notification_;
  rtc::Event bytes_received_notification_;
  absl::optional<uint64_t> bytes_received_threshold_;
  uint64_t bytes_received_;
  rtc::Event low_buffered_threshold_notification_;
  std::atomic<bool> want_low_buffered_threshold_notification_;
};

int main(int argc, char** argv) {
  rtc::InitializeSSL();
  absl::ParseCommandLine(argc, argv);

  bool is_server = absl::GetFlag(FLAGS_server);
  bool oneshot = absl::GetFlag(FLAGS_oneshot);
  uint16_t port = absl::GetFlag(FLAGS_port);
  uint64_t transfer_size = absl::GetFlag(FLAGS_transfer_size) * 1024 * 1024;
  uint64_t packet_size = absl::GetFlag(FLAGS_packet_size);
  std::string server_address = absl::GetFlag(FLAGS_address);
  std::string field_trials = absl::GetFlag(FLAGS_force_fieldtrials);

  webrtc::field_trial::InitFieldTrialsFromString(field_trials.c_str());

  auto signaling_thread = rtc::Thread::Create();
  signaling_thread->Start();

  if (is_server) {
    // Start server
    auto factory = webrtc::PeerConnectionClient::CreateDefaultFactory(
        signaling_thread.get());

    auto grpc_server = webrtc::GrpcSignalingServer::Create(
        [factory = rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>(
             factory),
         transfer_size, packet_size](webrtc::SignalingInterface* signaling) {
          webrtc::PeerConnectionClient client(factory.get(), signaling);
          client.StartPeerConnection();
          auto peer_connection = client.peerConnection();

          auto dc_or_error =
              peer_connection->CreateDataChannelOrError("benchmark", nullptr);
          auto data_channel = dc_or_error.MoveValue();
          auto data_channel_observer =
              std::make_unique<DataChannelObserverImpl>(data_channel);
          data_channel->RegisterObserver(data_channel_observer.get());
          data_channel_observer->WaitForOpenState(rtc::Event::kForever);

          // Wait for the sender and receiver peers to stabilize
          absl::SleepFor(absl::Seconds(1));

          std::string data(packet_size, '0');
          int64_t remaining_data = (int64_t)transfer_size;

          auto begin_time = absl::Now();

          while (remaining_data > 0ll) {
            if (remaining_data < (int64_t)data.size())
              data.resize(remaining_data);

            rtc::CopyOnWriteBuffer buffer(data);
            webrtc::DataBuffer data_buffer(buffer, true);
            if (!data_channel->Send(data_buffer)) {
              data_channel_observer->SetWantLowbufferedThreshold(true);
              data_channel_observer->WaitForLowbufferedThreshold(
                  rtc::Event::kForever);
              data_channel_observer->SetWantLowbufferedThreshold(false);
              continue;
            }
            remaining_data -= buffer.size();
            fprintf(stderr,
                    "Progress: %" PRId64 " / %" PRId64 " (%" PRId64 "%%)\n",
                    (transfer_size - remaining_data), transfer_size,
                    (100 - remaining_data * 100 / transfer_size));
          }

          data_channel_observer->WaitForClosedState(rtc::Event::kForever);
          data_channel->UnregisterObserver();

          auto end_time = absl::Now();
          auto duration_ms = absl::ToDoubleMilliseconds(end_time - begin_time);
          printf("Elapsed time: %gms %gMB/s\n", duration_ms,
                 ((static_cast<double>(transfer_size) / 1024 / 1024) /
                  (duration_ms / 1000)));
        },
        port, oneshot);
    grpc_server->Start();
    grpc_server->Wait();
  } else {
    auto factory = webrtc::PeerConnectionClient::CreateDefaultFactory(
        signaling_thread.get());
    auto grpc_client = webrtc::GrpcSignalingClient::Create(
        server_address + ":" + std::to_string(port));
    webrtc::PeerConnectionClient client(factory.get(),
                                        grpc_client->signaling_client());

    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel;
    rtc::Event got_data_channel;
    client.SetOnDataChannel(
        [&data_channel, &got_data_channel](
            rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
          data_channel = channel;
          got_data_channel.Set();
        });

    if (!grpc_client->Start()) {
      fprintf(stderr, "Failed to connect to server\n");
      return 1;
    }

    got_data_channel.Wait(rtc::Event::kForever);

    // DataChannel needs an observer to start draining the read queue
    DataChannelObserverImpl observer(data_channel.get());
    observer.SetBytesReceivedThreshold(transfer_size);
    data_channel->RegisterObserver(&observer);

    observer.WaitForBytesReceivedThreshold(rtc::Event::kForever);
    data_channel->UnregisterObserver();
    data_channel->Close();
  }

  signaling_thread->Quit();

  return 0;
}
