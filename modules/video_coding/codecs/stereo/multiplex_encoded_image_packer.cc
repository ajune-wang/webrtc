#include "modules/video_coding/codecs/stereo/include/multiplex_encoded_image_packer.h"

#include "modules/rtp_rtcp/source/byte_io.h"

namespace webrtc {
void PackHeader(uint8_t* buffer, MultiplexOffsetsHeader header) {
  int offset = 0;
  ByteWriter<uint8_t>::WriteBigEndian(buffer + offset, header.frame_count);
  offset += sizeof(uint8_t);

  ByteWriter<uint16_t>::WriteBigEndian(buffer + offset, header.picture_index);
  offset += sizeof(uint16_t);

  ByteWriter<uint32_t>::WriteBigEndian(buffer + offset,
                                       header.first_frame_header_offset);
  offset += sizeof(uint32_t);

  RTC_DCHECK_EQ(sizeof(MultiplexOffsetsHeader), offset);
}

MultiplexOffsetsHeader DepackHeader(uint8_t* buffer) {
  MultiplexOffsetsHeader header;
  int offset = 0;
  header.frame_count = ByteReader<uint8_t>::ReadBigEndian(buffer + offset);
  offset += sizeof(uint8_t);

  header.picture_index = ByteReader<uint16_t>::ReadBigEndian(buffer + offset);
  offset += sizeof(uint16_t);

  header.first_frame_header_offset =
      ByteReader<uint32_t>::ReadBigEndian(buffer + offset);
  offset += sizeof(uint32_t);

  RTC_DCHECK_EQ(sizeof(MultiplexOffsetsHeader), offset);

  return header;
}

void PackFrameHeader(uint8_t* buffer, MultiplexFrameHeader frame_header) {
  int offset = 0;
  ByteWriter<uint32_t>::WriteBigEndian(
      buffer + offset, frame_header.next_frame_index_header_offset);
  offset += sizeof(uint32_t);

  ByteWriter<uint32_t>::WriteBigEndian(
      buffer + offset, frame_header.next_frame_index_header_offset);
  offset += sizeof(uint32_t);

  ByteWriter<uint8_t>::WriteBigEndian(buffer + offset,
                                      frame_header.frame_index);
  offset += sizeof(uint8_t);

  ByteWriter<uint32_t>::WriteBigEndian(buffer + offset,
                                       frame_header.bitstream_offset);
  offset += sizeof(uint32_t);

  ByteWriter<uint32_t>::WriteBigEndian(buffer + offset,
                                       frame_header.bitstream_length);
  offset += sizeof(uint32_t);

  ByteWriter<uint8_t>::WriteBigEndian(buffer + offset, frame_header.codec_type);
  offset += sizeof(uint8_t);

  ByteWriter<uint8_t>::WriteBigEndian(buffer + offset, frame_header.frame_type);
  offset += sizeof(uint8_t);

  RTC_DCHECK_EQ(sizeof(MultiplexFrameHeader), offset);
}

MultiplexFrameHeader DepackFrameHeader(uint8_t* buffer) {
  MultiplexFrameHeader frame_header;
  int offset = 0;

  frame_header.next_frame_index_header_offset =
      ByteReader<uint32_t>::ReadBigEndian(buffer + offset);
  offset += sizeof(uint32_t);

  frame_header.next_frame_index_header_offset =
      ByteReader<uint32_t>::ReadBigEndian(buffer + offset);
  offset += sizeof(uint32_t);

  frame_header.frame_index =
      ByteReader<uint8_t>::ReadBigEndian(buffer + offset);
  offset += sizeof(uint8_t);

  frame_header.bitstream_offset =
      ByteReader<uint32_t>::ReadBigEndian(buffer + offset);
  offset += sizeof(uint32_t);

  frame_header.bitstream_length =
      ByteReader<uint32_t>::ReadBigEndian(buffer + offset);
  offset += sizeof(uint32_t);

  frame_header.codec_type = static_cast<VideoCodecType>(
      ByteReader<uint8_t>::ReadBigEndian(buffer + offset));
  offset += sizeof(uint8_t);

  frame_header.frame_type = static_cast<FrameType>(
      ByteReader<uint8_t>::ReadBigEndian(buffer + offset));
  offset += sizeof(uint8_t);

  RTC_DCHECK_EQ(sizeof(MultiplexFrameHeader), offset);
  return frame_header;
}

void PackBitstream(uint8_t* buffer, MultiplexImageComponent image) {
  memcpy(buffer, image.encoded_image._buffer, image.encoded_image._length);
}

EncodedImage MultiplexEncodedImagePacker::Pack(MultiplexImage multiplex_image) {
  MultiplexOffsetsHeader header;
  std::vector<MultiplexFrameHeader> frame_headers;

  header.frame_count = multiplex_image.image_components.size();
  header.picture_index = multiplex_image.picture_index;
  int header_offset = sizeof(MultiplexOffsetsHeader);
  header.first_frame_header_offset = header_offset;
  int bitstream_offset =
      header_offset + sizeof(MultiplexFrameHeader) * header.frame_count;

  std::vector<MultiplexImageComponent>& images =
      multiplex_image.image_components;
  EncodedImage combined_image = images[0].encoded_image;
  for (size_t i = 0; i < images.size(); i++) {
    MultiplexFrameHeader frame_header;
    header_offset += sizeof(MultiplexFrameHeader);
    frame_header.next_frame_index_header_offset = header_offset;
    frame_header.frame_index = images[i].frame_index;

    frame_header.bitstream_offset = bitstream_offset;
    frame_header.bitstream_length = images[i].encoded_image._length;
    bitstream_offset += frame_header.bitstream_length;

    frame_header.codec_type = images[i].codec_type;
    frame_header.frame_type = images[i].encoded_image._frameType;

    if (frame_header.frame_type == FrameType::kVideoFrameDelta) {
      combined_image._frameType = FrameType::kVideoFrameDelta;
    }

    frame_headers.push_back(frame_header);
  }

  combined_image._length = combined_image._size = bitstream_offset;
  combined_image._buffer = new uint8_t[combined_image._length];

  // header
  PackHeader(combined_image._buffer, header);
  header_offset = header.first_frame_header_offset;

  // Frame Header
  for (size_t i = 0; i < images.size(); i++) {
    PackFrameHeader(combined_image._buffer + header_offset, frame_headers[i]);
    header_offset = frame_headers[i].next_frame_index_header_offset;
  }

  // Bitstreams
  for (size_t i = 0; i < images.size(); i++) {
    PackBitstream(combined_image._buffer + frame_headers[i].bitstream_offset,
                  images[i]);
  }

  return combined_image;
}

MultiplexImage MultiplexEncodedImagePacker::Depack(
    EncodedImage combined_image) {
  MultiplexImage multiplex_image;

  MultiplexOffsetsHeader header = DepackHeader(combined_image._buffer);
  multiplex_image.picture_index = header.picture_index;

  std::vector<MultiplexFrameHeader> frame_headers;

  int header_offset = header.first_frame_header_offset;
  for (int i = 0; i < header.frame_count; i++) {
    frame_headers.push_back(
        DepackFrameHeader(combined_image._buffer + header_offset));
    header_offset = frame_headers.back().next_frame_index_header_offset;
  }

  for (size_t i = 0; i < frame_headers.size(); i++) {
    MultiplexImageComponent image_component;
    image_component.frame_index = frame_headers[i].frame_index;
    image_component.codec_type = frame_headers[i].codec_type;

    EncodedImage encoded_image = combined_image;
    encoded_image._frameType = frame_headers[i].frame_type;
    encoded_image._length = encoded_image._size =
        frame_headers[i].bitstream_length;
    encoded_image._buffer =
        combined_image._buffer + frame_headers[i].bitstream_offset;

    image_component.encoded_image = encoded_image;

    multiplex_image.image_components.push_back(image_component);
  }

  return multiplex_image;
}

}  // namespace webrtc
