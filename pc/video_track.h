/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_VIDEO_TRACK_H_
#define PC_VIDEO_TRACK_H_

#include <memory>
#include <string>

#include "api/media_stream_interface.h"
#include "api/media_stream_track.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "media/base/video_source_base.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class VideoTrack : public MediaStreamTrack<VideoTrackInterface>,
                   public rtc::VideoSourceBaseGuarded {
 public:
  static rtc::scoped_refptr<VideoTrack> Create(
      const std::string& label,
      VideoTrackSourceInterface* source,
      rtc::Thread* worker_thread);

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;
  VideoTrackSourceInterface* GetSource() const override;

  ContentHint content_hint() const override;
  void set_content_hint(ContentHint hint) override;
  bool set_enabled(bool enable) override;
  bool enabled() const override;
  MediaStreamTrackInterface::TrackState state() const override;
  std::string kind() const override;

 protected:
  VideoTrack(const std::string& id,
             VideoTrackSourceInterface* video_source,
             rtc::Thread* worker_thread);
  ~VideoTrack();

 private:
  // Handles |video_source_| state changes on the worker thread.
  void OnVideoSourceStateChanged(MediaSourceInterface::SourceState state);

  // Separate implementation for receiving notifications from the video source.
  // This object is associated with the signaling thread whereas the state of
  // the video track is largely managed on the worker thread. The observer
  // is managed as a separate memory allocation since it will outlive the
  // video track object.
  class SourceObserver : public ObserverInterface {
   public:
    explicit SourceObserver(VideoTrack* track);
    ~SourceObserver() override;

   private:
    // Implements ObserverInterface. Observes |video_source_| state.
    void OnChanged() override;

    RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker signaling_thread_;
    VideoTrack* const track_;
    rtc::Thread* const worker_thread_;
    const rtc::scoped_refptr<PendingTaskSafetyFlag> worker_safety_;
  };

  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  const rtc::scoped_refptr<PendingTaskSafetyFlag> worker_safety_;
  const rtc::scoped_refptr<VideoTrackSourceInterface> video_source_;
  ContentHint content_hint_ RTC_GUARDED_BY(worker_thread_);
  std::unique_ptr<SourceObserver> observer_;
};

}  // namespace webrtc

#endif  // PC_VIDEO_TRACK_H_
