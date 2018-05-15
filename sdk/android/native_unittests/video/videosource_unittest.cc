/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "api/video/video_sink_interface.h"
#include "sdk/android/generated_native_unittests_jni/jni/JavaVideoSourceTestHelper_jni.h"
#include "sdk/android/native_api/video/videosource.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {
class TestVideoSink : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  void OnFrame(const VideoFrame& frame) { frames_.push_back(frame); }

  std::vector<VideoFrame> GetFrames() {
    std::vector<VideoFrame> temp = frames_;
    frames_.clear();
    return temp;
  }

 private:
  std::vector<VideoFrame> frames_;
};
}  // namespace

TEST(JavaVideoSourceTest, CreateJavaVideoSource) {
  TestVideoSink test_video_sink;

  JNIEnv* env = AttachCurrentThreadIfNeeded();

  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName("signaling_thread", NULL);
  RTC_CHECK(signaling_thread->Start()) << "Failed to start thread";

  rtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(env, signaling_thread.get(),
                            false /* is_screencast */);
  video_track_source->AddOrUpdateSink(&test_video_sink, rtc::VideoSinkWants());

  Java_JavaVideoSourceTestHelper_deliverFrame(
      env, video_track_source->GetJavaVideoCapturerObserver(env));

  std::vector<VideoFrame> frames = test_video_sink.GetFrames();
  EXPECT_EQ(1u, frames.size());
  webrtc::VideoFrame frame = frames[0];
  EXPECT_EQ(2, frame.width());
  EXPECT_EQ(3, frame.height());
  EXPECT_EQ(180, frame.rotation());
}

}  // namespace test
}  // namespace webrtc
