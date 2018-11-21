/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test/gtest.h"

#define NUM_CODECS_WITH_FIXED_PAYLOAD_TYPE 13

namespace webrtc {

ACMTestTimer::ACMTestTimer() : _msec(0), _sec(0), _min(0), _hour(0) {
  return;
}

ACMTestTimer::~ACMTestTimer() {
  return;
}

void ACMTestTimer::Reset() {
  _msec = 0;
  _sec = 0;
  _min = 0;
  _hour = 0;
  return;
}
void ACMTestTimer::Tick10ms() {
  _msec += 10;
  Adjust();
  return;
}

void ACMTestTimer::Tick1ms() {
  _msec++;
  Adjust();
  return;
}

void ACMTestTimer::Tick100ms() {
  _msec += 100;
  Adjust();
  return;
}

void ACMTestTimer::Tick1sec() {
  _sec++;
  Adjust();
  return;
}

void ACMTestTimer::CurrentTimeHMS(char* currTime) {
  sprintf(currTime, "%4lu:%02u:%06.3f", _hour, _min,
          (double)_sec + (double)_msec / 1000.);
  return;
}

void ACMTestTimer::CurrentTime(unsigned long& h,
                               unsigned char& m,
                               unsigned char& s,
                               unsigned short& ms) {
  h = _hour;
  m = _min;
  s = _sec;
  ms = _msec;
  return;
}

void ACMTestTimer::Adjust() {
  unsigned int n;
  if (_msec >= 1000) {
    n = _msec / 1000;
    _msec -= (1000 * n);
    _sec += n;
  }
  if (_sec >= 60) {
    n = _sec / 60;
    _sec -= (n * 60);
    _min += n;
  }
  if (_min >= 60) {
    n = _min / 60;
    _min -= (n * 60);
    _hour += n;
  }
}

void VADCallback::Reset() {
  memset(_numFrameTypes, 0, sizeof(_numFrameTypes));
}

VADCallback::VADCallback() {
  memset(_numFrameTypes, 0, sizeof(_numFrameTypes));
}

void VADCallback::PrintFrameTypes() {
  printf("kEmptyFrame......... %d\n", _numFrameTypes[kEmptyFrame]);
  printf("kAudioFrameSpeech... %d\n", _numFrameTypes[kAudioFrameSpeech]);
  printf("kAudioFrameCN....... %d\n", _numFrameTypes[kAudioFrameCN]);
  printf("kVideoFrameKey...... %d\n", _numFrameTypes[kVideoFrameKey]);
  printf("kVideoFrameDelta.... %d\n", _numFrameTypes[kVideoFrameDelta]);
}

int32_t VADCallback::InFrameType(FrameType frame_type) {
  _numFrameTypes[frame_type]++;
  return 0;
}

int CodecIdForTest(std::string name, int clockrate_hz, int num_channels) {
  // TODO(solenberg): Implement ACM codec database, sort of.
  return 1;
}

}  // namespace webrtc
