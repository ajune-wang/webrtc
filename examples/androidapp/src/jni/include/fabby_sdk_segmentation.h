//  Copyright © 2017 Fabby Inc. All rights reserved.
#pragma once

#include <android/asset_manager.h>

#include "fabby_sdk.h"
#include "fabby_sdk_image.h"

// Abstract handle to describe Segmenter model. It needs to be initialized once
// with FabbySDKInitSegmenter and destroyed with FabbySDKDestroySegmenter when
// it is not needed anymore.
struct FabbySDKSegmenterHandle {
  void* data;
};

struct FabbySDKSegmenterInfo {
  // Both sides need to be divisible by this number to ensure segmenter is
  // working correctly.
  int side_should_be_divisible_by;

  // Segmentation model was trained for this size of an image,
  // therefore, it is recommended to scale longest side of an input image
  // to this side for better performance.
  int recommended_long_side_size;

  FabbySDKSegmenterHandle handle;
};

// Loads given segmenter model and initializes all internal structures.
FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKInitSegmenterFromFile(const char* path_to_segmenter_model,
                              bool try_to_run_on_gpu,
                              struct FabbySDKSegmenterInfo* info,
                              struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKInitSegmenterFromAsset(AAssetManager* asset_manager,
                               const char* asset_path,
                               bool try_to_run_on_gpu,
                               struct FabbySDKSegmenterInfo* info,
                               struct FabbySDKControlFunctions* functions);

// Destroys internal structures needed for segmenter for the given handle.
FABBY_SDK_EXPORT void FabbySDKDestroySegmenter(
    struct FabbySDKSegmenterHandle handle);

// Calculates segmentation mask for the given input image in BGRA format and
// outputs segmentation mask, where float values of the mask are [0..1].
//
// Note: it is not thread-safe with respect to the given handle, e.g. every
// handle may only be used in at most one FabbySDKSegmentObject call at
// the same time.
FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKSegmentObject(struct FabbySDKSegmenterHandle handle,
                      struct FabbySDKBGRAImage input_image,
                      float* mask,
                      struct FabbySDKControlFunctions* functions);
