/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_OBJC_AUDIO_DEVICE_H_
#define SDK_OBJC_NATIVE_SRC_OBJC_AUDIO_DEVICE_H_

#include <memory>

#import "objc_audio_device_delegate.h"

#include "api/sequence_checker.h"
#include "modules/audio_device/audio_device_generic.h"
#include "rtc_base/buffer.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "sdk/objc/base/RTCMacros.h"

#import "RTCAudioDevice.h"

#include <AudioUnit/AudioUnit.h>

namespace webrtc {

class FineAudioBuffer;

namespace objc_adm {

// Implements full duplex 16-bit mono PCM audio support for iOS using
// externally provided audio device implementation.
//
// An instance must be created and destroyed on one and the same thread.
// All supported public methods must also be called on the same thread.
// A thread checker will RTC_DCHECK if any supported method is called on an
// invalid thread.
//
class ObjCAudioDevice final : public AudioDeviceGeneric {
 public:
  explicit ObjCAudioDevice(id<RTC_OBJC_TYPE(RTCAudioDevice)> audio_device);
  ~ObjCAudioDevice() override;

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

  InitStatus Init() override;
  int32_t Terminate() override;
  bool Initialized() const override;

  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override;

  int32_t InitRecording() override;
  bool RecordingIsInitialized() const override;

  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;

  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override;

  int32_t PlayoutDelay(uint16_t& delayMS) const override;

  // No implementation for playout underrun on iOS. We override it to avoid a
  // periodic log that it isn't available from the base class.
  int32_t GetPlayoutUnderrunCount() const override;

  // Native audio parameters stored during construction.
  // These methods are unique for the iOS implementation.
  int GetPlayoutAudioParameters(AudioParameters* params) const override;
  int GetRecordAudioParameters(AudioParameters* params) const override;

  // These methods are currently not fully implemented on iOS:

  // See audio_device_not_implemented.cc for trivial implementations.
  int32_t ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const override;
  int32_t PlayoutIsAvailable(bool& available) override;
  int32_t RecordingIsAvailable(bool& available) override;
  int16_t PlayoutDevices() override;
  int16_t RecordingDevices() override;
  int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) override;
  int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) override;
  int32_t SetPlayoutDevice(uint16_t index) override;
  int32_t SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device) override;
  int32_t SetRecordingDevice(uint16_t index) override;
  int32_t SetRecordingDevice(AudioDeviceModule::WindowsDeviceType device) override;
  int32_t InitSpeaker() override;
  bool SpeakerIsInitialized() const override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;
  int32_t SpeakerVolumeIsAvailable(bool& available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t& volume) const override;
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override;
  int32_t MinSpeakerVolume(uint32_t& minVolume) const override;
  int32_t MicrophoneVolumeIsAvailable(bool& available) override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t& volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const override;
  int32_t MinMicrophoneVolume(uint32_t& minVolume) const override;
  int32_t MicrophoneMuteIsAvailable(bool& available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool& enabled) const override;
  int32_t SpeakerMuteIsAvailable(bool& available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool& enabled) const override;
  int32_t StereoPlayoutIsAvailable(bool& available) override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool& enabled) const override;
  int32_t StereoRecordingIsAvailable(bool& available) override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool& enabled) const override;

  OSStatus OnDeliverRecordedData(AudioUnitRenderActionFlags* flags,
                                 const AudioTimeStamp* time_stamp,
                                 NSInteger bus_number,
                                 UInt32 num_frames,
                                 const AudioBufferList* io_data,
                                 RTC_OBJC_TYPE(RTCAudioDeviceRenderRecordedDataBlock) renderBlock);
  OSStatus OnGetPlayoutData(AudioUnitRenderActionFlags* flags,
                            const AudioTimeStamp* time_stamp,
                            NSInteger bus_number,
                            UInt32 num_frames,
                            AudioBufferList* io_data);

  void HandleAudioParametersChange();

  void HandleAudioInterrupted();

 private:
  // Uses current `playout_parameters_` and `record_parameters_` to inform the
  // audio device buffer (ADB) about our internal audio parameters.
  void UpdateAudioDeviceBuffer();

  // Since the preferred audio parameters are only hints to the OS, the actual
  // values may be different once the AVAudioSession has been activated.
  // This method asks for the current hardware parameters and takes actions
  // if they should differ from what we have asked for initially. It also
  // defines `playout_parameters_` and `record_parameters_`.
  void SetupAudioBuffers(AudioParameters& parameters,
                         double sample_rate,
                         NSTimeInterval io_buffer_duration);

  // Set to 1 when recording is active and 0 otherwise.
  volatile int recording_;

  // Set to 1 when playout is active and 0 otherwise.
  volatile int playing_;

  bool is_initialized_ RTC_GUARDED_BY(thread_checker_) = false;
  bool is_playout_initialized_ RTC_GUARDED_BY(thread_checker_) = false;
  bool is_recording_initialized_ RTC_GUARDED_BY(thread_checker_) = false;

  // Ensures that methods are called from the same thread as this object is
  // created on.
  SequenceChecker thread_checker_;

  // Native I/O audio thread checker.
  SequenceChecker io_playout_thread_checker_;
  SequenceChecker io_record_thread_checker_;

  // Thread that this object is created on.
  rtc::Thread* thread_;

  // Raw pointer handle provided to us in AttachAudioBuffer(). Owned by the
  // AudioDeviceModuleImpl class and called by AudioDeviceModule::Create().
  // The AudioDeviceBuffer is a member of the AudioDeviceModuleImpl instance
  // and therefore outlives this object.
  AudioDeviceBuffer* audio_device_buffer_;

  // Contains audio parameters (sample rate, #channels, buffer size etc.) for
  // the playout and recording sides. These structure is set in two steps:
  // first, native sample rate and #channels are defined in Init(). Next, the
  // audio session is activated and we verify that the preferred parameters
  // were granted by the OS. At this stage it is also possible to add a third
  // component to the parameters; the native I/O buffer duration.
  // A RTC_CHECK will be hit if we for some reason fail to open an audio session
  // using the specified parameters.
  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;

  // The audio device which actually play and record audio.
  id<RTC_OBJC_TYPE(RTCAudioDevice)> audio_device_;

  // FineAudioBuffer takes an AudioDeviceBuffer which delivers audio data
  // in chunks of 10ms. It then allows for this data to be pulled in
  // a finer or coarser granularity. I.e. interacting with this class instead
  // of directly with the AudioDeviceBuffer one can ask for any number of
  // audio data samples. Is also supports a similar scheme for the recording
  // side.
  // Example: native buffer size can be 128 audio frames at 16kHz sample rate.
  // WebRTC will provide 480 audio frames per 10ms but iOS asks for 128
  // in each callback (one every 8ms). This class can then ask for 128 and the
  // FineAudioBuffer will ask WebRTC for new data only when needed and also
  // cache non-utilized audio between callbacks. On the recording side, iOS
  // can provide audio data frames of size 128 and these are accumulated until
  // enough data to supply one 10ms call exists. This 10ms chunk is then sent
  // to WebRTC and the remaining part is stored.
  std::unique_ptr<FineAudioBuffer> record_fine_audio_buffer_;

  std::unique_ptr<FineAudioBuffer> playout_fine_audio_buffer_;

  // Temporary storage for recorded data.
  rtc::BufferT<int16_t> record_audio_buffer_;

  ObjCAudioDeviceDelegate* audio_device_delegate_;
};
}  // namespace objc_adm
}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_OBJC_AUDIO_DEVICE_H_
