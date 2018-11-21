/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_TEST_UTILITY_H_
#define MODULES_AUDIO_CODING_TEST_UTILITY_H_

#include "modules/audio_coding/include/audio_coding_module.h"
#include "test/gtest.h"

namespace webrtc {

//-----------------------------
#define CHECK_ERROR(f)                      \
  do {                                      \
    EXPECT_GE(f, 0) << "Error Calling API"; \
  } while (0)

class ACMTestTimer {
 public:
  ACMTestTimer();
  ~ACMTestTimer();

  void Reset();
  void Tick10ms();
  void Tick1ms();
  void Tick100ms();
  void Tick1sec();
  void CurrentTimeHMS(char* currTime);
  void CurrentTime(unsigned long& h,
                   unsigned char& m,
                   unsigned char& s,
                   unsigned short& ms);

 private:
  void Adjust();

  unsigned short _msec;
  unsigned char _sec;
  unsigned char _min;
  unsigned long _hour;
};

class VADCallback : public ACMVADCallback {
 public:
  VADCallback();

  int32_t InFrameType(FrameType frame_type) override;

  void PrintFrameTypes();
  void Reset();

 private:
  uint32_t _numFrameTypes[5];
};

namespace test {

int CodecIdForTest(std::string name, int clockrate_hz, int num_channels);

}  // namespace test

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_TEST_UTILITY_H_
