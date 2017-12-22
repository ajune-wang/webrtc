// Copyright 2017 Fabby Inc
#pragma once

#include <stdint.h>

// Thin wrapper around bitmap data.
// data points to a four channel bitmap. data[0]..data[3] stores first pixel,
// data[4]..data[7] second etc. Channel order: data[i + 0] stores Blue channel
// data[i + 1] stores Green channel
// data[i + 2] stores Red channel.
// data[i + 3] stores Alpha channel
struct FabbySDKBGRAImage {
  int width;
  int height;
  uint8_t* data;
};

// Used in real time processing in OpenGL thread
// Structure contains input/output OpenGL texture information from current GL
// thread type: texture type, if texture used as input parameter than this value
// is ignored
//       currently supported only oes textures
struct FabbySDKTexture {
  int textureId;
  int type;
  int width;
  int height;
};

// Rectangle structure
struct FabbySDKRotatedRect {
  float x;
  float y;
  float width;
  float height;
  float angle;  // relative to rect center
};

struct FabbySDKRect {
  int x;
  int y;
  int width;
  int height;
};
