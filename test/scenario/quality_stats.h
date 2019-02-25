/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_QUALITY_STATS_H_
#define TEST_SCENARIO_QUALITY_STATS_H_

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "rtc_base/ref_counter.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "test/logging/log_writer.h"
#include "test/scenario/quality_info.h"
#include "test/scenario/scenario_config.h"
#include "test/statistics.h"

namespace webrtc {
namespace test {

template <typename T>
class RefCountWrapper : public T, public rtc::RefCountInterface {
 public:
  explicit RefCountWrapper(T&& obj) : T(std::move(obj)) {}
  void AddRef() const override { ref_count_.IncRef(); }
  rtc::RefCountReleaseStatus Release() const override {
    const auto status = ref_count_.DecRef();
    if (status == rtc::RefCountReleaseStatus::kDroppedLastRef) {
      delete this;
    }
    return status;
  }
  virtual bool HasOneRef() const { return ref_count_.HasOneRef(); }

 protected:
  ~RefCountWrapper() override {}
  mutable webrtc::webrtc_impl::RefCounter ref_count_{0};
};

template <typename T>
using refcount_ptr = rtc::scoped_refptr<RefCountWrapper<T>>;

template <typename T>
refcount_ptr<T> MakeRefCountPtr(T&& obj) {
  return rtc::scoped_refptr<RefCountWrapper<T>>(
      new RefCountWrapper<T>(std::move(obj)));
}

template <typename T>
refcount_ptr<T> MakeNullRefCountPtr() {
  return rtc::scoped_refptr<RefCountWrapper<T>>();
}

class VideoFrameMatcher {
 public:
  explicit VideoFrameMatcher(
      std::vector<std::function<void(const VideoFramePair&)>>
          frame_pair_handlers);
  ~VideoFrameMatcher();
  void RegisterLayer(int layer_id);
  void OnCapturedFrame(const VideoFrame& frame, Timestamp at_time);
  void OnDecodedFrame(const VideoFrame& frame,
                      Timestamp render_time,
                      int layer_id);
  bool Active() const;
  Clock* clock();

 private:
  struct DecodedFrame {
    int id;
    Timestamp render_time;
    rtc::scoped_refptr<VideoFrameBuffer> frame;
    rtc::scoped_refptr<VideoFrameBuffer> thumb;
    int repeat_count;
  };
  struct CapturedFrame {
    int id;
    Timestamp capture_time;
    rtc::scoped_refptr<VideoFrameBuffer> frame;
    rtc::scoped_refptr<VideoFrameBuffer> thumb;
    double best_score;
    refcount_ptr<DecodedFrame> best_decode;
  };
  struct VideoLayer {
    int layer_id;
    std::deque<CapturedFrame> captured_frames;
    refcount_ptr<DecodedFrame> last_decode;
    int next_decoded_id = 0;
  };
  void HandleMatch(CapturedFrame& captured, int layer_id) {
    VideoFramePair frame_pair;
    frame_pair.layer_id = layer_id;
    frame_pair.captured = captured.frame;
    frame_pair.capture_id = captured.id;
    if (captured.best_decode) {
      frame_pair.decode_id = captured.best_decode->id;
      frame_pair.capture_time = captured.capture_time;
      frame_pair.decoded = captured.best_decode->frame;
      frame_pair.render_time = captured.best_decode->render_time;
      frame_pair.repeated = captured.best_decode->repeat_count++;
    }
    for (auto& handler : frame_pair_handlers_)
      handler(frame_pair);
  }
  void Finalize();
  int next_capture_id_ = 0;
  std::vector<std::function<void(const VideoFramePair&)>> frame_pair_handlers_;
  std::map<int, VideoLayer> layers_;
  rtc::TaskQueue task_queue_;
};

class ForwardingCapturedFrameTap
    : public rtc::VideoSinkInterface<VideoFrame>,
      public rtc::VideoSourceInterface<VideoFrame> {
 public:
  ForwardingCapturedFrameTap(Clock* clock,
                             VideoFrameMatcher* matcher,
                             rtc::VideoSourceInterface<VideoFrame>* source);
  ForwardingCapturedFrameTap(ForwardingCapturedFrameTap&) = delete;
  ForwardingCapturedFrameTap& operator=(ForwardingCapturedFrameTap&) = delete;
  ~ForwardingCapturedFrameTap();

  // VideoSinkInterface interface
  void OnFrame(const VideoFrame& frame) override;
  void OnDiscardedFrame() override;

  // VideoSourceInterface interface
  void AddOrUpdateSink(VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(VideoSinkInterface<VideoFrame>* sink) override;
  VideoFrame PopFrame();

 private:
  Clock* const clock_;
  VideoFrameMatcher* const matcher_;
  rtc::VideoSourceInterface<VideoFrame>* const source_;
  VideoSinkInterface<VideoFrame>* sink_;
  int discarded_count_ = 0;
};

class DecodedFrameTap : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  explicit DecodedFrameTap(VideoFrameMatcher* matcher, int layer_id);
  // VideoSinkInterface interface
  void OnFrame(const VideoFrame& frame) override;

 private:
  VideoFrameMatcher* const matcher_;
  int layer_id_;
};

struct VideoQualityStats {
  int total = 0;
  int valid = 0;
  int lost = 0;
  Statistics end_to_end_seconds;
  Statistics frame_size;
  Statistics psnr;
  Statistics ssim;
};

class VideoQualityAnalyzer {
 public:
  explicit VideoQualityAnalyzer(
      std::unique_ptr<RtcEventLogOutput> writer = nullptr);
  ~VideoQualityAnalyzer();
  void HandleFramePair(VideoFramePair sample);
  VideoQualityStats stats() const;
  void PrintHeaders();
  void PrintFrameInfo(const VideoFramePair& sample);
  std::function<void(const VideoFramePair&)> Handler();

 private:
  VideoQualityStats stats_;
  const std::unique_ptr<RtcEventLogOutput> writer_;
};
}  // namespace test
}  // namespace webrtc
#endif  // TEST_SCENARIO_QUALITY_STATS_H_
