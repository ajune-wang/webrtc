/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_DEVICE_IOS_FACTORY_H_
#define SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_DEVICE_IOS_FACTORY_H_

#include "audio_device_ios.h"
#include "api/scoped_refptr.h"

namespace webrtc {
namespace ios_adm {

class AudioDeviceIOSFactory {
 public:
  virtual AudioDeviceIOS* CreateAudioDeviceModule() = 0;
  virtual ~AudioDeviceIOSFactory(){};
};

}  // namespace ios_adm
}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_DEVICE_IOS_FACTORY_H_
