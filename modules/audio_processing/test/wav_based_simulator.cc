/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/wav_based_simulator.h"

#include <stdio.h>
#include <iostream>

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/test/test_utils.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

std::vector<WavBasedSimulator::SimulationEventType>
WavBasedSimulator::GetCustomEventChain(const std::string& filename) {
  std::vector<WavBasedSimulator::SimulationEventType> call_chain;
  FILE* stream = OpenFile(filename.c_str(), "r");

  RTC_CHECK(stream) << "Could not open the custom call order file, reverting "
                       "to using the default call order";

  char c;
  size_t num_read = fread(&c, sizeof(char), 1, stream);
  while (num_read > 0) {
    switch (c) {
      case 'r':
        call_chain.push_back(SimulationEventType::kProcessReverseStream);
        break;
      case 'c':
        call_chain.push_back(SimulationEventType::kProcessStream);
        break;
      case '\n':
        break;
      default:
        FATAL() << "Incorrect custom call order file, reverting to using the "
                   "default call order";
        fclose(stream);
        return WavBasedSimulator::GetDefaultEventChain();
    }

    num_read = fread(&c, sizeof(char), 1, stream);
  }

  fclose(stream);
  return call_chain;
}

WavBasedSimulator::WavBasedSimulator(
    const SimulationSettings& settings,
    std::unique_ptr<AudioProcessingBuilder> ap_builder)
    : AudioProcessingSimulator(settings, std::move(ap_builder)) {}

WavBasedSimulator::~WavBasedSimulator() = default;

std::vector<WavBasedSimulator::SimulationEventType>
WavBasedSimulator::GetDefaultEventChain() {
  std::vector<WavBasedSimulator::SimulationEventType> call_chain(2);
  call_chain[0] = SimulationEventType::kProcessStream;
  call_chain[1] = SimulationEventType::kProcessReverseStream;
  return call_chain;
}

void WavBasedSimulator::PrepareProcessStreamCall() {
  if (settings_.fixed_interface) {
    CopyToAudioFrame(*in_buf_, &fwd_frame_);
  }
  ap_->set_stream_key_pressed(settings_.use_ts && (*settings_.use_ts));

  if (!settings_.use_stream_delay || *settings_.use_stream_delay) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->set_stream_delay_ms(
                     settings_.stream_delay ? *settings_.stream_delay : 0));
  }
}

void WavBasedSimulator::PrepareReverseProcessStreamCall() {
  if (settings_.fixed_interface) {
    CopyToAudioFrame(*reverse_in_buf_, &rev_frame_);
  }
}

void WavBasedSimulator::Process() {
  if (settings_.custom_call_order_filename) {
    call_chain_ = WavBasedSimulator::GetCustomEventChain(
        *settings_.custom_call_order_filename);
  } else {
    call_chain_ = WavBasedSimulator::GetDefaultEventChain();
  }
  CreateAudioProcessor();

  Initialize();

  // Initialize the frames and inits for which to control the dumping of data.
  const bool timed_data_dump =
      settings_.dump_start_seconds || settings_.dump_end_seconds ||
      settings_.dump_start_frame || settings_.dump_end_frame;
  int frame_to_activate_data_dumping = -1;
  int frame_to_deactivate_data_dumping = -1;
  RTC_CHECK(!settings_.dump_internal_data || WEBRTC_APM_DEBUG_DUMP == 1);
  if (timed_data_dump) {
    // Set the frame for which to activate data dumping.
    ApmDataDumper::SetActivated(
        !(settings_.dump_start_frame || settings_.dump_start_seconds));
    int frame_to_activate_data_dumping = -1;
    if (settings_.dump_start_frame) {
      RTC_CHECK(!settings_.dump_start_seconds);
      frame_to_activate_data_dumping = *settings_.dump_start_frame;
    } else if (settings_.dump_start_seconds) {
      RTC_CHECK(!settings_.dump_start_frame);
      frame_to_activate_data_dumping =
          static_cast<int>(floor((*settings_.dump_start_seconds) * 100));
    }

    // Set the frame for which to deactivate data dumping.
    int frame_to_deactivate_data_dumping = -1;
    if (settings_.dump_end_frame) {
      RTC_CHECK(!settings_.dump_end_seconds);
      frame_to_deactivate_data_dumping = *settings_.dump_end_frame;
    } else if (settings_.dump_end_seconds) {
      RTC_CHECK(!settings_.dump_end_frame);
      frame_to_deactivate_data_dumping =
          static_cast<int>(floor((*settings_.dump_end_seconds) * 100));
    }
  } else {
    ApmDataDumper::SetActivated(settings_.dump_internal_data);
  }

  bool samples_left_to_process = true;
  int call_chain_index = 0;
  int capture_frames_since_init = 0;
  while (samples_left_to_process) {
    switch (call_chain_[call_chain_index]) {
      case SimulationEventType::kProcessStream:
        // Active/deactivate dumping of data.
        if (timed_data_dump) {
          if (capture_frames_since_init == frame_to_activate_data_dumping) {
            ApmDataDumper::SetActivated(true);
          }
          if (capture_frames_since_init == frame_to_deactivate_data_dumping) {
            ApmDataDumper::SetActivated(false);
          }
        }

        samples_left_to_process = HandleProcessStreamCall();
        ++capture_frames_since_init;
        break;
      case SimulationEventType::kProcessReverseStream:
        if (settings_.reverse_input_filename) {
          samples_left_to_process = HandleProcessReverseStreamCall();
        }
        break;
      default:
        RTC_CHECK(false);
    }

    call_chain_index = (call_chain_index + 1) % call_chain_.size();
  }

  DestroyAudioProcessor();
}

void WavBasedSimulator::Analyze() {
  std::unique_ptr<WavReader> in_file(
      new WavReader(settings_.input_filename->c_str()));
  int input_sample_rate_hz = in_file->sample_rate();
  int input_num_channels = in_file->num_channels();
  buffer_reader_.reset(new ChannelBufferWavReader(std::move(in_file)));

  int output_sample_rate_hz = settings_.output_sample_rate_hz
                                  ? *settings_.output_sample_rate_hz
                                  : input_sample_rate_hz;
  int output_num_channels = settings_.output_num_channels
                                ? *settings_.output_num_channels
                                : input_num_channels;

  int reverse_sample_rate_hz = 48000;
  int reverse_num_channels = 1;
  int reverse_output_sample_rate_hz = 48000;
  int reverse_output_num_channels = 1;
  if (settings_.reverse_input_filename) {
    std::unique_ptr<WavReader> reverse_in_file(
        new WavReader(settings_.reverse_input_filename->c_str()));
    reverse_sample_rate_hz = reverse_in_file->sample_rate();
    reverse_num_channels = reverse_in_file->num_channels();
    reverse_buffer_reader_.reset(
        new ChannelBufferWavReader(std::move(reverse_in_file)));

    reverse_output_sample_rate_hz =
        settings_.reverse_output_sample_rate_hz
            ? *settings_.reverse_output_sample_rate_hz
            : reverse_sample_rate_hz;
    reverse_output_num_channels = settings_.reverse_output_num_channels
                                      ? *settings_.reverse_output_num_channels
                                      : reverse_num_channels;
  }

  std::cout << "Inits:" << std::endl;
  std::cout << "1: -->" << std::endl;
  std::cout << " Time:" << std::endl;
  std::cout << "  Capture: 0 s (0 frames) Render: 0 s (0 frames)" << std::endl;
  std::cout << " Configuration:" << std::endl;
  std::cout << "  Capture" << std::endl;
  std::cout << "   Input" << std::endl;
  std::cout << "    " << input_num_channels << " channels" << std::endl;
  std::cout << "    " << input_sample_rate_hz << " Hz" << std::endl;
  std::cout << "   Output" << std::endl;
  std::cout << "    " << output_num_channels << " channels" << std::endl;
  std::cout << "    " << output_sample_rate_hz << " Hz" << std::endl;
  std::cout << "  Render" << std::endl;
  std::cout << "   Input" << std::endl;
  std::cout << "    " << reverse_num_channels << " channels" << std::endl;
  std::cout << "    " << reverse_sample_rate_hz << " Hz" << std::endl;
  std::cout << "   Output" << std::endl;
  std::cout << "    " << reverse_output_num_channels << " channels"
            << std::endl;
  std::cout << "    " << reverse_output_sample_rate_hz << " Hz" << std::endl;
}

bool WavBasedSimulator::HandleProcessStreamCall() {
  bool samples_left_to_process = buffer_reader_->Read(in_buf_.get());
  if (samples_left_to_process) {
    PrepareProcessStreamCall();
    ProcessStream(settings_.fixed_interface);
  }
  return samples_left_to_process;
}

bool WavBasedSimulator::HandleProcessReverseStreamCall() {
  bool samples_left_to_process =
      reverse_buffer_reader_->Read(reverse_in_buf_.get());
  if (samples_left_to_process) {
    PrepareReverseProcessStreamCall();
    ProcessReverseStream(settings_.fixed_interface);
  }
  return samples_left_to_process;
}

void WavBasedSimulator::Initialize() {
  std::unique_ptr<WavReader> in_file(
      new WavReader(settings_.input_filename->c_str()));
  int input_sample_rate_hz = in_file->sample_rate();
  int input_num_channels = in_file->num_channels();
  buffer_reader_.reset(new ChannelBufferWavReader(std::move(in_file)));

  int output_sample_rate_hz = settings_.output_sample_rate_hz
                                  ? *settings_.output_sample_rate_hz
                                  : input_sample_rate_hz;
  int output_num_channels = settings_.output_num_channels
                                ? *settings_.output_num_channels
                                : input_num_channels;

  int reverse_sample_rate_hz = 48000;
  int reverse_num_channels = 1;
  int reverse_output_sample_rate_hz = 48000;
  int reverse_output_num_channels = 1;
  if (settings_.reverse_input_filename) {
    std::unique_ptr<WavReader> reverse_in_file(
        new WavReader(settings_.reverse_input_filename->c_str()));
    reverse_sample_rate_hz = reverse_in_file->sample_rate();
    reverse_num_channels = reverse_in_file->num_channels();
    reverse_buffer_reader_.reset(
        new ChannelBufferWavReader(std::move(reverse_in_file)));

    reverse_output_sample_rate_hz =
        settings_.reverse_output_sample_rate_hz
            ? *settings_.reverse_output_sample_rate_hz
            : reverse_sample_rate_hz;
    reverse_output_num_channels = settings_.reverse_output_num_channels
                                      ? *settings_.reverse_output_num_channels
                                      : reverse_num_channels;
  }

  SetupBuffersConfigsOutputs(
      input_sample_rate_hz, output_sample_rate_hz, reverse_sample_rate_hz,
      reverse_output_sample_rate_hz, input_num_channels, output_num_channels,
      reverse_num_channels, reverse_output_num_channels);
}

}  // namespace test
}  // namespace webrtc
