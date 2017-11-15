/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  This file contains common constants for VoiceEngine, as well as
 *  platform specific settings.
 */

#ifndef VOICE_ENGINE_VOICE_ENGINE_DEFINES_H_
#define VOICE_ENGINE_VOICE_ENGINE_DEFINES_H_

namespace webrtc {

// VolumeControl
enum { kMinVolumeLevel = 0 };
enum { kMaxVolumeLevel = 255 };

// VideoSync
// Lowest minimum playout delay
enum { kVoiceEngineMinMinPlayoutDelayMs = 0 };
// Highest minimum playout delay
enum { kVoiceEngineMaxMinPlayoutDelayMs = 10000 };

}  // namespace webrtc

#if defined(_WIN32)
#define WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE \
  AudioDeviceModule::kDefaultCommunicationDevice
#else
#define WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE 0
#endif  // #if (defined(_WIN32)

#endif  // VOICE_ENGINE_VOICE_ENGINE_DEFINES_H_
