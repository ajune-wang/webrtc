//  Copyright © 2017 Fabby Inc. All rights reserved.
#pragma once

#include <android/asset_manager.h>
#include "fabby_sdk.h"
#import "fabby_sdk_image.h"

// Abstract handle to describe VideoSegmenter model. It needs to be initialized
// once with FabbySDKInitVideoSegmenter and destroyed with
// FabbySDKDestroyVideoSegmenter when it is not needed anymore.
struct FabbySDKVideoSegmenterHandle {
  void* data;
};

struct FabbySDKVideoSegmenterInfo {
  // Both sides need to be divisible by this number to ensure segmenter is
  // working correctly.
  int side_should_be_divisible_by;

  // Segmentation model was trained for this size of an image,
  // therefore, it is recommended to scale longest side of an input image
  // to this side for better performance.
  int recommended_long_side_size;

  struct FabbySDKVideoSegmenterHandle handle;
};

// Loads given segmenter model and initializes all internal structures.
//
// Returns false if segmenter was not initialized properly.
// Warning: This method must be called in Open GL Thread
FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKInitVideoSegmenterFromFile(const char* path_to_segmenter_model,
                                   struct FabbySDKVideoSegmenterInfo* info,
                                   struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKInitVideoSegmenterFromAsset(AAssetManager* asset_manager,
                                    const char* asset_path,
                                    struct FabbySDKVideoSegmenterInfo* info,
                                    struct FabbySDKControlFunctions* functions);

// Destroys internal structures needed for segmenter for the given handle.
FABBY_SDK_EXPORT void FabbySDKDestroyVideoSegmenter(
    struct FabbySDKVideoSegmenterHandle handle);

// Segmentation internally uses previous frames. If you pause segmentation and
// restart it later, call this function before calling
// FabbySDKVideoSegmentObject.
FABBY_SDK_EXPORT void FabbySDKResetVideoSegmenter(
    struct FabbySDKVideoSegmenterHandle handle);

// Segments oes texture @image from current OpenGL Thread
// cameraAngle - orientation from camera in degrees, can be 90 and 270
// flip - if != 0 will flip input image before sending it to segmeneter.
// rotatedTexture - an output parameter. If non null information about opengl
// texture which holds the image which is
//   rotated according to @cameraAngle and @flip params.
// maskTexture - an output parameter. If non null information about opengl
// texture with mask will be filled in. Warning: information stored in
// @rotatedTexture and @maskTexture will be valid only till the next
//   FabbySDKVideoSegmentObject call.
// Warning: this method should be called from OpenGL Thread
FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKVideoSegmentObject(struct FabbySDKVideoSegmenterHandle handle,
                           struct FabbySDKTexture image,
                           int cameraAngle,
                           int flip,
                           struct FabbySDKTexture* rotatedTexture,
                           struct FabbySDKTexture* maskTexture,
                           struct FabbySDKControlFunctions* functions);
