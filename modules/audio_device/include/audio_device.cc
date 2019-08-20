/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/string_utils.h"

namespace webrtc {

// TODO(nisse): Over the transition, let overloads call each other. Subclasses
// must override one or the other.
int32_t AudioDeviceModule::PlayoutDeviceName(uint16_t index,
                                             std::string* name,
                                             std::string* guid) {
  char name_cstr[kAdmMaxDeviceNameSize];
  char guid_cstr[kAdmMaxGuidSize];
  int32_t err = PlayoutDeviceName(index, name_cstr, guid_cstr);
  if (err == 0) {
    if (name) {
      *name = name_cstr;
    }
    if (guid) {
      *guid = guid_cstr;
    }
  }
  return err;
}

int32_t AudioDeviceModule::RecordingDeviceName(uint16_t index,
                                               std::string* name,
                                               std::string* guid) {
  char name_cstr[kAdmMaxDeviceNameSize];
  char guid_cstr[kAdmMaxGuidSize];
  int32_t err = RecordingDeviceName(index, name_cstr, guid_cstr);
  if (err == 0) {
    if (name) {
      *name = name_cstr;
    }
    if (guid) {
      *guid = guid_cstr;
    }
  }
  return err;
}

int32_t AudioDeviceModule::PlayoutDeviceName(
    uint16_t index,
    char name_cstr[kAdmMaxDeviceNameSize],
    char guid_cstr[kAdmMaxGuidSize]) {
  std::string name;
  std::string guid;
  int32_t err = PlayoutDeviceName(index, &name, &guid);
  if (err == 0) {
    rtc::strcpyn(name_cstr, kAdmMaxDeviceNameSize, name.c_str());
    rtc::strcpyn(guid_cstr, kAdmMaxGuidSize, guid.c_str());
  }
  return err;
}

int32_t AudioDeviceModule::RecordingDeviceName(
    uint16_t index,
    char name_cstr[kAdmMaxDeviceNameSize],
    char guid_cstr[kAdmMaxGuidSize]) {
  std::string name;
  std::string guid;
  int32_t err = RecordingDeviceName(index, &name, &guid);
  if (err == 0) {
    rtc::strcpyn(name_cstr, kAdmMaxDeviceNameSize, name.c_str());
    rtc::strcpyn(guid_cstr, kAdmMaxGuidSize, guid.c_str());
  }
  return err;
}

}  // namespace webrtc
