/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/audio_device_impl.h"

#include "modules/audio_device/audio_device_config.h"
#include "modules/audio_device/audio_device_generic.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "system_wrappers/include/metrics.h"

#if defined(_WIN32)
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
#include "audio_device_core_win.h"
#endif
#elif defined(WEBRTC_ANDROID)
#include <stdlib.h>
#include "modules/audio_device/android/audio_device_template.h"
#include "modules/audio_device/android/audio_manager.h"
#include "modules/audio_device/android/audio_record_jni.h"
#include "modules/audio_device/android/audio_track_jni.h"
#include "modules/audio_device/android/opensles_player.h"
#include "modules/audio_device/android/opensles_recorder.h"
#elif defined(WEBRTC_LINUX)
#if defined(LINUX_ALSA)
#include "audio_device_alsa_linux.h"
#endif
#if defined(LINUX_PULSE)
#include "audio_device_pulse_linux.h"
#endif
#elif defined(WEBRTC_IOS)
#include "audio_device_ios.h"
#elif defined(WEBRTC_MAC)
#include "audio_device_mac.h"
#endif
#if defined(WEBRTC_DUMMY_FILE_DEVICES)
#include "modules/audio_device/dummy/file_audio_device_factory.h"
#endif
#include "modules/audio_device/dummy/audio_device_dummy.h"
#include "modules/audio_device/dummy/file_audio_device.h"

#define CHECKinitialized_() \
  {                         \
    if (!initialized_) {    \
      return -1;            \
    };                      \
  }

#define CHECKinitialized__BOOL() \
  {                              \
    if (!initialized_) {         \
      return false;              \
    };                           \
  }

namespace webrtc {

// static
rtc::scoped_refptr<AudioDeviceModule> AudioDeviceModule::Create(
    const AudioLayer audio_layer) {
  NLOG(INFO, __FUNCTION__);
  // Create the generic reference counted (platform independent) implementation.
  rtc::scoped_refptr<AudioDeviceModuleImpl> audioDevice(
      new rtc::RefCountedObject<AudioDeviceModuleImpl>(audio_layer));

  // Ensure that the current platform is supported.
  if (audioDevice->CheckPlatform() == -1) {
    return nullptr;
  }

  // Create the platform-dependent implementation.
  if (audioDevice->CreatePlatformSpecificObjects() == -1) {
    return nullptr;
  }

  // Ensure that the generic audio buffer can communicate with the platform
  // specific parts.
  if (audioDevice->AttachAudioBuffer() == -1) {
    return nullptr;
  }

  return audioDevice;
}

// TODO(bugs.webrtc.org/7306): deprecated.
rtc::scoped_refptr<AudioDeviceModule> AudioDeviceModule::Create(
    const int32_t id,
    const AudioLayer audio_layer) {
  NLOG(INFO, __FUNCTION__);
  return AudioDeviceModule::Create(audio_layer);
}

AudioDeviceModuleImpl::AudioDeviceModuleImpl(const AudioLayer audioLayer)
    : audio_layer_(audioLayer) {
  NLOG(INFO, __FUNCTION__);
}

int32_t AudioDeviceModuleImpl::CheckPlatform() {
  NLOG(INFO, __FUNCTION__);
  // Ensure that the current platform is supported
  PlatformType platform(kPlatformNotSupported);
#if defined(_WIN32)
  platform = kPlatformWin32;
  NLOG(INFO, "current platform is Win32");
#elif defined(WEBRTC_ANDROID)
  platform = kPlatformAndroid;
  NLOG(INFO, "current platform is Android");
#elif defined(WEBRTC_LINUX)
  platform = kPlatformLinux;
  NLOG(INFO, "current platform is Linux");
#elif defined(WEBRTC_IOS)
  platform = kPlatformIOS;
  NLOG(INFO, "current platform is IOS");
#elif defined(WEBRTC_MAC)
  platform = kPlatformMac;
  NLOG(INFO, "current platform is Mac");
#endif
  if (platform == kPlatformNotSupported) {
    NLOG(LERROR,
         "current platform is not supported => this module will self "
         "destruct!");
    return -1;
  }
  platform_type_ = platform;
  return 0;
}

int32_t AudioDeviceModuleImpl::CreatePlatformSpecificObjects() {
  NLOG(INFO, __FUNCTION__);
// Dummy ADM implementations if build flags are set.
#if defined(WEBRTC_DUMMY_AUDIO_BUILD)
  audio_device_.reset(new AudioDeviceDummy());
  NLOG(INFO, "Dummy Audio APIs will be utilized");
#elif defined(WEBRTC_DUMMY_FILE_DEVICES)
  audio_device_.reset(FileAudioDeviceFactory::CreateFileAudioDevice());
  if (audio_device_) {
    NLOG(INFO, "Will use file-playing dummy device.");
  } else {
    // Create a dummy device instead.
    audio_device_.reset(new AudioDeviceDummy());
    NLOG(INFO, "Dummy Audio APIs will be utilized");
  }

// Real (non-dummy) ADM implementations.
#else
  AudioLayer audio_layer(PlatformAudioLayer());
// Windows ADM implementation.
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  if ((audio_layer == kWindowsCoreAudio) ||
      (audio_layer == kPlatformDefaultAudio)) {
    NLOG(INFO, "Attempting to use the Windows Core Audio APIs...");
    if (AudioDeviceWindowsCore::CoreAudioIsSupported()) {
      audio_device_.reset(new AudioDeviceWindowsCore());
      NLOG(INFO, "Windows Core Audio APIs will be utilized");
    }
  }
#endif  // defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)

#if defined(WEBRTC_ANDROID)
  // Create an Android audio manager.
  audio_manager_android_.reset(new AudioManager());
  // Select best possible combination of audio layers.
  if (audio_layer == kPlatformDefaultAudio) {
    if (audio_manager_android_->IsLowLatencyPlayoutSupported() &&
        audio_manager_android_->IsLowLatencyRecordSupported()) {
      // Use OpenSL ES for both playout and recording.
      audio_layer = kAndroidOpenSLESAudio;
    } else if (audio_manager_android_->IsLowLatencyPlayoutSupported() &&
               !audio_manager_android_->IsLowLatencyRecordSupported()) {
      // Use OpenSL ES for output on devices that only supports the
      // low-latency output audio path.
      audio_layer = kAndroidJavaInputAndOpenSLESOutputAudio;
    } else {
      // Use Java-based audio in both directions when low-latency output is
      // not supported.
      audio_layer = kAndroidJavaAudio;
    }
  }
  AudioManager* audio_manager = audio_manager_android_.get();
  if (audio_layer == kAndroidJavaAudio) {
    // Java audio for both input and output audio.
    audio_device_.reset(new AudioDeviceTemplate<AudioRecordJni, AudioTrackJni>(
        audio_layer, audio_manager));
  } else if (audio_layer == kAndroidOpenSLESAudio) {
    // OpenSL ES based audio for both input and output audio.
    audio_device_.reset(
        new AudioDeviceTemplate<OpenSLESRecorder, OpenSLESPlayer>(
            audio_layer, audio_manager));
  } else if (audio_layer == kAndroidJavaInputAndOpenSLESOutputAudio) {
    // Java audio for input and OpenSL ES for output audio (i.e. mixed APIs).
    // This combination provides low-latency output audio and at the same
    // time support for HW AEC using the AudioRecord Java API.
    audio_device_.reset(new AudioDeviceTemplate<AudioRecordJni, OpenSLESPlayer>(
        audio_layer, audio_manager));
  } else {
    // Invalid audio layer.
    audio_device_.reset(nullptr);
  }
// END #if defined(WEBRTC_ANDROID)

// Linux ADM implementation.
#elif defined(WEBRTC_LINUX)
  if ((audio_layer == kLinuxPulseAudio) ||
      (audio_layer == kPlatformDefaultAudio)) {
#if defined(LINUX_PULSE)
    NLOG(INFO, "Attempting to use Linux PulseAudio APIs...");
    // Linux PulseAudio implementation.
    audio_device_.reset(new AudioDeviceLinuxPulse());
    NLOG(INFO, "Linux PulseAudio APIs will be utilized");
#endif
#if defined(LINUX_PULSE)
#endif
  } else if (audio_layer == kLinuxAlsaAudio) {
#if defined(LINUX_ALSA)
    // Linux ALSA implementation.
    audio_device_.reset(new AudioDeviceLinuxALSA());
    NLOG(INFO, "Linux ALSA APIs will be utilized.");
#endif
  }
#endif  // #if defined(WEBRTC_LINUX)

// iOS ADM implementation.
#if defined(WEBRTC_IOS)
  if (audio_layer == kPlatformDefaultAudio) {
    audio_device_.reset(new AudioDeviceIOS());
    NLOG(INFO, "iPhone Audio APIs will be utilized.");
  }
// END #if defined(WEBRTC_IOS)

// Mac OS X ADM implementation.
#elif defined(WEBRTC_MAC)
  if (audio_layer == kPlatformDefaultAudio) {
    audio_device_.reset(new AudioDeviceMac());
    NLOG(INFO, "Mac OS X Audio APIs will be utilized.");
  }
#endif  // WEBRTC_MAC

  // Dummy ADM implementation.
  if (audio_layer == kDummyAudio) {
    audio_device_.reset(new AudioDeviceDummy());
    NLOG(INFO, "Dummy Audio APIs will be utilized.");
  }
#endif  // if defined(WEBRTC_DUMMY_AUDIO_BUILD)

  if (!audio_device_) {
    NLOG(LS_ERROR,
         "Failed to create the platform specific ADM implementation.");
    return -1;
  }
  return 0;
}

int32_t AudioDeviceModuleImpl::AttachAudioBuffer() {
  NLOG(INFO, __FUNCTION__);
  audio_device_->AttachAudioBuffer(&audio_device_buffer_);
  return 0;
}

AudioDeviceModuleImpl::~AudioDeviceModuleImpl() {
  NLOG(INFO, __FUNCTION__);
}

int32_t AudioDeviceModuleImpl::ActiveAudioLayer(AudioLayer* audioLayer) const {
  NLOG(INFO, __FUNCTION__);
  AudioLayer activeAudio;
  if (audio_device_->ActiveAudioLayer(activeAudio) == -1) {
    return -1;
  }
  *audioLayer = activeAudio;
  return 0;
}

int32_t AudioDeviceModuleImpl::Init() {
  NLOG(INFO, __FUNCTION__);
  if (initialized_)
    return 0;
  RTC_CHECK(audio_device_);
  AudioDeviceGeneric::InitStatus status = audio_device_->Init();
  RTC_HISTOGRAM_ENUMERATION(
      "WebRTC.Audio.InitializationResult", static_cast<int>(status),
      static_cast<int>(AudioDeviceGeneric::InitStatus::NUM_STATUSES));
  if (status != AudioDeviceGeneric::InitStatus::OK) {
    NLOG(LS_ERROR, "Audio device initialization failed.");
    return -1;
  }
  initialized_ = true;
  return 0;
}

int32_t AudioDeviceModuleImpl::Terminate() {
  NLOG(INFO, __FUNCTION__);
  if (!initialized_)
    return 0;
  if (audio_device_->Terminate() == -1) {
    return -1;
  }
  initialized_ = false;
  return 0;
}

bool AudioDeviceModuleImpl::Initialized() const {
  NLOG(INFO, __FUNCTION__, ": ", initialized_);
  return initialized_;
}

int32_t AudioDeviceModuleImpl::InitSpeaker() {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  return audio_device_->InitSpeaker();
}

int32_t AudioDeviceModuleImpl::InitMicrophone() {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  return audio_device_->InitMicrophone();
}

int32_t AudioDeviceModuleImpl::SpeakerVolumeIsAvailable(bool* available) {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->SpeakerVolumeIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  NLOG(INFO, "output: ", isAvailable);
  return 0;
}

int32_t AudioDeviceModuleImpl::SetSpeakerVolume(uint32_t volume) {
  NLOG(INFO, __FUNCTION__, "(", volume, ")");
  CHECKinitialized_();
  return audio_device_->SetSpeakerVolume(volume);
}

int32_t AudioDeviceModuleImpl::SpeakerVolume(uint32_t* volume) const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  uint32_t level = 0;
  if (audio_device_->SpeakerVolume(level) == -1) {
    return -1;
  }
  *volume = level;
  NLOG(INFO, "output: ", *volume);
  return 0;
}

bool AudioDeviceModuleImpl::SpeakerIsInitialized() const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized__BOOL();
  bool isInitialized = audio_device_->SpeakerIsInitialized();
  NLOG(INFO, "output: ", isInitialized);
  return isInitialized;
}

bool AudioDeviceModuleImpl::MicrophoneIsInitialized() const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized__BOOL();
  bool isInitialized = audio_device_->MicrophoneIsInitialized();
  NLOG(INFO, "output: ", isInitialized);
  return isInitialized;
}

int32_t AudioDeviceModuleImpl::MaxSpeakerVolume(uint32_t* maxVolume) const {
  CHECKinitialized_();
  uint32_t maxVol = 0;
  if (audio_device_->MaxSpeakerVolume(maxVol) == -1) {
    return -1;
  }
  *maxVolume = maxVol;
  return 0;
}

int32_t AudioDeviceModuleImpl::MinSpeakerVolume(uint32_t* minVolume) const {
  CHECKinitialized_();
  uint32_t minVol = 0;
  if (audio_device_->MinSpeakerVolume(minVol) == -1) {
    return -1;
  }
  *minVolume = minVol;
  return 0;
}

int32_t AudioDeviceModuleImpl::SpeakerMuteIsAvailable(bool* available) {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->SpeakerMuteIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  NLOG(INFO, "output: ", isAvailable);
  return 0;
}

int32_t AudioDeviceModuleImpl::SetSpeakerMute(bool enable) {
  NLOG(INFO, __FUNCTION__, "(", enable, ")");
  CHECKinitialized_();
  return audio_device_->SetSpeakerMute(enable);
}

int32_t AudioDeviceModuleImpl::SpeakerMute(bool* enabled) const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool muted = false;
  if (audio_device_->SpeakerMute(muted) == -1) {
    return -1;
  }
  *enabled = muted;
  NLOG(INFO, "output: ", muted);
  return 0;
}

int32_t AudioDeviceModuleImpl::MicrophoneMuteIsAvailable(bool* available) {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->MicrophoneMuteIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  NLOG(INFO, "output: ", isAvailable);
  return 0;
}

int32_t AudioDeviceModuleImpl::SetMicrophoneMute(bool enable) {
  NLOG(INFO, __FUNCTION__, "(", enable, ")");
  CHECKinitialized_();
  return (audio_device_->SetMicrophoneMute(enable));
}

int32_t AudioDeviceModuleImpl::MicrophoneMute(bool* enabled) const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool muted = false;
  if (audio_device_->MicrophoneMute(muted) == -1) {
    return -1;
  }
  *enabled = muted;
  NLOG(INFO, "output: ", muted);
  return 0;
}

int32_t AudioDeviceModuleImpl::MicrophoneVolumeIsAvailable(bool* available) {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->MicrophoneVolumeIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  NLOG(INFO, "output: ", isAvailable);
  return 0;
}

int32_t AudioDeviceModuleImpl::SetMicrophoneVolume(uint32_t volume) {
  NLOG(INFO, __FUNCTION__, "(", volume, ")");
  CHECKinitialized_();
  return (audio_device_->SetMicrophoneVolume(volume));
}

int32_t AudioDeviceModuleImpl::MicrophoneVolume(uint32_t* volume) const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  uint32_t level = 0;
  if (audio_device_->MicrophoneVolume(level) == -1) {
    return -1;
  }
  *volume = level;
  NLOG(INFO, "output: ", *volume);
  return 0;
}

int32_t AudioDeviceModuleImpl::StereoRecordingIsAvailable(
    bool* available) const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->StereoRecordingIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  NLOG(INFO, "output: ", isAvailable);
  return 0;
}

int32_t AudioDeviceModuleImpl::SetStereoRecording(bool enable) {
  NLOG(INFO, __FUNCTION__, "(", enable, ")");
  CHECKinitialized_();
  if (audio_device_->RecordingIsInitialized()) {
    NLOG(WARNING, "recording in stereo is not supported");
    return -1;
  }
  if (audio_device_->SetStereoRecording(enable) == -1) {
    NLOG(WARNING, "failed to change stereo recording");
    return -1;
  }
  int8_t nChannels(1);
  if (enable) {
    nChannels = 2;
  }
  audio_device_buffer_.SetRecordingChannels(nChannels);
  return 0;
}

int32_t AudioDeviceModuleImpl::StereoRecording(bool* enabled) const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool stereo = false;
  if (audio_device_->StereoRecording(stereo) == -1) {
    return -1;
  }
  *enabled = stereo;
  NLOG(INFO, "output: ", stereo);
  return 0;
}

int32_t AudioDeviceModuleImpl::StereoPlayoutIsAvailable(bool* available) const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->StereoPlayoutIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  NLOG(INFO, "output: ", isAvailable);
  return 0;
}

int32_t AudioDeviceModuleImpl::SetStereoPlayout(bool enable) {
  NLOG(INFO, __FUNCTION__, "(", enable, ")");
  CHECKinitialized_();
  if (audio_device_->PlayoutIsInitialized()) {
    NLOG(LERROR, "unable to set stereo mode while playing side is initialized");
    return -1;
  }
  if (audio_device_->SetStereoPlayout(enable)) {
    NLOG(WARNING, "stereo playout is not supported");
    return -1;
  }
  int8_t nChannels(1);
  if (enable) {
    nChannels = 2;
  }
  audio_device_buffer_.SetPlayoutChannels(nChannels);
  return 0;
}

int32_t AudioDeviceModuleImpl::StereoPlayout(bool* enabled) const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool stereo = false;
  if (audio_device_->StereoPlayout(stereo) == -1) {
    return -1;
  }
  *enabled = stereo;
  NLOG(INFO, "output: ", stereo);
  return 0;
}

int32_t AudioDeviceModuleImpl::PlayoutIsAvailable(bool* available) {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->PlayoutIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  NLOG(INFO, "output: ", isAvailable);
  return 0;
}

int32_t AudioDeviceModuleImpl::RecordingIsAvailable(bool* available) {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  bool isAvailable = false;
  if (audio_device_->RecordingIsAvailable(isAvailable) == -1) {
    return -1;
  }
  *available = isAvailable;
  NLOG(INFO, "output: ", isAvailable);
  return 0;
}

int32_t AudioDeviceModuleImpl::MaxMicrophoneVolume(uint32_t* maxVolume) const {
  CHECKinitialized_();
  uint32_t maxVol(0);
  if (audio_device_->MaxMicrophoneVolume(maxVol) == -1) {
    return -1;
  }
  *maxVolume = maxVol;
  return 0;
}

int32_t AudioDeviceModuleImpl::MinMicrophoneVolume(uint32_t* minVolume) const {
  CHECKinitialized_();
  uint32_t minVol(0);
  if (audio_device_->MinMicrophoneVolume(minVol) == -1) {
    return -1;
  }
  *minVolume = minVol;
  return 0;
}

int16_t AudioDeviceModuleImpl::PlayoutDevices() {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  uint16_t nPlayoutDevices = audio_device_->PlayoutDevices();
  NLOG(INFO, "output: ", nPlayoutDevices);
  return (int16_t)(nPlayoutDevices);
}

int32_t AudioDeviceModuleImpl::SetPlayoutDevice(uint16_t index) {
  NLOG(INFO, __FUNCTION__, "(", index, ")");
  CHECKinitialized_();
  return audio_device_->SetPlayoutDevice(index);
}

int32_t AudioDeviceModuleImpl::SetPlayoutDevice(WindowsDeviceType device) {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  return audio_device_->SetPlayoutDevice(device);
}

int32_t AudioDeviceModuleImpl::PlayoutDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize]) {
  NLOG(INFO, __FUNCTION__, "(", index, ", ...)");
  CHECKinitialized_();
  if (name == NULL) {
    return -1;
  }
  if (audio_device_->PlayoutDeviceName(index, name, guid) == -1) {
    return -1;
  }
  if (name != NULL) {
    NLOG(INFO, "output: name = ", name);
  }
  if (guid != NULL) {
    NLOG(INFO, "output: guid = ", guid);
  }
  return 0;
}

int32_t AudioDeviceModuleImpl::RecordingDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize]) {
  NLOG(INFO, __FUNCTION__, "(", index, ", ...)");
  CHECKinitialized_();
  if (name == NULL) {
    return -1;
  }
  if (audio_device_->RecordingDeviceName(index, name, guid) == -1) {
    return -1;
  }
  if (name != NULL) {
    NLOG(INFO, "output: name = ", name);
  }
  if (guid != NULL) {
    NLOG(INFO, "output: guid = ", guid);
  }
  return 0;
}

int16_t AudioDeviceModuleImpl::RecordingDevices() {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  uint16_t nRecordingDevices = audio_device_->RecordingDevices();
  NLOG(INFO, "output: ", nRecordingDevices);
  return (int16_t)nRecordingDevices;
}

int32_t AudioDeviceModuleImpl::SetRecordingDevice(uint16_t index) {
  NLOG(INFO, __FUNCTION__, "(", index, ")");
  CHECKinitialized_();
  return audio_device_->SetRecordingDevice(index);
}

int32_t AudioDeviceModuleImpl::SetRecordingDevice(WindowsDeviceType device) {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  return audio_device_->SetRecordingDevice(device);
}

int32_t AudioDeviceModuleImpl::InitPlayout() {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  if (PlayoutIsInitialized()) {
    return 0;
  }
  int32_t result = audio_device_->InitPlayout();
  NLOG(INFO, "output: ", result);
  RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.InitPlayoutSuccess",
                        static_cast<int>(result == 0));
  return result;
}

int32_t AudioDeviceModuleImpl::InitRecording() {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  if (RecordingIsInitialized()) {
    return 0;
  }
  int32_t result = audio_device_->InitRecording();
  NLOG(INFO, "output: ", result);
  RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.InitRecordingSuccess",
                        static_cast<int>(result == 0));
  return result;
}

bool AudioDeviceModuleImpl::PlayoutIsInitialized() const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized__BOOL();
  return audio_device_->PlayoutIsInitialized();
}

bool AudioDeviceModuleImpl::RecordingIsInitialized() const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized__BOOL();
  return audio_device_->RecordingIsInitialized();
}

int32_t AudioDeviceModuleImpl::StartPlayout() {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  if (Playing()) {
    return 0;
  }
  audio_device_buffer_.StartPlayout();
  int32_t result = audio_device_->StartPlayout();
  NLOG(INFO, "output: ", result);
  RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StartPlayoutSuccess",
                        static_cast<int>(result == 0));
  return result;
}

int32_t AudioDeviceModuleImpl::StopPlayout() {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  int32_t result = audio_device_->StopPlayout();
  audio_device_buffer_.StopPlayout();
  NLOG(INFO, "output: ", result);
  RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StopPlayoutSuccess",
                        static_cast<int>(result == 0));
  return result;
}

bool AudioDeviceModuleImpl::Playing() const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized__BOOL();
  return audio_device_->Playing();
}

int32_t AudioDeviceModuleImpl::StartRecording() {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  if (Recording()) {
    return 0;
  }
  audio_device_buffer_.StartRecording();
  int32_t result = audio_device_->StartRecording();
  NLOG(INFO, "output: ", result);
  RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StartRecordingSuccess",
                        static_cast<int>(result == 0));
  return result;
}

int32_t AudioDeviceModuleImpl::StopRecording() {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized_();
  int32_t result = audio_device_->StopRecording();
  audio_device_buffer_.StopRecording();
  NLOG(INFO, "output: ", result);
  RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StopRecordingSuccess",
                        static_cast<int>(result == 0));
  return result;
}

bool AudioDeviceModuleImpl::Recording() const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized__BOOL();
  return audio_device_->Recording();
}

int32_t AudioDeviceModuleImpl::RegisterAudioCallback(
    AudioTransport* audioCallback) {
  NLOG(INFO, __FUNCTION__);
  return audio_device_buffer_.RegisterAudioCallback(audioCallback);
}

int32_t AudioDeviceModuleImpl::PlayoutDelay(uint16_t* delayMS) const {
  CHECKinitialized_();
  uint16_t delay = 0;
  if (audio_device_->PlayoutDelay(delay) == -1) {
    NLOG(LERROR, "failed to retrieve the playout delay");
    return -1;
  }
  *delayMS = delay;
  return 0;
}

bool AudioDeviceModuleImpl::BuiltInAECIsAvailable() const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized__BOOL();
  bool isAvailable = audio_device_->BuiltInAECIsAvailable();
  NLOG(INFO, "output: ", isAvailable);
  return isAvailable;
}

int32_t AudioDeviceModuleImpl::EnableBuiltInAEC(bool enable) {
  NLOG(INFO, __FUNCTION__, "(", enable, ")");
  CHECKinitialized_();
  int32_t ok = audio_device_->EnableBuiltInAEC(enable);
  NLOG(INFO, "output: ", ok);
  return ok;
}

bool AudioDeviceModuleImpl::BuiltInAGCIsAvailable() const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized__BOOL();
  bool isAvailable = audio_device_->BuiltInAGCIsAvailable();
  NLOG(INFO, "output: ", isAvailable);
  return isAvailable;
}

int32_t AudioDeviceModuleImpl::EnableBuiltInAGC(bool enable) {
  NLOG(INFO, __FUNCTION__, "(", enable, ")");
  CHECKinitialized_();
  int32_t ok = audio_device_->EnableBuiltInAGC(enable);
  NLOG(INFO, "output: ", ok);
  return ok;
}

bool AudioDeviceModuleImpl::BuiltInNSIsAvailable() const {
  NLOG(INFO, __FUNCTION__);
  CHECKinitialized__BOOL();
  bool isAvailable = audio_device_->BuiltInNSIsAvailable();
  NLOG(INFO, "output: ", isAvailable);
  return isAvailable;
}

int32_t AudioDeviceModuleImpl::EnableBuiltInNS(bool enable) {
  NLOG(INFO, __FUNCTION__, "(", enable, ")");
  CHECKinitialized_();
  int32_t ok = audio_device_->EnableBuiltInNS(enable);
  NLOG(INFO, "output: ", ok);
  return ok;
}

#if defined(WEBRTC_IOS)
int AudioDeviceModuleImpl::GetPlayoutAudioParameters(
    AudioParameters* params) const {
  NLOG(INFO, __FUNCTION__);
  int r = audio_device_->GetPlayoutAudioParameters(params);
  NLOG(INFO, "output: ", r);
  return r;
}

int AudioDeviceModuleImpl::GetRecordAudioParameters(
    AudioParameters* params) const {
  NLOG(INFO, __FUNCTION__);
  int r = audio_device_->GetRecordAudioParameters(params);
  NLOG(INFO, "output: ", r);
  return r;
}
#endif  // WEBRTC_IOS

AudioDeviceModuleImpl::PlatformType AudioDeviceModuleImpl::Platform() const {
  NLOG(INFO, __FUNCTION__);
  return platform_type_;
}

AudioDeviceModule::AudioLayer AudioDeviceModuleImpl::PlatformAudioLayer()
    const {
  NLOG(INFO, __FUNCTION__);
  return audio_layer_;
}

}  // namespace webrtc
