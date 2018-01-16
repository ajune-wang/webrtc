#ifndef MODULES_VIDEO_CODING_CODECS_STEREO_INCLUDE_MULTIPLEX_ENCODED_IMAGE_PACKER_H_
#define MODULES_VIDEO_CODING_CODECS_STEREO_INCLUDE_MULTIPLEX_ENCODED_IMAGE_PACKER_H_
#include <vector>

#include "common_types.h"
#include "common_video/include/video_frame.h"

namespace webrtc {

struct MultiplexOffsetsHeader {
  uint8_t frame_count;
  uint16_t picture_index;
  uint32_t first_frame_header_offset;
};

struct MultiplexFrameHeader {
  uint32_t next_frame_index_header_offset;
  uint8_t frame_index;
  uint32_t bitstream_offset;
  uint32_t bitstream_length;
  VideoCodecType codec_type;
  FrameType frame_type;
};

struct MultiplexImageComponent {
  VideoCodecType codec_type;
  int frame_index;
  EncodedImage encoded_image;
};

struct MultiplexImage {
  int picture_index;
  int frame_count;
  std::vector<MultiplexImageComponent> image_components;

  MultiplexImage(int picture_index, int frame_count);
};

class MultiplexEncodedImagePacker {
 public:
  // Note: It is caller responsibility to release the buffer of the result.
  static EncodedImage PackAndRelease(MultiplexImage);

  // Note: The image components just share the memory with |combined_image|.
  static MultiplexImage Depack(EncodedImage combined_image);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_STEREO_INCLUDE_MULTIPLEX_ENCODED_IMAGE_PACKER_H_
