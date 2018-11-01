/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "api/media_transport_interface.h"
#include "api/test/fake_media_transport.h"
#include "rtc_base/gunit.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

class Callback : public webrtc::MediaTransportStateCallback {
 public:
  explicit Callback(rtc::Thread* should_be_invoked_on)
      : should_be_invoked_on_(should_be_invoked_on) {}

  MediaTransportState state() { return state_; }

 private:
  void OnStateChanged(MediaTransportState state) {
    ASSERT_TRUE(should_be_invoked_on_->IsCurrent());
    state_ = state;
  }

  MediaTransportState state_ = MediaTransportState::kPending;
  rtc::Thread* should_be_invoked_on_;
};

TEST(MediaTransportStateCallbackThreadSafeWrapperTest,
     TestSynchronousCallback) {
  Callback callback(rtc::Thread::Current());
  FakeMediaTransport media_transport(MediaTransportSettings{});
  webrtc::MediaTransportStateCallbackThreadSafeWrapper wrapper(
      rtc::Thread::Current());
  wrapper.SetMediaTransportStateCallback(&callback);
  wrapper.RegisterToMediaTransport(&media_transport);
  media_transport.SetState(MediaTransportState::kWritable);
  ASSERT_EQ(MediaTransportState::kWritable, callback.state());
}

TEST(MediaTransportStateCallbackThreadSafeWrapperTest,
     TestAsynchronousCallback) {
  std::unique_ptr<rtc::Thread> network_thread = rtc::Thread::Create();
  network_thread->Start();
  Callback callback(network_thread.get());
  FakeMediaTransport media_transport(MediaTransportSettings{});
  webrtc::MediaTransportStateCallbackThreadSafeWrapper wrapper(
      network_thread.get());
  wrapper.SetMediaTransportStateCallback(&callback);
  wrapper.RegisterToMediaTransport(&media_transport);
  media_transport.SetState(MediaTransportState::kWritable);
  ASSERT_EQ_WAIT(MediaTransportState::kWritable, callback.state(), 500);
}

}  // namespace
}  // namespace webrtc