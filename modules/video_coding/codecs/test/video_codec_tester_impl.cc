/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_tester_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "api/task_queue/default_task_queue_factory.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/test/video_codec_analyzer.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/sleep.h"
#include "test/testsupport/video_frame_writer.h"

namespace webrtc {
namespace test {

namespace {
using RawVideoSource = VideoCodecTester::RawVideoSource;
using CodedVideoSource = VideoCodecTester::CodedVideoSource;
using EncoderSettings = VideoCodecTester::EncoderSettings;
using EncodingSettings = VideoCodecTester::EncodingSettings;
using DecoderSettings = VideoCodecTester::DecoderSettings;
using PacingSettings = VideoCodecTester::PacingSettings;
using PacingMode = PacingSettings::PacingMode;
using DecodeCallback =
    absl::AnyInvocable<void(const VideoFrame& decoded_frame)>;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

// A thread-safe wrapper for video source to be shared with the quality analyzer
// that reads reference frames from a separate thread.
class SyncRawVideoSource : public VideoCodecAnalyzer::ReferenceVideoSource {
 public:
  explicit SyncRawVideoSource(RawVideoSource* video_source)
      : video_source_(video_source) {}

  absl::optional<VideoFrame> PullFrame() {
    MutexLock lock(&mutex_);
    return video_source_->PullFrame();
  }

  VideoFrame GetFrame(uint32_t timestamp_rtp, Resolution resolution) override {
    MutexLock lock(&mutex_);
    return video_source_->GetFrame(timestamp_rtp, resolution);
  }

 protected:
  RawVideoSource* const video_source_ RTC_GUARDED_BY(mutex_);
  Mutex mutex_;
};

// Pacer calculates delay necessary to keep frame encode or decode call spaced
// from the previous calls by the pacing time. `Delay` is expected to be called
// as close as possible to posting frame encode or decode task. This class is
// not thread safe.
class Pacer {
 public:
  explicit Pacer(PacingSettings settings)
      : settings_(settings), delay_(TimeDelta::Zero()) {}
  Timestamp Schedule(Timestamp timestamp) {
    Timestamp now = Timestamp::Micros(rtc::TimeMicros());
    if (settings_.mode == PacingMode::kNoPacing) {
      return now;
    }

    Timestamp scheduled = now;
    if (prev_scheduled_) {
      scheduled = *prev_scheduled_ + PacingTime(timestamp);
      if (scheduled < now) {
        scheduled = now;
      }
    }

    prev_timestamp_ = timestamp;
    prev_scheduled_ = scheduled;
    return scheduled;
  }

 private:
  TimeDelta PacingTime(Timestamp timestamp) {
    if (settings_.mode == PacingMode::kRealTime) {
      return timestamp - *prev_timestamp_;
    }
    RTC_CHECK_EQ(PacingMode::kConstantRate, settings_.mode);
    return 1 / settings_.constant_rate;
  }

  PacingSettings settings_;
  absl::optional<Timestamp> prev_timestamp_;
  absl::optional<Timestamp> prev_scheduled_;
  TimeDelta delay_;
};

// Task queue that keeps the number of queued tasks below a certain limit. If
// the limit is reached, posting of a next task is blocked until execution of a
// previously posted task starts. This class is not thread-safe.
class LimitedTaskQueue {
 public:
  // The codec tester reads frames from video source in the main thread.
  // Encoding and decoding are done in separate threads. If encoding or
  // decoding is slow, the reading may go far ahead and may buffer too many
  // frames in memory. To prevent this we limit the encoding/decoding queue
  // size. When the queue is full, the main thread and, hence, reading frames
  // from video source is blocked until a previously posted encoding/decoding
  // task starts.
  static constexpr int kMaxTaskQueueSize = 3;

  LimitedTaskQueue() : queue_size_(0) {}

  void PostScheduledTask(absl::AnyInvocable<void() &&> task, Timestamp start) {
    ++queue_size_;
    task_queue_.PostTask([this, task = std::move(task), start]() mutable {
      int wait_ms = static_cast<int>(start.ms() - rtc::TimeMillis());
      if (wait_ms > 0) {
        SleepMs(wait_ms);
      }

      std::move(task)();
      --queue_size_;
      task_executed_.Set();
    });

    task_executed_.Reset();
    if (queue_size_ > kMaxTaskQueueSize) {
      task_executed_.Wait(rtc::Event::kForever);
    }
    RTC_CHECK(queue_size_ <= kMaxTaskQueueSize);
  }

  void PostTaskAndWait(absl::AnyInvocable<void() &&> task) {
    PostScheduledTask(std::move(task), Timestamp::Zero());
    WaitForPreviouslyPostedTasks();
  }

  void WaitForPreviouslyPostedTasks() {
    task_queue_.SendTask([] {});
  }

  TaskQueueForTest task_queue_;
  std::atomic_int queue_size_;
  rtc::Event task_executed_;
};

class TesterY4mWriter {
 public:
  explicit TesterY4mWriter(absl::string_view base_path)
      : base_path_(base_path) {}

  ~TesterY4mWriter() {
    task_queue_.SendTask([] {});
  }

  void Write(const VideoFrame& frame, int spatial_idx) {
    task_queue_.PostTask([this, frame, spatial_idx] {
      if (y4m_writers_.find(spatial_idx) == y4m_writers_.end()) {
        std::string file_path =
            base_path_ + "_s" + std::to_string(spatial_idx) + ".y4m";

        Y4mVideoFrameWriterImpl* y4m_writer = new Y4mVideoFrameWriterImpl(
            file_path, frame.width(), frame.height(), /*fps=*/30);
        RTC_CHECK(y4m_writer);

        y4m_writers_[spatial_idx] =
            std::unique_ptr<VideoFrameWriter>(y4m_writer);
      }

      y4m_writers_.at(spatial_idx)->WriteFrame(frame);
    });
  }

 protected:
  std::string base_path_;
  std::map<int, std::unique_ptr<VideoFrameWriter>> y4m_writers_;
  TaskQueueForTest task_queue_;
};

class TesterIvfWriter {
 public:
  explicit TesterIvfWriter(absl::string_view base_path)
      : base_path_(base_path) {}

  ~TesterIvfWriter() {
    task_queue_.SendTask([] {});
  }

  void Write(const EncodedImage& encoded_frame) {
    task_queue_.PostTask([this, encoded_frame] {
      int spatial_idx = encoded_frame.SpatialIndex().value_or(0);
      if (ivf_file_writers_.find(spatial_idx) == ivf_file_writers_.end()) {
        std::string ivf_path =
            base_path_ + "_s" + std::to_string(spatial_idx) + ".ivf";

        FileWrapper ivf_file = FileWrapper::OpenWriteOnly(ivf_path);
        RTC_CHECK(ivf_file.is_open());

        std::unique_ptr<IvfFileWriter> ivf_writer =
            IvfFileWriter::Wrap(std::move(ivf_file), /*byte_limit=*/0);
        RTC_CHECK(ivf_writer);

        ivf_file_writers_[spatial_idx] = std::move(ivf_writer);
      }

      // To play: ffplay -vcodec vp8|vp9|av1|hevc|h264 filename
      ivf_file_writers_.at(spatial_idx)
          ->WriteFrame(encoded_frame, VideoCodecType::kVideoCodecGeneric);
    });
  }

 protected:
  std::string base_path_;
  std::map<int, std::unique_ptr<IvfFileWriter>> ivf_file_writers_;
  TaskQueueForTest task_queue_;
};

class Decoder : public DecodedImageCallback {
 public:
  Decoder(VideoDecoder* decoder,
          const DecoderSettings& settings,
          VideoCodecAnalyzer* analyzer)
      : decoder_(decoder),
        settings_(settings),
        analyzer_(analyzer),
        pacer_(settings.pacing) {
    RTC_CHECK(analyzer_) << "Analyzer must be provided";
    decoder->RegisterDecodeCompleteCallback(this);
  }

  void Initialize() {
    task_queue_.PostTaskAndWait([this] {
      VideoDecoder::Settings ds;
      // ds.set_codec_type();
      ds.set_number_of_cores(1);
      ds.set_max_render_resolution({1280, 720});
      decoder_->Configure(ds);
    });

    if (settings_.decoder_input_base_path) {
      input_writer_ =
          std::make_unique<TesterIvfWriter>(*settings_.decoder_input_base_path);
    }
    if (settings_.decoder_output_base_path) {
      output_writer_ = std::make_unique<TesterY4mWriter>(
          *settings_.decoder_output_base_path);
    }
  }

  void Decode(const EncodedImage& encoded_frame,
              const CodecSpecificInfo* codec_specific_info) {
    {
      MutexLock lock(&mutex_);
      timestamp_sidx_[encoded_frame.Timestamp()] =
          encoded_frame.SimulcastIndex().value_or(
              encoded_frame.SpatialIndex().value_or(0));
    }

    Timestamp timestamp =
        Timestamp::Micros((encoded_frame.Timestamp() / k90kHz).us());

    task_queue_.PostScheduledTask(
        [this, encoded_frame, codec_specific_info] {
          analyzer_->StartDecode(encoded_frame);
          decoder_->Decode(encoded_frame, /*render_time_ms*/ 0);
          if (input_writer_) {
            input_writer_->Write(encoded_frame);
          }
        },
        pacer_.Schedule(timestamp));
  }

  void Flush() {
    // TODO(webrtc:14852): Add Flush() to VideoDecoder API.
    task_queue_.PostTaskAndWait([this] { decoder_->Release(); });
  }

 protected:
  int Decoded(VideoFrame& decoded_frame) override {
    int sidx;
    {
      MutexLock lock(&mutex_);
      auto it = timestamp_sidx_.find(decoded_frame.timestamp());
      RTC_CHECK(it != timestamp_sidx_.end());
      sidx = it->second;
      timestamp_sidx_.erase(timestamp_sidx_.begin(), sidx);
    }

    analyzer_->FinishDecode(decoded_frame, sidx);
    if (output_writer_) {
      output_writer_->Write(decoded_frame, sidx);
    }

    return WEBRTC_VIDEO_CODEC_OK;
  }

  VideoDecoder* const decoder_;
  const DecoderSettings& settings_;
  VideoCodecAnalyzer* const analyzer_;
  Pacer pacer_;
  LimitedTaskQueue task_queue_;
  std::unique_ptr<TesterIvfWriter> input_writer_;
  std::unique_ptr<TesterY4mWriter> output_writer_;
  std::map<uint32_t, int> timestamp_sidx_ RTC_GUARDED_BY(mutex_);
  Mutex mutex_;
};

class MultiLayerDecoder {
 public:
  MultiLayerDecoder(const std::map<int, VideoDecoder*>& decoders,
                    const DecoderSettings& settings,
                    VideoCodecAnalyzer* analyzer) {
    for (auto& [sidx, decoder] : decoders) {
      decoders_.emplace(sidx, Decoder(decoder, settings, analyzer));
    }
  }

  void Initialize() {
    for (auto& [sidx, decoder] : decoders_) {
      decoder.Initialize();
    }
  }

  void Decode(const EncodedImage& encoded_frame,
              const CodecSpecificInfo* codec_specific_info) {
    // TODO(webrtc:14852): In the case of SVC, pass lower reference spatial
    // layer frame to upper layer decoder(s).
    int sidx = encoded_frame.SimulcastIndex().value_or(
        encoded_frame.SpatialIndex().value_or(0));
    decoders_.at(sidx).Decode(encoded_frame, codec_specific_info);
  }

  void Flush() {
    for (auto& [sidx, decoder] : decoders_) {
      decoder.Flush();
    }
  }

 protected:
  std::map<int, Decoder> decoders_;
};

class Encoder : public EncodedImageCallback {
 public:
  Encoder(VideoEncoder* encoder,
          const EncoderSettings& encoder_settings,
          const std::map<int, EncodingSettings>& frame_settings,
          VideoCodecAnalyzer* analyzer,
          MultiLayerDecoder* decoder)
      : encoder_(encoder),
        encoder_settings_(encoder_settings),
        frame_settings_(frame_settings),
        analyzer_(analyzer),
        decoder_(decoder),
        pacer_(encoder_settings.pacing),
        frame_count_(0) {
    RTC_CHECK(analyzer_) << "Analyzer must be provided";
    if (encoder_settings.encoder_input_base_path) {
      input_writer_ = std::make_unique<TesterY4mWriter>(
          *encoder_settings.encoder_input_base_path);
    }

    if (encoder_settings.encoder_output_base_path) {
      output_writer_ = std::make_unique<TesterIvfWriter>(
          *encoder_settings.encoder_output_base_path);
    }
  }

  void Initialize() {
    task_queue_.PostTaskAndWait([this] {
      const EncodingSettings& first_frame_settings = frame_settings_.at(0);
      Configure(first_frame_settings);
      SetRates(first_frame_settings);
    });
  }

  void Configure(const EncodingSettings& es) {
    VideoCodec vc;
    const EncodingSettings::LayerSettings& layer_settings =
        es.layer_settings.begin()->second;
    vc.width = layer_settings.resolution.width;
    vc.height = layer_settings.resolution.height;
    const DataRate& bitrate = layer_settings.bitrate;
    vc.startBitrate = bitrate.kbps();
    vc.maxBitrate = bitrate.kbps();
    vc.minBitrate = 0;
    vc.maxFramerate = static_cast<uint32_t>(layer_settings.framerate.hertz());
    vc.active = true;
    vc.qpMax = 63;
    vc.numberOfSimulcastStreams = 0;
    vc.mode = webrtc::VideoCodecMode::kRealtimeVideo;
    vc.SetFrameDropEnabled(true);
    vc.SetScalabilityMode(es.scalability_mode);

    vc.codecType = PayloadStringToCodecType(es.sdp_video_format.name);
    if (vc.codecType == kVideoCodecVP8) {
      *(vc.VP8()) = VideoEncoder::GetDefaultVp8Settings();
    } else if (vc.codecType == kVideoCodecVP9) {
      *(vc.VP9()) = VideoEncoder::GetDefaultVp9Settings();
    } else if (vc.codecType == kVideoCodecH264) {
      *(vc.H264()) = VideoEncoder::GetDefaultH264Settings();
    }

    VideoEncoder::Settings ves(
        VideoEncoder::Capabilities(/*loss_notification=*/false),
        /*number_of_cores=*/1,
        /*max_payload_size=*/1440);

    int result = encoder_->InitEncode(&vc, ves);
    RTC_CHECK(result == WEBRTC_VIDEO_CODEC_OK);

    SetRates(es);
  }

  void SetRates(const EncodingSettings& es) {
    VideoEncoder::RateControlParameters rc;
    int num_spatial_layers =
        ScalabilityModeToNumSpatialLayers(es.scalability_mode);
    int num_temporal_layers =
        ScalabilityModeToNumSpatialLayers(es.scalability_mode);
    for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
      for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
        auto layer_settings =
            es.layer_settings.find({.spatial_idx = sidx, .temporal_idx = tidx});
        RTC_CHECK(layer_settings != es.layer_settings.end())
            << "Bitrate for layer S=" << sidx << " T=" << tidx << " is not set";
        rc.bitrate.SetBitrate(sidx, tidx, layer_settings->second.bitrate.bps());
      }
    }

    rc.framerate_fps =
        es.layer_settings.begin()->second.framerate.millihertz() / 1000.0;
    encoder_->SetRates(rc);
  }

  void Encode(const VideoFrame& input_frame) {
    Timestamp timestamp =
        Timestamp::Micros((input_frame.timestamp() / k90kHz).us());

    task_queue_.PostScheduledTask(
        [this, input_frame] {
          analyzer_->StartEncode(input_frame);
          encoder_->Encode(input_frame, /*frame_types=*/nullptr);
        },
        pacer_.Schedule(timestamp));
  }

  void Flush() {
    task_queue_.PostTaskAndWait([this] { encoder_->Release(); });
  }

 protected:
  Result OnEncodedImage(const EncodedImage& encoded_frame,
                        const CodecSpecificInfo* codec_specific_info) override {
    analyzer_->FinishEncode(encoded_frame);
    if (decoder_ != nullptr) {
      decoder_->Decode(encoded_frame, codec_specific_info);
    }

    if (output_writer_ != nullptr) {
      output_writer_->Write(encoded_frame);
    }
    return Result(Result::Error::OK);
  }

  VideoEncoder* const encoder_;
  const EncoderSettings& encoder_settings_;
  const std::map<int, EncodingSettings>& frame_settings_;
  VideoCodecAnalyzer* const analyzer_;
  MultiLayerDecoder* const decoder_;
  Pacer pacer_;
  LimitedTaskQueue task_queue_;
  std::unique_ptr<TesterY4mWriter> input_writer_;
  std::unique_ptr<TesterIvfWriter> output_writer_;
  int frame_count_;
  std::map<uint32_t, int> timestamp_sidx_ RTC_GUARDED_BY(mutex_);
  Mutex mutex_;
};

class MultiLayerEncoder {
 public:
  MultiLayerEncoder(const std::map<int, VideoEncoder*>& encoders,
                    const EncoderSettings& encoder_settings,
                    const std::map<int, EncodingSettings>& frame_settings,
                    MultiLayerDecoder* decoder,
                    VideoCodecAnalyzer* analyzer) {
    for (auto& [sidx, encoder] : encoders) {
      encoders_.emplace(sidx, Encoder(encoder, encoder_settings, frame_settings,
                                      analyzer, decoder));
    }
  }

  void Initialize() {
    for (auto& [sidx, encoder] : encoders_) {
      encoder.Initialize();
    }
  }

  void Encode(const VideoFrame& input_frame) {
    // TODO(webrtc:14852): Support cases with more than one encoder.
    encoders_.at(0).Encode(input_frame);
  }

  void Flush() {
    for (auto& [sidx, encoder] : encoders_) {
      encoder.Flush();
    }
  }

 protected:
  std::map<int, Encoder> encoders_;
};

}  // namespace

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunDecodeTest(
    CodedVideoSource* video_source,
    std::map<int, VideoDecoder*> decoders,
    const DecoderSettings& decoder_settings) {
  VideoCodecAnalyzer analyzer;
  MultiLayerDecoder mld(decoders, decoder_settings, &analyzer);

  mld.Initialize();

  while (auto frame = video_source->PullFrame()) {
    mld.Decode(*frame, /*codec_specific_info=*/nullptr);
  }

  mld.Flush();

  return analyzer.GetStats();
}

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunEncodeTest(
    RawVideoSource* video_source,
    std::map<int, VideoEncoder*> encoders,
    const EncoderSettings& encoder_settings,
    const std::map<int, EncodingSettings>& frame_settings) {
  SyncRawVideoSource sync_source(video_source);
  VideoCodecAnalyzer analyzer;
  MultiLayerEncoder mle(encoders, encoder_settings, frame_settings,
                        /*decoder=*/nullptr, &analyzer);

  mle.Initialize();

  while (auto frame = sync_source.PullFrame()) {
    mle.Encode(*frame);
  }

  mle.Flush();

  return analyzer.GetStats();
}

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunEncodeDecodeTest(
    RawVideoSource* video_source,
    std::map<int, VideoEncoder*> encoders,
    std::map<int, VideoDecoder*> decoders,
    const EncoderSettings& encoder_settings,
    const DecoderSettings& decoder_settings,
    const std::map<int, EncodingSettings>& frame_settings) {
  SyncRawVideoSource sync_source(video_source);
  VideoCodecAnalyzer analyzer(&sync_source);
  MultiLayerDecoder mld(decoders, decoder_settings, &analyzer);
  MultiLayerEncoder mle(encoders, encoder_settings, frame_settings, &mld,
                        &analyzer);

  mle.Initialize();
  mld.Initialize();

  while (auto frame = sync_source.PullFrame()) {
    mle.Encode(*frame);
  }

  mle.Flush();
  mld.Flush();

  return analyzer.GetStats();
}

}  // namespace test
}  // namespace webrtc
