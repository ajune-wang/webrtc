//  Copyright © 2017 Fabby Inc. All rights reserved.

#pragma once

#include <android/asset_manager.h>

#include "fabby_sdk.h"
#include "fabby_sdk_image.h"

// Abstract handle to describe Landmark detection model. It needs to be
// initialized once with FabbySDKInitLandmarksDetector and destroyed with
// FabbySDKDestroyLandmarksDetector when it is not needed anymore.
struct FabbySDKLandmarksDetectorHandle {
  void* data;
};

struct FabbySDKLandmarksDetectorInfo {
  // Number of landmarks returned by model.
  int landmarks_count;
  // Width input image should be.
  int width;
  // Height input image should be.
  int height;
  struct FabbySDKLandmarksDetectorHandle handle;
};

// Coordinates of single landmark. values are in range
// [0, FabbySDKLandmarksDetectorInfo::width), [0,
// FabbySDKLandmarksDetectorInfo::height) [0, 0] is in top left corner.
struct FabbySDKLandmark {
  float x;
  float y;
};

FABBY_SDK_EXPORT FABBY_SDK_RESULT FabbySDKInitLandmarksDetectorFromFile(
    const char* model_path,
    bool try_to_run_on_gpu,
    struct FabbySDKLandmarksDetectorInfo* info,
    struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT FABBY_SDK_RESULT FabbySDKInitLandmarksDetectorFromAsset(
    AAssetManager* asset_manager,
    const char* asset_path,
    bool try_to_run_on_gpu,
    struct FabbySDKLandmarksDetectorInfo* info,
    struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT void FabbySDKDestroyLandmarksDetector(
    struct FabbySDKLandmarksDetectorHandle handle);

// Detects landmarks. Memory pointed by @landmarks should be allocated to store
// at least FabbySDKLandmarksDetectorInfo::landmarks_count elements
//
// Note: it is not thread-safe with respect to the given handle, e.g. every
// handle may only be used in at most one FabbySDKDetectLandmarks call at
// the same time.
FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKDetectLandmarks(struct FabbySDKLandmarksDetectorHandle handle,
                        struct FabbySDKBGRAImage input_image,
                        const struct FabbySDKRotatedRect& faceRegion,
                        struct FabbySDKLandmark* landmarks,
                        struct FabbySDKControlFunctions* functions);
