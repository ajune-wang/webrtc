/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/audio_device_factory_impl.h"

#include <memory>

#include "modules/audio_device/audio_device_generic.h"
#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/logging.h"

#if defined(_WIN32)
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
#include "modules/audio_device/win/audio_device_core_win.h"
#endif
#elif defined(WEBRTC_ANDROID)
#include <stdlib.h>
#if defined(WEBRTC_AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
#include "modules/audio_device/android/aaudio_player.h"
#include "modules/audio_device/android/aaudio_recorder.h"
#endif
#include "modules/audio_device/android/audio_device_template.h"
#include "modules/audio_device/android/audio_manager.h"
#include "modules/audio_device/android/audio_record_jni.h"
#include "modules/audio_device/android/audio_track_jni.h"
#include "modules/audio_device/android/opensles_player.h"
#include "modules/audio_device/android/opensles_recorder.h"
#elif defined(WEBRTC_LINUX)
#if defined(WEBRTC_ENABLE_LINUX_ALSA)
#include "modules/audio_device/linux/audio_device_alsa_linux.h"
#endif
#if defined(WEBRTC_ENABLE_LINUX_PULSE)
#include "modules/audio_device/linux/audio_device_pulse_linux.h"
#endif
#elif defined(WEBRTC_IOS)
#include "sdk/objc/native/src/audio/audio_device_ios.h"
#elif defined(WEBRTC_MAC)
#include "modules/audio_device/mac/audio_device_mac.h"
#endif
// #if defined(WEBRTC_DUMMY_FILE_DEVICES)
#include "modules/audio_device/dummy/file_audio_device.h"
#include "modules/audio_device/dummy/file_audio_device_factory.h"
// #endif
#include "modules/audio_device/dummy/audio_device_dummy.h"

namespace webrtc {

std::unique_ptr<AudioDeviceGeneric> AudioDeviceFactoryImpl::CreateAudioDevice(
    AudioDeviceModule::AudioLayer audio_layer,
    AudioManager* android_audio_manager) {
  RTC_LOG(LS_INFO) << "Creating platform specific AudioDevice. audio_layer="
                   << audio_layer;
  std::unique_ptr<AudioDeviceGeneric> audio_device;

  // Dummy ADM implementations if build flags are set.
#if defined(WEBRTC_DUMMY_AUDIO_BUILD)
  audio_device.reset(new AudioDeviceDummy());
  RTC_LOG(LS_INFO) << "Dummy Audio APIs will be utilized";
#elif defined(WEBRTC_DUMMY_FILE_DEVICES)
  audio_device.reset(FileAudioDeviceFactory::CreateFileAudioDevice());
  if (audio_device_) {
    RTC_LOG(LS_INFO) << "Will use file-playing dummy device.";
  } else {
    // Create a dummy device instead.
    audio_device.reset(new AudioDeviceDummy());
    RTC_LOG(LS_INFO) << "Dummy Audio APIs will be utilized";
  }

// Real (non-dummy) ADM implementations.
#else
// Windows ADM implementation.
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  if ((audio_layer == AudioDeviceModule::kWindowsCoreAudio) ||
      (audio_layer == AudioDeviceModule::kPlatformDefaultAudio)) {
    RTC_LOG(LS_INFO) << "Attempting to use the Windows Core Audio APIs...";
    if (AudioDeviceWindowsCore::CoreAudioIsSupported()) {
      audio_device.reset(new AudioDeviceWindowsCore());
      RTC_LOG(LS_INFO) << "Windows Core Audio APIs will be utilized";
    }
  }
#endif  // defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)

#if defined(WEBRTC_ANDROID)
  // Select best possible combination of audio layers.
  if (audio_layer == AudioDeviceModule::kPlatformDefaultAudio) {
    if (android_audio_manager->IsAAudioSupported()) {
      // Use of AAudio for both playout and recording has highest priority.
      audio_layer = AudioDeviceModule::kAndroidAAudioAudio;
    } else if (android_audio_manager->IsLowLatencyPlayoutSupported() &&
               android_audio_manager->IsLowLatencyRecordSupported()) {
      // Use OpenSL ES for both playout and recording.
      audio_layer = AudioDeviceModule::kAndroidOpenSLESAudio;
    } else if (android_audio_manager->IsLowLatencyPlayoutSupported() &&
               !android_audio_manager->IsLowLatencyRecordSupported()) {
      // Use OpenSL ES for output on devices that only supports the
      // low-latency output audio path.
      audio_layer = AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio;
    } else {
      // Use Java-based audio in both directions when low-latency output is
      // not supported.
      audio_layer = AudioDeviceModule::kAndroidJavaAudio;
    }
  }
  if (audio_layer == AudioDeviceModule::kAndroidJavaAudio) {
    // Java audio for both input and output audio.
    audio_device.reset(new AudioDeviceTemplate<AudioRecordJni, AudioTrackJni>(
        audio_layer, android_audio_manager));
  } else if (audio_layer == AudioDeviceModule::kAndroidOpenSLESAudio) {
    // OpenSL ES based audio for both input and output audio.
    audio_device.reset(
        new AudioDeviceTemplate<OpenSLESRecorder, OpenSLESPlayer>(
            audio_layer, android_audio_manager));
  } else if (audio_layer ==
             AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio) {
    // Java audio for input and OpenSL ES for output audio (i.e. mixed APIs).
    // This combination provides low-latency output audio and at the same
    // time support for HW AEC using the AudioRecord Java API.
    audio_device.reset(new AudioDeviceTemplate<AudioRecordJni, OpenSLESPlayer>(
        audio_layer, android_audio_manager));
  } else if (audio_layer == AudioDeviceModule::kAndroidAAudioAudio) {
#if defined(WEBRTC_AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
    // AAudio based audio for both input and output.
    audio_device.reset(new AudioDeviceTemplate<AAudioRecorder, AAudioPlayer>(
        audio_layer, android_audio_manager));
#endif
  } else if (audio_layer ==
             AudioDeviceModule::kAndroidJavaInputAndAAudioOutputAudio) {
#if defined(WEBRTC_AUDIO_DEVICE_INCLUDE_ANDROID_AAUDIO)
    // Java audio for input and AAudio for output audio (i.e. mixed APIs).
    audio_device.reset(new AudioDeviceTemplate<AudioRecordJni, AAudioPlayer>(
        audio_layer, android_audio_manager));
#endif
  } else {
    RTC_LOG(LS_ERROR) << "The requested audio layer is not supported";
    audio_device.reset(nullptr);
  }
// END #if defined(WEBRTC_ANDROID)

// Linux ADM implementation.
// Note that, WEBRTC_ENABLE_LINUX_ALSA is always defined by default when
// WEBRTC_LINUX is defined. WEBRTC_ENABLE_LINUX_PULSE depends on the
// 'rtc_include_pulse_audio' build flag.
// TODO(bugs.webrtc.org/9127): improve support and make it more clear that
// PulseAudio is the default selection.
#elif defined(WEBRTC_LINUX)
#if !defined(WEBRTC_ENABLE_LINUX_PULSE)
  // Build flag 'rtc_include_pulse_audio' is set to false. In this mode:
  // - kPlatformDefaultAudio => ALSA, and
  // - kLinuxAlsaAudio => ALSA, and
  // - kLinuxPulseAudio => Invalid selection.
  RTC_LOG(LS_WARNING) << "PulseAudio is disabled using build flag.";
  if ((audio_layer == AudioDeviceModule::kLinuxAlsaAudio) ||
      (audio_layer == AudioDeviceModule::kPlatformDefaultAudio)) {
    audio_device.reset(new AudioDeviceLinuxALSA());
    RTC_LOG(LS_INFO) << "Linux ALSA APIs will be utilized.";
  }
#else
  // Build flag 'rtc_include_pulse_audio' is set to true (default). In this
  // mode:
  // - kPlatformDefaultAudio => PulseAudio, and
  // - kLinuxPulseAudio => PulseAudio, and
  // - kLinuxAlsaAudio => ALSA (supported but not default).
  RTC_LOG(LS_INFO) << "PulseAudio support is enabled.";
  if ((audio_layer == AudioDeviceModule::kLinuxPulseAudio) ||
      (audio_layer == AudioDeviceModule::kPlatformDefaultAudio)) {
    // Linux PulseAudio implementation is default.
    audio_device.reset(new AudioDeviceLinuxPulse());
    RTC_LOG(LS_INFO) << "Linux PulseAudio APIs will be utilized";
  } else if (audio_layer == AudioDeviceModule::kLinuxAlsaAudio) {
    audio_device.reset(new AudioDeviceLinuxALSA());
    RTC_LOG(LS_WARNING) << "Linux ALSA APIs will be utilized.";
  }
#endif  // #if !defined(WEBRTC_ENABLE_LINUX_PULSE)
#endif  // #if defined(WEBRTC_LINUX)

// iOS ADM implementation.
#if defined(WEBRTC_IOS)
  if (audio_layer == AudioDeviceModule::kPlatformDefaultAudio) {
    audio_device.reset(
        new ios_adm::AudioDeviceIOS(/*bypass_voice_processing=*/false));
    RTC_LOG(LS_INFO) << "iPhone Audio APIs will be utilized.";
  }
// END #if defined(WEBRTC_IOS)

// Mac OS X ADM implementation.
#elif defined(WEBRTC_MAC)
  if (audio_layer == AudioDeviceModule::kPlatformDefaultAudio) {
    audio_device.reset(new AudioDeviceMac());
    RTC_LOG(LS_INFO) << "Mac OS X Audio APIs will be utilized.";
  }
#endif  // WEBRTC_MAC

  // Dummy ADM implementation.
  if (audio_layer == AudioDeviceModule::kDummyAudio) {
    audio_device.reset(new AudioDeviceDummy());
    RTC_LOG(LS_INFO) << "Dummy Audio APIs will be utilized.";
  }
#endif  // if defined(WEBRTC_DUMMY_AUDIO_BUILD)

  if (!audio_device) {
    RTC_LOG(LS_ERROR)
        << "Failed to create the platform specific AudioDevice implementation.";
  }
  return audio_device;
}

}  // namespace webrtc
