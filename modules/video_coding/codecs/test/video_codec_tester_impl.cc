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
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_bitrate_allocator.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/codecs/test/video_codec_analyzer.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/sleep.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/video_frame_writer.h"

namespace webrtc {
namespace test {

namespace {
using CodedVideoSource = VideoCodecTester::CodedVideoSource;
using VideoSourceSettings = VideoCodecTester::VideoSourceSettings;
using LayerSettings = EncodingSettings::LayerSettings;
using EncoderSettings = VideoCodecTester::EncoderSettings;
using DecoderSettings = VideoCodecTester::DecoderSettings;
using PacingSettings = VideoCodecTester::PacingSettings;
using PacingMode = PacingSettings::PacingMode;
using DecodeCallback =
    absl::AnyInvocable<void(const VideoFrame& decoded_frame)>;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

// A thread-safe raw video frame reader to be shared with the quality analyzer
// that reads reference frames from a separate thread.
class VideoSource : public VideoCodecAnalyzer::ReferenceVideoSource {
 public:
  explicit VideoSource(VideoSourceSettings source_settings)
      : source_settings_(source_settings) {
    frame_reader_ = CreateYuvFrameReader(
        source_settings_.file_path, source_settings_.resolution,
        YuvFrameReaderImpl::RepeatMode::kPingPong);
    RTC_CHECK(frame_reader_);
  }

  // Pulls next frame. Frame RTP timestamp is set accordingly to
  // `EncodingSettings::framerate`.
  VideoFrame PullFrame(uint32_t timestamp_rtp,
                       Resolution resolution,
                       Frequency framerate) {
    MutexLock lock(&mutex_);
    int frame_num;
    auto buffer = frame_reader_->PullFrame(
        &frame_num, resolution,
        {.num = framerate.millihertz<int>(),
         .den = source_settings_.framerate.millihertz<int>()});
    RTC_CHECK(buffer) << "Cannot pull frame. RTP timestamp " << timestamp_rtp;
    frame_num_[timestamp_rtp] = frame_num;
    return VideoFrame::Builder()
        .set_video_frame_buffer(buffer)
        .set_timestamp_rtp(timestamp_rtp)
        .set_timestamp_us((timestamp_rtp / k90kHz).us())
        .build();
  }

  // Reads frame specified by `timestamp_rtp`, scales it to `resolution` and
  // returns. Frame with the given `timestamp_rtp` is expected to be pulled
  // before.
  VideoFrame GetFrame(uint32_t timestamp_rtp, Resolution resolution) {
    MutexLock lock(&mutex_);
    RTC_CHECK(frame_num_.find(timestamp_rtp) != frame_num_.end())
        << "Frame with RTP timestamp " << timestamp_rtp
        << " was not pulled before";
    auto buffer =
        frame_reader_->ReadFrame(frame_num_.at(timestamp_rtp), resolution);
    return VideoFrame::Builder()
        .set_video_frame_buffer(buffer)
        .set_timestamp_rtp(timestamp_rtp)
        .build();
  }

 protected:
  VideoSourceSettings source_settings_;
  std::unique_ptr<FrameReader> frame_reader_;
  std::map<uint32_t, int> frame_num_;
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
    while (queue_size_ > kMaxTaskQueueSize) {
      task_executed_.Wait(rtc::Event::kForever);
    }
    RTC_CHECK(queue_size_ <= kMaxTaskQueueSize);
  }

  void PostTask(absl::AnyInvocable<void() &&> task) {
    PostScheduledTask(std::move(task), Timestamp::Zero());
  }

  void PostTaskAndWait(absl::AnyInvocable<void() &&> task) {
    PostScheduledTask(std::move(task), Timestamp::Zero());
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
      int spatial_idx = encoded_frame.SimulcastIndex().value_or(0);
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
  Decoder(VideoDecoderFactory* decoder_factory,
          const DecoderSettings& decoder_settings,
          VideoCodecAnalyzer* analyzer)
      : decoder_factory_(decoder_factory),
        analyzer_(analyzer),
        pacer_(decoder_settings.pacing_settings) {
    RTC_CHECK(analyzer_) << "Analyzer must be provided";

    if (decoder_settings.decoder_input_base_path) {
      ivf_writer_ = std::make_unique<TesterIvfWriter>(
          *decoder_settings.decoder_input_base_path);
    }

    if (decoder_settings.decoder_output_base_path) {
      y4m_writer_ = std::make_unique<TesterY4mWriter>(
          *decoder_settings.decoder_output_base_path);
    }
  }

  void Initialize(SdpVideoFormat sdp_video_format) {
    decoder_ = decoder_factory_->CreateVideoDecoder(sdp_video_format);
    RTC_CHECK(decoder_) << "Could not create decoder of video format "
                        << sdp_video_format.ToString();

    task_queue_.PostTaskAndWait([this] {
      decoder_->RegisterDecodeCompleteCallback(this);

      VideoDecoder::Settings ds;
      // ds.set_codec_type();
      ds.set_number_of_cores(1);
      ds.set_max_render_resolution({1280, 720});
      decoder_->Configure(ds);
    });
  }

  void Decode(const EncodedImage& encoded_frame) {
    Timestamp timestamp =
        Timestamp::Micros((encoded_frame.RtpTimestamp() / k90kHz).us());

    task_queue_.PostScheduledTask(
        [this, encoded_frame] {
          analyzer_->StartDecode(encoded_frame);
          int error = decoder_->Decode(encoded_frame, /*render_time_ms*/ 0);
          if (error != 0) {
            RTC_LOG(LS_WARNING)
                << "Decode failed with error code " << error
                << " RTP timestamp " << encoded_frame.RtpTimestamp();
          }
        },
        pacer_.Schedule(timestamp));

    if (ivf_writer_) {
      ivf_writer_->Write(encoded_frame);
    }
  }

  void Flush() {
    // TODO(webrtc:14852): Add Flush() to VideoDecoder API.
    task_queue_.PostTaskAndWait([this] { decoder_->Release(); });
  }

 protected:
  int Decoded(VideoFrame& decoded_frame) override {
    analyzer_->FinishDecode(decoded_frame, /*spatial_idx=*/0);

    if (y4m_writer_) {
      y4m_writer_->Write(decoded_frame, /*spatial_idx=*/0);
    }

    return WEBRTC_VIDEO_CODEC_OK;
  }

  VideoDecoderFactory* decoder_factory_;
  std::unique_ptr<VideoDecoder> decoder_;
  VideoCodecAnalyzer* const analyzer_;
  Pacer pacer_;
  LimitedTaskQueue task_queue_;
  std::unique_ptr<TesterIvfWriter> ivf_writer_;
  std::unique_ptr<TesterY4mWriter> y4m_writer_;
  Mutex mutex_;
};

class Encoder : public EncodedImageCallback {
 public:
  using EncodeCallback =
      absl::AnyInvocable<void(const EncodedImage& encoded_frame)>;

  Encoder(VideoEncoderFactory* encoder_factory,
          const EncoderSettings& encoder_settings,
          VideoCodecAnalyzer* analyzer)
      : encoder_factory_(encoder_factory),
        analyzer_(analyzer),
        pacer_(encoder_settings.pacing_settings) {
    RTC_CHECK(analyzer_) << "Analyzer must be provided";

    if (encoder_settings.encoder_input_base_path) {
      y4m_writer_ = std::make_unique<TesterY4mWriter>(
          *encoder_settings.encoder_input_base_path);
    }

    if (encoder_settings.encoder_output_base_path) {
      ivf_writer_ = std::make_unique<TesterIvfWriter>(
          *encoder_settings.encoder_output_base_path);
    }
  }

  void Initialize(const EncodingSettings& encoding_settings) {
    encoder_ = encoder_factory_->CreateVideoEncoder(
        encoding_settings.sdp_video_format);
    RTC_CHECK(encoder_) << "Could not create encoder of video format "
                        << encoding_settings.sdp_video_format.ToString();

    task_queue_.PostTaskAndWait([this, encoding_settings] {
      encoder_->RegisterEncodeCompleteCallback(this);
      Configure(encoding_settings);
      SetRates(encoding_settings);
    });
  }

  void Encode(const VideoFrame& input_frame,
              const EncodingSettings& encoding_settings,
              absl::optional<EncodeCallback> callback = absl::nullopt) {
    {
      MutexLock lock(&mutex_);
      callbacks_[input_frame.timestamp()] = std::move(callback);
    }

    Timestamp timestamp =
        Timestamp::Micros((input_frame.timestamp() / k90kHz).us());

    task_queue_.PostScheduledTask(
        [this, input_frame, encoding_settings] {
          analyzer_->StartEncode(input_frame, encoding_settings);

          if (!last_encoding_settings_ ||
              !IsSameRate(encoding_settings, *last_encoding_settings_)) {
            SetRates(encoding_settings);
          }

          int error = encoder_->Encode(input_frame, /*frame_types=*/nullptr);
          if (error != 0) {
            RTC_LOG(LS_WARNING) << "Encode failed with error code " << error
                                << " RTP timestamp " << input_frame.timestamp();
          }

          last_encoding_settings_ = encoding_settings;
        },
        pacer_.Schedule(timestamp));

    if (y4m_writer_) {
      y4m_writer_->Write(input_frame, /*spatial_idx=*/0);
    }
  }

  void Flush() {
    task_queue_.PostTaskAndWait([this] { encoder_->Release(); });
  }

 protected:
  Result OnEncodedImage(const EncodedImage& encoded_frame,
                        const CodecSpecificInfo* codec_specific_info) override {
    analyzer_->FinishEncode(encoded_frame);

    absl::optional<EncodeCallback> callback;
    {
      MutexLock lock(&mutex_);
      auto it = callbacks_.find(encoded_frame.RtpTimestamp());
      RTC_CHECK(it != callbacks_.end());
      callback = std::move(it->second);
      callbacks_.erase(callbacks_.begin(), it);
    }

    if (callback.has_value()) {
      (*callback)(encoded_frame);
    }

    if (ivf_writer_ != nullptr) {
      ivf_writer_->Write(encoded_frame);
    }

    return Result(Result::Error::OK);
  }

  void Configure(const EncodingSettings& es) {
    const LayerSettings& layers_settings = es.layers_settings.rbegin()->second;
    const DataRate& bitrate = layers_settings.bitrate;

    VideoCodec vc;
    vc.width = layers_settings.resolution.width;
    vc.height = layers_settings.resolution.height;
    vc.startBitrate = bitrate.kbps();
    vc.maxBitrate = bitrate.kbps();
    vc.minBitrate = 0;
    vc.maxFramerate = layers_settings.framerate.hertz<uint32_t>();
    vc.active = true;
    vc.numberOfSimulcastStreams = 0;
    vc.mode = webrtc::VideoCodecMode::kRealtimeVideo;
    vc.SetFrameDropEnabled(true);
    vc.SetScalabilityMode(es.scalability_mode);
    vc.SetVideoEncoderComplexity(VideoCodecComplexity::kComplexityNormal);

    vc.codecType = PayloadStringToCodecType(es.sdp_video_format.name);
    switch (vc.codecType) {
      case kVideoCodecVP8:
        *(vc.VP8()) = VideoEncoder::GetDefaultVp8Settings();
        vc.VP8()->SetNumberOfTemporalLayers(
            ScalabilityModeToNumTemporalLayers(es.scalability_mode));
        vc.qpMax = cricket::kDefaultVideoMaxQpVpx;
        // TODO(webrtc:14852): Configure simulcast.
        break;
      case kVideoCodecVP9:
        *(vc.VP9()) = VideoEncoder::GetDefaultVp9Settings();
        // See LibvpxVp9Encoder::ExplicitlyConfiguredSpatialLayers.
        vc.spatialLayers[0].targetBitrate = vc.maxBitrate;
        vc.qpMax = cricket::kDefaultVideoMaxQpVpx;
        break;
      case kVideoCodecAV1:
        vc.qpMax = cricket::kDefaultVideoMaxQpVpx;
        break;
      case kVideoCodecH264:
        *(vc.H264()) = VideoEncoder::GetDefaultH264Settings();
        vc.qpMax = cricket::kDefaultVideoMaxQpH26x;
        break;
      case kVideoCodecH265:
        vc.qpMax = cricket::kDefaultVideoMaxQpH26x;
        break;
      case kVideoCodecGeneric:
      case kVideoCodecMultiplex:
        RTC_CHECK_NOTREACHED();
        break;
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
        ScalabilityModeToNumTemporalLayers(es.scalability_mode);
    for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
      for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
        auto layers_settings = es.layers_settings.find(
            {.spatial_idx = sidx, .temporal_idx = tidx});
        RTC_CHECK(layers_settings != es.layers_settings.end())
            << "Bitrate for layer S=" << sidx << " T=" << tidx << " is not set";
        rc.bitrate.SetBitrate(sidx, tidx,
                              layers_settings->second.bitrate.bps());
      }
    }

    rc.framerate_fps =
        es.layers_settings.rbegin()->second.framerate.millihertz() / 1000.0;

    encoder_->SetRates(rc);
  }

  bool IsSameRate(const EncodingSettings& a, const EncodingSettings& b) const {
    for (auto [layer_id, layer] : a.layers_settings) {
      const auto& other_layer = b.layers_settings.at(layer_id);
      if (layer.bitrate != other_layer.bitrate ||
          layer.framerate != other_layer.framerate) {
        return false;
      }
    }

    return true;
  }

  VideoEncoderFactory* const encoder_factory_;
  std::unique_ptr<VideoEncoder> encoder_;
  VideoCodecAnalyzer* const analyzer_;
  Pacer pacer_;
  absl::optional<EncodingSettings> last_encoding_settings_;
  std::unique_ptr<VideoBitrateAllocator> bitrate_allocator_;
  LimitedTaskQueue task_queue_;
  std::unique_ptr<TesterY4mWriter> y4m_writer_;
  std::unique_ptr<TesterIvfWriter> ivf_writer_;
  std::map<uint32_t, int> sidx_ RTC_GUARDED_BY(mutex_);
  std::map<uint32_t, absl::optional<EncodeCallback>> callbacks_
      RTC_GUARDED_BY(mutex_);
  Mutex mutex_;
};

}  // namespace

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunDecodeTest(
    CodedVideoSource* video_source,
    VideoDecoderFactory* decoder_factory,
    const DecoderSettings& decoder_settings,
    const SdpVideoFormat& sdp_video_format) {
  VideoCodecAnalyzer analyzer;
  Decoder decoder(decoder_factory, decoder_settings, &analyzer);

  decoder.Initialize(sdp_video_format);

  while (auto frame = video_source->PullFrame()) {
    decoder.Decode(*frame);
  }

  decoder.Flush();

  return analyzer.GetStats();
}

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunEncodeTest(
    const VideoSourceSettings& source_settings,
    VideoEncoderFactory* encoder_factory,
    const EncoderSettings& encoder_settings,
    const std::map<uint32_t, EncodingSettings>& encoding_settings) {
  VideoSource video_source(source_settings);
  VideoCodecAnalyzer analyzer;
  Encoder encoder(encoder_factory, encoder_settings, &analyzer);

  encoder.Initialize(encoding_settings.begin()->second);

  for (const auto& [timestamp_rtp, frame_settings] : encoding_settings) {
    const EncodingSettings::LayerSettings& top_layer =
        frame_settings.layers_settings.rbegin()->second;

    VideoFrame source_frame = video_source.PullFrame(
        timestamp_rtp, top_layer.resolution, top_layer.framerate);

    encoder.Encode(source_frame, frame_settings);
  }

  encoder.Flush();

  return analyzer.GetStats();
}

std::unique_ptr<VideoCodecStats> VideoCodecTesterImpl::RunEncodeDecodeTest(
    const VideoSourceSettings& source_settings,
    VideoEncoderFactory* encoder_factory,
    VideoDecoderFactory* decoder_factory,
    const EncoderSettings& encoder_settings,
    const DecoderSettings& decoder_settings,
    const std::map<uint32_t, EncodingSettings>& encoding_settings) {
  VideoSource video_source(source_settings);
  VideoCodecAnalyzer analyzer(&video_source);
  Decoder decoder(decoder_factory, decoder_settings, &analyzer);
  Encoder encoder(encoder_factory, encoder_settings, &analyzer);

  encoder.Initialize(encoding_settings.begin()->second);
  decoder.Initialize(encoding_settings.begin()->second.sdp_video_format);

  for (const auto& [timestamp_rtp, frame_settings] : encoding_settings) {
    const EncodingSettings::LayerSettings& top_layer =
        frame_settings.layers_settings.rbegin()->second;

    VideoFrame source_frame = video_source.PullFrame(
        timestamp_rtp, top_layer.resolution, top_layer.framerate);

    encoder.Encode(source_frame, frame_settings,
                   [&decoder](const EncodedImage& encoded_frame) {
                     decoder.Decode(encoded_frame);
                   });
  }

  encoder.Flush();
  decoder.Flush();

  return analyzer.GetStats();
}

}  // namespace test
}  // namespace webrtc
