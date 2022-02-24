/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_TOOLS_DATA_CHANNEL_BENCHMARK_GRPC_SIGNALING_H_
#define RTC_TOOLS_DATA_CHANNEL_BENCHMARK_GRPC_SIGNALING_H_

#include <memory>
#include <string>

#include "api/jsep.h"
#include "rtc_tools/data_channel_benchmark/signaling_interface.h"

namespace webrtc {
class GrpcSignalingServer {
 public:
  virtual ~GrpcSignalingServer() = default;
  virtual void Start() = 0;
  virtual void Wait() = 0;
  virtual void Stop() = 0;
  virtual int SelectedPort() = 0;

  static std::unique_ptr<GrpcSignalingServer> Create(
      std::function<void(webrtc::SignalingInterface*)> callback,
      int port,
      bool oneshot);
};

class GrpcSignalingClient {
 public:
  virtual ~GrpcSignalingClient() = default;
  virtual bool Start() = 0;
  virtual webrtc::SignalingInterface* signaling_client() = 0;

  static std::unique_ptr<GrpcSignalingClient> Create(const std::string& server);
};

}  // namespace webrtc
#endif  // RTC_TOOLS_DATA_CHANNEL_BENCHMARK_GRPC_SIGNALING_H_
