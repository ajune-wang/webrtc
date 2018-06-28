/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/win/core_audio_base_win.h"

#include <string>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/win/windows_version.h"

using Microsoft::WRL::ComPtr;

namespace webrtc {
namespace webrtc_win {
namespace {

enum DefaultDeviceType {
  kDefault,
  kDefaultCommunications,
  kDefaultDeviceTypeMaxCount,
};

const char* DirectionToString(CoreAudioBase::Direction direction) {
  switch (direction) {
    case CoreAudioBase::Direction::kOutput:
      return "Output";
    case CoreAudioBase::Direction::kInput:
      return "Input";
    default:
      return "Unkown";
  }
}

const char* SessionStateToString(AudioSessionState state) {
  switch (state) {
    case AudioSessionStateActive:
      return "Active";
    case AudioSessionStateInactive:
      return "Inactive";
    case AudioSessionStateExpired:
      return "Expired";
    default:
      return "Invalid";
  }
}

const char* SessionDisconnectReasonToString(
    AudioSessionDisconnectReason reason) {
  switch (reason) {
    case DisconnectReasonDeviceRemoval:
      return "DeviceRemoval";
    case DisconnectReasonServerShutdown:
      return "ServerShutdown";
    case DisconnectReasonFormatChanged:
      return "FormatChanged";
    case DisconnectReasonSessionLogoff:
      return "SessionLogoff";
    case DisconnectReasonSessionDisconnected:
      return "Disconnected";
    case DisconnectReasonExclusiveModeOverride:
      return "ExclusiveModeOverride";
    default:
      return "Invalid";
  }
}

void Run(void* obj) {
  RTC_DCHECK(obj);
  reinterpret_cast<CoreAudioBase*>(obj)->ThreadRun();
}

}  // namespace

CoreAudioBase::CoreAudioBase(Direction direction,
                             OnDataCallback data_callback,
                             OnErrorCallback error_callback)
    : direction_(direction),
      on_data_callback_(data_callback),
      on_error_callback_(error_callback),
      format_() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction) << "]";

  // Create the event which the audio engine will signal each time a buffer
  // becomes ready to be processed by the client.
  audio_samples_event_.Set(CreateEvent(nullptr, false, false, nullptr));
  RTC_DCHECK(audio_samples_event_.IsValid());

  // Event to be set in Stop() when rendering/capturing shall stop.
  stop_event_.Set(CreateEvent(nullptr, false, false, nullptr));
  RTC_DCHECK(stop_event_.IsValid());

  // Event to be set when it has been detected that an active device has been
  // invalidated or the stream format has changed.
  restart_event_.Set(CreateEvent(nullptr, false, false, nullptr));
  RTC_DCHECK(restart_event_.IsValid());
}

CoreAudioBase::~CoreAudioBase() {
  RTC_DLOG(INFO) << __FUNCTION__;
  RTC_DCHECK_EQ(ref_count_, 1);
}

EDataFlow CoreAudioBase::GetDataFlow() const {
  return direction_ == CoreAudioBase::Direction::kOutput ? eRender : eCapture;
}

int CoreAudioBase::NumberOfActiveDevices() const {
  return core_audio_utility::NumberOfActiveDevices(GetDataFlow());
}

int CoreAudioBase::NumberOfEnumeratedDevices() const {
  const int num_active = NumberOfActiveDevices();
  return num_active > 0 ? num_active + kDefaultDeviceTypeMaxCount : 0;
}

bool CoreAudioBase::IsDefaultDevice(int index) const {
  return index == kDefault;
}

bool CoreAudioBase::IsDefaultCommunicationsDevice(int index) const {
  return index == kDefaultCommunications;
}

bool CoreAudioBase::IsDefaultDevice(const std::string& device_id) const {
  return (IsInput() &&
          (device_id == core_audio_utility::GetDefaultInputDeviceID())) ||
         (IsOutput() &&
          (device_id == core_audio_utility::GetDefaultOutputDeviceID()));
}

bool CoreAudioBase::IsDefaultCommunicationsDevice(
    const std::string& device_id) const {
  return (IsInput() &&
          (device_id ==
           core_audio_utility::GetCommunicationsInputDeviceID())) ||
         (IsOutput() &&
          (device_id == core_audio_utility::GetCommunicationsOutputDeviceID()));
}

bool CoreAudioBase::IsInput() const {
  return direction_ == CoreAudioBase::Direction::kInput;
}

bool CoreAudioBase::IsOutput() const {
  return direction_ == CoreAudioBase::Direction::kOutput;
}

std::string CoreAudioBase::GetDeviceID(int index) const {
  if (index >= NumberOfEnumeratedDevices()) {
    RTC_LOG(LS_ERROR) << "Invalid device index";
    return std::string();
  }

  std::string device_id;
  if (IsDefaultDevice(index)) {
    device_id = IsInput() ? core_audio_utility::GetDefaultInputDeviceID()
                          : core_audio_utility::GetDefaultOutputDeviceID();
  } else if (IsDefaultCommunicationsDevice(index)) {
    device_id = IsInput()
                    ? core_audio_utility::GetCommunicationsInputDeviceID()
                    : core_audio_utility::GetCommunicationsOutputDeviceID();
  } else {
    AudioDeviceNames device_names;
    bool ok = IsInput()
                  ? core_audio_utility::GetInputDeviceNames(&device_names)
                  : core_audio_utility::GetOutputDeviceNames(&device_names);
    if (ok) {
      device_id = device_names[index].unique_id;
    }
  }
  return device_id;
}

int CoreAudioBase::DeviceName(int index,
                              std::string* name,
                              std::string* guid) const {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";
  if (index > NumberOfEnumeratedDevices() - 1) {
    RTC_LOG(LS_ERROR) << "Invalid device index";
    return -1;
  }

  AudioDeviceNames device_names;
  bool ok = IsInput() ? core_audio_utility::GetInputDeviceNames(&device_names)
                      : core_audio_utility::GetOutputDeviceNames(&device_names);
  if (!ok) {
    RTC_LOG(LS_ERROR) << "Failed to get the device name";
    return -1;
  }

  *name = device_names[index].device_name;
  RTC_DLOG(INFO) << "name: " << *name;
  if (guid != nullptr) {
    *guid = device_names[index].unique_id;
    RTC_DLOG(INFO) << "guid: " << guid;
  }
  return 0;
}

bool CoreAudioBase::Init() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";
  RTC_DCHECK(!device_id_.empty());
  RTC_DCHECK(audio_device_buffer_);
  RTC_DCHECK(!audio_client_.Get());

  // Use an existing |device_id_| and set parameters which are required to
  // create an audio client. It is up to the parent class to set |device_id_|.
  // TODO(henrika): improve device notification.
  std::string device_id = device_id_;
  ERole role = eConsole;
  if (IsDefaultDevice(device_id)) {
    device_id = AudioDeviceName::kDefaultDeviceId;
    role = eConsole;
  } else if (IsDefaultCommunicationsDevice(device_id)) {
    device_id = AudioDeviceName::kDefaultCommunicationsDeviceId;
    role = eCommunications;
  }

  // Create an IAudioClient interface which enables us to create and initialize
  // an audio stream between an audio application and the audio engine.
  ComPtr<IAudioClient> audio_client =
      core_audio_utility::CreateClient(device_id, GetDataFlow(), role);
  if (!audio_client.Get()) {
    return false;
  }

  // Retrieve preferred audio input or output parameters for the given client.
  AudioParameters params;
  if (FAILED(core_audio_utility::GetPreferredAudioParameters(audio_client.Get(),
                                                             &params))) {
    return false;
  }

  // Define the output WAVEFORMATEXTENSIBLE format in |format_|.
  WAVEFORMATEX* format = &format_.Format;
  format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format->nChannels = rtc::dchecked_cast<WORD>(params.channels());
  format->nSamplesPerSec = params.sample_rate();
  format->wBitsPerSample = rtc::dchecked_cast<WORD>(params.bits_per_sample());
  format->nBlockAlign = (format->wBitsPerSample / 8) * format->nChannels;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
  format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  // Add the parts which are unique for the WAVE_FORMAT_EXTENSIBLE structure.
  format_.Samples.wValidBitsPerSample =
      rtc::dchecked_cast<WORD>(params.bits_per_sample());
  // TODO(henrika): improve (common for input and output?)
  format_.dwChannelMask = params.channels() == 1
                              ? SPEAKER_FRONT_CENTER
                              : SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
  format_.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
  RTC_DLOG(INFO) << core_audio_utility::WaveFormatExToString(&format_);

  // Verify that the format is supported.
  if (!core_audio_utility::IsFormatSupported(
          audio_client.Get(), AUDCLNT_SHAREMODE_SHARED, &format_)) {
    return false;
  }

  // Initialize the audio stream between the client and the device in shared
  // mode using event-driven buffer handling.
  if (FAILED(core_audio_utility::SharedModeInitialize(
          audio_client.Get(), &format_, audio_samples_event_,
          &endpoint_buffer_size_frames_))) {
    return false;
  }

  // Check device period and the preferred buffer size and log a warning if
  // WebRTC's buffer size is not an even divisor of the preferred buffer size
  // in Core Audio.
  // TODO(henrik): sort out if a non-perfect match really is an issue.
  REFERENCE_TIME device_period;
  if (FAILED(core_audio_utility::GetDevicePeriod(
          audio_client.Get(), AUDCLNT_SHAREMODE_SHARED, &device_period))) {
    return false;
  }
  const double device_period_in_seconds =
      static_cast<double>(
          core_audio_utility::ReferenceTimeToTimeDelta(device_period).ms()) /
      1000.0L;
  const int preferred_frames_per_buffer =
      static_cast<int>(params.sample_rate() * device_period_in_seconds + 0.5);
  RTC_DLOG(INFO) << "preferred_frames_per_buffer: "
                 << preferred_frames_per_buffer;
  if (preferred_frames_per_buffer % params.frames_per_buffer()) {
    RTC_LOG(WARNING) << "Buffer size of " << params.frames_per_buffer()
                     << " is not an even divisor of "
                     << preferred_frames_per_buffer;
  }

  // Create an AudioSessionControl interface given the initialized client.
  // The IAudioControl interface enables a client to configure the control
  // parameters for an audio session and to monitor events in the session.
  ComPtr<IAudioSessionControl> audio_session_control =
      core_audio_utility::CreateAudioSessionControl(audio_client.Get());
  if (!audio_session_control.Get()) {
    return false;
  }

  // The Sndvol program displays volume and mute controls for sessions that
  // are in the active and inactive states.
  AudioSessionState state;
  if (FAILED(audio_session_control->GetState(&state))) {
    return false;
  }
  RTC_DLOG(INFO) << "audio session state: " << SessionStateToString(state);
  RTC_DCHECK_EQ(state, AudioSessionStateInactive);

  // Register the client to receive notifications of session events, including
  // changes in the stream state.
  if (FAILED(audio_session_control->RegisterAudioSessionNotification(this))) {
    return false;
  }

  // Store valid COM interfaces.
  audio_client_ = audio_client;
  audio_session_control_ = audio_session_control;

  return true;
}

bool CoreAudioBase::Start() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";

  audio_thread_ = rtc::MakeUnique<rtc::PlatformThread>(
      Run, this, IsInput() ? "wasapi_capture_thread" : "wasapi_render_thread",
      rtc::kRealtimePriority);
  audio_thread_->Start();
  if (!audio_thread_->IsRunning()) {
    StopThread();
    RTC_LOG(LS_ERROR) << "Failed to start audio thread";
    return false;
  }
  RTC_DLOG(INFO) << "Started thread with name: " << audio_thread_->name();

  // Start streaming data between the endpoint buffer and the audio engine.
  _com_error error = audio_client_->Start();
  if (FAILED(error.Error())) {
    StopThread();
    RTC_LOG(LS_ERROR) << "IAudioClient::Start failed: "
                      << core_audio_utility::ErrorToString(error);
    return false;
  }

  return true;
}

bool CoreAudioBase::Stop() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";

  // Stop streaming and the internal audio thread.
  _com_error error = audio_client_->Stop();
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::Stop failed: "
                      << core_audio_utility::ErrorToString(error);
  }
  StopThread();

  // Flush all pending data and reset the audio clock stream position to 0.
  error = audio_client_->Reset();
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR) << "IAudioClient::Reset failed: "
                      << core_audio_utility::ErrorToString(error);
  }

  if (IsOutput()) {
    // Extra safety check to ensure that the buffers are cleared.
    // If the buffers are not cleared correctly, the next call to Start()
    // would fail with AUDCLNT_E_BUFFER_ERROR at
    // IAudioRenderClient::GetBuffer().
    UINT32 num_queued_frames = 0;
    audio_client_->GetCurrentPadding(&num_queued_frames);
    RTC_DCHECK_EQ(0u, num_queued_frames);
  }

  // Delete the previous registration by the client to receive notifications
  // about audio session events.
  RTC_DLOG(INFO) << "audio session state: "
                 << SessionStateToString(GetAudioSessionState());
  error = audio_session_control_->UnregisterAudioSessionNotification(this);
  if (FAILED(error.Error())) {
    RTC_LOG(LS_ERROR)
        << "IAudioSessionControl::UnregisterAudioSessionNotification failed: "
        << core_audio_utility::ErrorToString(error);
  }

  return true;
}

bool CoreAudioBase::IsVolumeControlAvailable(bool* available) const {
  // A valid IAudioClient is required to access the ISimpleAudioVolume interface
  // properly. It is possible to use IAudioSessionManager::GetSimpleAudioVolume
  // as well but we use the audio client here to ensure that the initialized
  // audio session is visible under group box labeled "Applications" in
  // Sndvol.exe.
  if (!audio_client_.Get()) {
    return false;
  }

  // Try to create an ISimpleAudioVolume instance.
  ComPtr<ISimpleAudioVolume> audio_volume =
      core_audio_utility::CreateSimpleAudioVolume(audio_client_.Get());
  if (!audio_volume.Get()) {
    RTC_DLOG(LS_ERROR) << "Volume control is not supported";
    return false;
  }

  // Try to use the valid volume control.
  float volume = 0.0;
  _com_error error = audio_volume->GetMasterVolume(&volume);
  if (error.Error() != S_OK) {
    RTC_LOG(LS_ERROR) << "ISimpleAudioVolume::GetMasterVolume failed: "
                      << core_audio_utility::ErrorToString(error);
    *available = false;
  }
  RTC_DLOG(INFO) << "master volume for output audio session: " << volume;

  *available = true;
  return false;
}

void CoreAudioBase::StopThread() {
  RTC_DLOG(INFO) << __FUNCTION__;
  if (audio_thread_) {
    if (audio_thread_->IsRunning()) {
      RTC_DLOG(INFO) << "Sets stop_event...";
      SetEvent(stop_event_.Get());
      RTC_DLOG(INFO) << "PlatformThread::Stop...";
      audio_thread_->Stop();
    }
    audio_thread_.reset();

    // Ensure that we don't quit the main thread loop immediately next
    // time Start() is called.
    ResetEvent(stop_event_.Get());
    ResetEvent(restart_event_.Get());
  }
}

bool CoreAudioBase::HandleRestartEvent() {
  RTC_DLOG(INFO) << __FUNCTION__ << "[" << DirectionToString(direction())
                 << "]";
  RTC_DCHECK(audio_thread_);
  RTC_DCHECK(is_restarting_);

  // First, stop audio streaming since this part is common for both input and
  // output clients.
  _com_error error = audio_client_->Stop();
  if (FAILED(error.Error())) {
    // Note that, the FAILED macro does not include the case when
    // IAudioClient::Stop returns S_FALSE(=1) since it is expected when a
    // device has been invalidated. We only end up here when the resulting
    // HRESULT is less than zero, i.e., for a "real" error.
    RTC_LOG(LS_ERROR) << "IAudioClient::Stop failed during restart attempt: "
                      << core_audio_utility::ErrorToString(error);
    is_restarting_ = false;
    return false;
  }

  // Next, let each client (input and/or output) take care of its own restart
  // sequence since each side needs unique actions.
  bool restart_error = on_error_callback_(ErrorType::kRestartIsRequired);

  is_restarting_ = false;
  return restart_error;
}

AudioSessionState CoreAudioBase::GetAudioSessionState() const {
  AudioSessionState state = AudioSessionStateInactive;
  RTC_DCHECK(audio_session_control_.Get());
  _com_error error = audio_session_control_->GetState(&state);
  if (FAILED(error.Error())) {
    RTC_DLOG(LS_ERROR) << "IAudioSessionControl::GetState failed: "
                       << core_audio_utility::ErrorToString(error);
  }
  return state;
}

// TODO(henrika): only used for debugging purposes currently.
ULONG CoreAudioBase::AddRef() {
  return InterlockedIncrement(&ref_count_);
  ;
}

// TODO(henrika): does not call delete this.
ULONG CoreAudioBase::Release() {
  return InterlockedDecrement(&ref_count_);
}

// TODO(henrika): can probably be replaced by "return S_OK" only.
HRESULT CoreAudioBase::QueryInterface(REFIID iid, void** object) {
  if (object == nullptr) {
    return E_POINTER;
  }
  *object = nullptr;
  if (iid == IID_IUnknown || iid == __uuidof(IAudioSessionEvents)) {
    *object = static_cast<IAudioSessionEvents*>(this);
  } else {
    return E_NOINTERFACE;
  }
  return S_OK;
}

// IAudioSessionEvents::OnStateChanged
HRESULT CoreAudioBase::OnStateChanged(AudioSessionState new_state) {
  RTC_DLOG(INFO) << "___" << __FUNCTION__ << "["
                 << DirectionToString(direction())
                 << "] new_state: " << SessionStateToString(new_state);
  return S_OK;
}

// When a session is disconnected because of a device removal or format change
// event, we want to inform the audio thread about the lost audio session and
// trigger an attempt to restart audio using a new (default) device.
HRESULT CoreAudioBase::OnSessionDisconnected(
    AudioSessionDisconnectReason disconnect_reason) {
  RTC_DLOG(INFO) << "___" << __FUNCTION__ << "["
                 << DirectionToString(direction()) << "] reason: "
                 << SessionDisconnectReasonToString(disconnect_reason);
  if (disconnect_reason == DisconnectReasonDeviceRemoval) {
    is_restarting_ = true;
    SetEvent(restart_event_.Get());
  } else if (disconnect_reason == DisconnectReasonFormatChanged) {
    is_restarting_ = true;
    SetEvent(restart_event_.Get());
  }
  return S_OK;
}

// IAudioSessionEvents::OnDisplayNameChanged
HRESULT CoreAudioBase::OnDisplayNameChanged(LPCWSTR new_display_name,
                                            LPCGUID event_context) {
  return S_OK;
}

// IAudioSessionEvents::OnIconPathChanged
HRESULT CoreAudioBase::OnIconPathChanged(LPCWSTR new_icon_path,
                                         LPCGUID event_context) {
  return S_OK;
}

// IAudioSessionEvents::OnSimpleVolumeChanged
HRESULT CoreAudioBase::OnSimpleVolumeChanged(float new_simple_volume,
                                             BOOL new_mute,
                                             LPCGUID event_context) {
  return S_OK;
}

// IAudioSessionEvents::OnChannelVolumeChanged
HRESULT CoreAudioBase::OnChannelVolumeChanged(DWORD channel_count,
                                              float new_channel_volumes[],
                                              DWORD changed_channel,
                                              LPCGUID event_context) {
  return S_OK;
}

// IAudioSessionEvents::OnGroupingParamChanged
HRESULT CoreAudioBase::OnGroupingParamChanged(LPCGUID new_grouping_param,
                                              LPCGUID event_context) {
  return S_OK;
}

void CoreAudioBase::ThreadRun() {
  if (!core_audio_utility::IsMMCSSSupported()) {
    RTC_LOG(LS_ERROR) << "MMCSS is not supported";
    return;
  }
  RTC_DLOG(INFO) << "[" << DirectionToString(direction())
                 << "] ThreadRun starts...";
  // TODO(henrika): difference between "Pro Audio" and "Audio"?
  ScopedMMCSSRegistration mmcss_registration(L"Pro Audio");
  ScopedCOMInitializer com_initializer(ScopedCOMInitializer::kMTA);
  RTC_DCHECK(mmcss_registration.Succeeded());
  RTC_DCHECK(com_initializer.Succeeded());
  RTC_DCHECK(stop_event_.IsValid());
  RTC_DCHECK(audio_samples_event_.IsValid());

  bool streaming = true;
  bool error = false;
  HANDLE wait_array[] = {stop_event_.Get(), restart_event_.Get(),
                         audio_samples_event_.Get()};

  // The device frequency is the frequency generated by the hardware clock in
  // the audio device. The GetFrequency() method reports a constant frequency.
  UINT64 device_frequency = 0;
  _com_error result(S_FALSE);
  if (audio_clock_.Get()) {
    RTC_DCHECK(IsOutput());
    result = audio_clock_->GetFrequency(&device_frequency);
    if (FAILED(result.Error())) {
      RTC_LOG(LS_ERROR) << "IAudioClock::GetFrequency failed: "
                        << core_audio_utility::ErrorToString(result);
    }
  }

  // Keep streaming audio until the stop event or the stream-switch event
  // is signaled. An error event can also break the main thread loop.
  while (streaming && !error) {
    // Wait for a close-down event, stream-switch event or a new render event.
    DWORD wait_result = WaitForMultipleObjects(arraysize(wait_array),
                                               wait_array, false, INFINITE);
    switch (wait_result) {
      case WAIT_OBJECT_0 + 0:
        // |stop_event_| has been set.
        streaming = false;
        break;
      case WAIT_OBJECT_0 + 1:
        // |restart_event_| has been set.
        error = !HandleRestartEvent();
        break;
      case WAIT_OBJECT_0 + 2:
        // |audio_samples_event_| has been set.
        error = !on_data_callback_(device_frequency);
        break;
      default:
        error = true;
        break;
    }
  }

  if (streaming && error) {
    RTC_LOG(LS_ERROR) << "[" << DirectionToString(direction())
                      << "] WASAPI streaming failed.";
    // Stop audio streaming since something has gone wrong in our main thread
    // loop. Note that, we are still in a "started" state, hence a Stop() call
    // is required to join the thread properly.
    result = audio_client_->Stop();
    if (FAILED(result.Error())) {
      RTC_LOG(LS_ERROR) << "IAudioClient::Stop failed: "
                        << core_audio_utility::ErrorToString(result);
    }

    // TODO(henrika): notify clients that something has gone wrong and that
    // this stream should be destroyed instead of reused in the future.
  }

  RTC_DLOG(INFO) << "[" << DirectionToString(direction())
                 << "] ...ThreadRun stops";
}

}  // namespace webrtc_win
}  // namespace webrtc
