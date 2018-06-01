/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/win/audio_device_module_win.h"

#include <utility>

#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {
namespace webrtc_win {
namespace {

// This class combines a generic instance of an AudioInput and a generic
// instance of an AudioOutput to create an AudioDeviceModule. This is mostly
// done by delegating to the audio input/output with some glue code. This class
// also directly implements some of the AudioDeviceModule methods with dummy
// implementations.
//
// An instance must be created, destroyed and used on one and the same thread,
// i.e., all public methods must also be called on the same thread. A thread
// checker will RTC_DCHECK if any method is called on an invalid thread.
// TODO(henrika): is thread checking needed in AudioInput and AudioOutput?
class WindowsAudioDeviceModule : public AudioDeviceModule {
 public:
  enum class InitStatus {
    OK = 0,
    PLAYOUT_ERROR = 1,
    RECORDING_ERROR = 2,
    OTHER_ERROR = 3,
    NUM_STATUSES = 4
  };

  WindowsAudioDeviceModule(AudioDeviceModule::AudioLayer audio_layer,
                           std::unique_ptr<AudioInput> audio_input,
                           std::unique_ptr<AudioOutput> audio_output)
      : audio_layer_(audio_layer),
        input_(std::move(audio_input)),
        output_(std::move(audio_output)) {
    RTC_DCHECK_EQ(audio_layer, AudioDeviceModule::kWindowsCoreAudio2);
    RTC_CHECK(input_);
    RTC_CHECK(output_);
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
  }

  ~WindowsAudioDeviceModule() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    Terminate();
  }

  WindowsAudioDeviceModule(const WindowsAudioDeviceModule&) = delete;
  WindowsAudioDeviceModule& operator=(const WindowsAudioDeviceModule&) = delete;

  int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer* audioLayer) const override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *audioLayer = audio_layer_;
    return 0;
  }

  int32_t RegisterAudioCallback(AudioTransport* audioCallback) override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK(audio_device_buffer_);
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return audio_device_buffer_->RegisterAudioCallback(audioCallback);
  }

  int32_t Init() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    if (initialized_) {
      return 0;
    }
    audio_device_buffer_ = rtc::MakeUnique<AudioDeviceBuffer>();
    AttachAudioBuffer();
    InitStatus status;
    if (output_->Init() != 0) {
      status = InitStatus::PLAYOUT_ERROR;
    } else if (input_->Init() != 0) {
      output_->Terminate();
      status = InitStatus::RECORDING_ERROR;
    } else {
      initialized_ = true;
      status = InitStatus::OK;
    }
    if (status != InitStatus::OK) {
      RTC_LOG(LS_ERROR) << "Audio device initialization failed";
      return -1;
    }
    return 0;
  }

  int32_t Terminate() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    if (!initialized_)
      return 0;
    int32_t err = input_->Terminate();
    err |= output_->Terminate();
    initialized_ = false;
    RTC_DCHECK_EQ(err, 0);
    return err;
  }

  bool Initialized() const override {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return initialized_;
  }

  int16_t PlayoutDevices() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return output_->NumDevices();
  }

  int16_t RecordingDevices() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->NumDevices();
  }

  int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return output_->DeviceName(index, name, guid);
  }
  int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->DeviceName(index, name, guid);
  }

  int32_t SetPlayoutDevice(uint16_t index) override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return output_->SetDevice(index);
  }

  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return output_->SetDevice(device);
  }
  int32_t SetRecordingDevice(uint16_t index) override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->SetDevice(index);
  }

  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->SetDevice(device);
  }

  int32_t PlayoutIsAvailable(bool* available) override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *available = true;
    return 0;
  }

  int32_t InitPlayout() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return output_->InitPlayout();
  }

  bool PlayoutIsInitialized() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return output_->PlayoutIsInitialized();
  }

  int32_t RecordingIsAvailable(bool* available) override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *available = true;
    return 0;
  }

  int32_t InitRecording() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->InitRecording();
  }

  bool RecordingIsInitialized() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->RecordingIsInitialized();
  }

  int32_t StartPlayout() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return output_->StartPlayout();
  }

  int32_t StopPlayout() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return output_->StopPlayout();
  }

  bool Playing() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return output_->Playing();
  }

  int32_t StartRecording() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->StartRecording();
  }

  int32_t StopRecording() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return input_->StopRecording();
  }

  bool Recording() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    return input_->Recording();
  }

  int32_t InitSpeaker() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RTC_DLOG(LS_WARNING) << "This method has no effect";
    return initialized_ ? 0 : -1;
  }

  bool SpeakerIsInitialized() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RTC_DLOG(LS_WARNING) << "This method has no effect";
    return initialized_;
  }

  int32_t InitMicrophone() override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RTC_DLOG(LS_WARNING) << "This method has no effect";
    return initialized_ ? 0 : -1;
  }

  bool MicrophoneIsInitialized() const override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    RTC_DLOG(LS_WARNING) << "This method has no effect";
    return initialized_;
  }

  int32_t SpeakerVolumeIsAvailable(bool* available) override {
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return output_->VolumeIsAvailable(available);
  }

  int32_t SetSpeakerVolume(uint32_t volume) override { return 0; }
  int32_t SpeakerVolume(uint32_t* volume) const override { return 0; }
  int32_t MaxSpeakerVolume(uint32_t* maxVolume) const override { return 0; }
  int32_t MinSpeakerVolume(uint32_t* minVolume) const override { return 0; }

  int32_t MicrophoneVolumeIsAvailable(bool* available) override { return 0; }
  int32_t SetMicrophoneVolume(uint32_t volume) override { return 0; }
  int32_t MicrophoneVolume(uint32_t* volume) const override { return 0; }
  int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override { return 0; }
  int32_t MinMicrophoneVolume(uint32_t* minVolume) const override { return 0; }

  int32_t SpeakerMuteIsAvailable(bool* available) override { return 0; }
  int32_t SetSpeakerMute(bool enable) override { return 0; }
  int32_t SpeakerMute(bool* enabled) const override { return 0; }

  int32_t MicrophoneMuteIsAvailable(bool* available) override { return 0; }
  int32_t SetMicrophoneMute(bool enable) override { return 0; }
  int32_t MicrophoneMute(bool* enabled) const override { return 0; }

  int32_t StereoPlayoutIsAvailable(bool* available) const override {
    // TODO(henrika): improve support.
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *available = true;
    return 0;
  }

  int32_t SetStereoPlayout(bool enable) override {
    // TODO(henrika): improve support.
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return 0;
  }

  int32_t StereoPlayout(bool* enabled) const override {
    // TODO(henrika): improve support.
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *enabled = true;
    return 0;
  }

  int32_t StereoRecordingIsAvailable(bool* available) const override {
    // TODO(henrika): improve support.
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *available = true;
    return 0;
  }

  int32_t SetStereoRecording(bool enable) override {
    // TODO(henrika): improve support.
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return 0;
  }

  int32_t StereoRecording(bool* enabled) const override {
    // TODO(henrika): improve support.
    RTC_LOG(INFO) << __FUNCTION__;
    RTC_DCHECK_RUN_ON(&thread_checker_);
    *enabled = true;
    return 0;
  }

  int32_t PlayoutDelay(uint16_t* delayMS) const override { return 0; }

  bool BuiltInAECIsAvailable() const override { return false; }
  bool BuiltInAGCIsAvailable() const override { return false; }
  bool BuiltInNSIsAvailable() const override { return false; }

  int32_t EnableBuiltInAEC(bool enable) override { return 0; }
  int32_t EnableBuiltInAGC(bool enable) override { return 0; }
  int32_t EnableBuiltInNS(bool enable) override { return 0; }

  int32_t AttachAudioBuffer() {
    RTC_DLOG(INFO) << __FUNCTION__;
    output_->AttachAudioBuffer(audio_device_buffer_.get());
    input_->AttachAudioBuffer(audio_device_buffer_.get());
    return 0;
  }

 private:
  rtc::ThreadChecker thread_checker_;
  const AudioDeviceModule::AudioLayer audio_layer_;
  const std::unique_ptr<AudioInput> input_;
  const std::unique_ptr<AudioOutput> output_;
  std::unique_ptr<AudioDeviceBuffer> audio_device_buffer_;
  bool initialized_ = false;
};

}  // namespace

rtc::scoped_refptr<AudioDeviceModule>
CreateWindowsCoreAudioAudioDeviceModuleFromInputAndOutput(
    AudioDeviceModule::AudioLayer audio_layer,
    std::unique_ptr<AudioInput> audio_input,
    std::unique_ptr<AudioOutput> audio_output) {
  RTC_LOG(INFO) << __FUNCTION__;
  return new rtc::RefCountedObject<WindowsAudioDeviceModule>(
      audio_layer, std::move(audio_input), std::move(audio_output));
}

}  // namespace webrtc_win
}  // namespace webrtc
