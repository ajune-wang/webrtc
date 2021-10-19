/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <chrono>
#include <thread>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
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
      : dc_(dc) {}
  void OnStateChange() override {
    RTC_LOG(LS_INFO) << "State changed to " << dc_->state();
  }
  void OnMessage(const webrtc::DataBuffer& buffer) override {}
  void OnBufferedAmountChange(uint64_t sent_data_size) override {}

 private:
  webrtc::DataChannelInterface* dc_;
};

template <typename Callback>
void WaitFor(Callback cb) {
  while (!cb()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

int main(int argc, char** argv) {
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

          WaitFor([&] {
            return data_channel->state() == webrtc::DataChannelInterface::kOpen;
          });

          std::this_thread::sleep_for(std::chrono::seconds(1));

          std::string data(packet_size, '0');
          int64_t remaining_data = (int64_t)transfer_size;

          auto begin_time = std::chrono::high_resolution_clock::now();

          while (remaining_data > 0ll) {
            if (remaining_data < (int64_t)data.size())
              data.resize(remaining_data);

            rtc::CopyOnWriteBuffer buffer(data);
            webrtc::DataBuffer data_buffer(buffer, true);
            if (!data_channel->Send(data_buffer)) {
              WaitFor([&] {
                auto low_watermark =
                    webrtc::DataChannelInterface::MaxSendQueueSize() / 2;
                return data_channel->buffered_amount() <= low_watermark;
              });
              continue;
            }
            remaining_data -= buffer.size();
            fprintf(stderr, "Progress: %llu / %llu (%llu%%)\n",
                    (transfer_size - remaining_data), transfer_size,
                    (100 - remaining_data * 100 / transfer_size));
          }

          WaitFor([&] {
            return data_channel->state() ==
                   webrtc::DataChannelInterface::kClosed;
          });
          auto end_time = std::chrono::high_resolution_clock::now();
          auto duration_ms =
              std::chrono::duration<double, std::milli>(end_time - begin_time)
                  .count();
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
    if (!grpc_client->Start()) {
      fprintf(stderr, "Failed to connect to server\n");
      return 1;
    }

    WaitFor([&] { return client.dataChannels().size() >= 1; });
    auto data_channel = client.dataChannels().front();

    // DataChannel needs an observer to start draining the read queue
    DataChannelObserverImpl observer(data_channel.get());
    data_channel->RegisterObserver(&observer);

    WaitFor([&] { return data_channel->bytes_received() == transfer_size; });
    data_channel->UnregisterObserver();
    data_channel->Close();
  }

  signaling_thread->Quit();

  return 0;
}
