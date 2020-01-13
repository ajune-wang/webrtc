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

class AudioFrameProcessor {
 public:
  class Sink {
   public:
    virtual ~Sink() = default;
    Sink() = default;
    virtual void OnFrameProcessed(std::unique_ptr<AudioFrame> frame) = 0;
  };

  virtual ~AudioFrameProcessor() = default;
  AudioFrameProcessor() = default;

  virtual void Process(std::unique_ptr<AudioFrame> frame) = 0;
  virtual void SetSink(Sink* sink) = 0;
};

}  // namespace webrtc

#endif  // API_AUDIO_AUDIO_FRAME_PROCESSOR_H_
