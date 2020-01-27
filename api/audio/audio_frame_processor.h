/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_AUDIO_FRAME_PROCESSOR_H_
#define API_AUDIO_AUDIO_FRAME_PROCESSOR_H_

#include <memory>

namespace webrtc {

class AudioFrame;

// If passed into PeerConnectionFactory, will be used for additional
// asynchronous processing of captured audio frames, performed before encoding.
class AudioFrameProcessor {
 public:
  // Receives processed audio frames and hands them over to WebRTC for encoding.
  // Implemented by WebRTC.
  class Sink {
   public:
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;

    Sink() = default;
    virtual ~Sink() = default;
    virtual void OnFrameProcessed(std::unique_ptr<AudioFrame> frame) = 0;
  };

  AudioFrameProcessor(const AudioFrameProcessor&) = delete;
  AudioFrameProcessor& operator=(const AudioFrameProcessor&) = delete;

  AudioFrameProcessor() = default;
  virtual ~AudioFrameProcessor() = default;

  // Is called by WebRTC to pass |frame| for processing.
  virtual void Process(std::unique_ptr<AudioFrame> frame) = 0;
  // Is called by WebRTC to specify a sink which will receive processed audio
  // frames.
  virtual void SetSink(Sink* sink) = 0;
};

}  // namespace webrtc

#endif  // API_AUDIO_AUDIO_FRAME_PROCESSOR_H_
