//  Copyright © 2017 Fabby Inc. All rights reserved.

#pragma once

#include <android/asset_manager.h>

#include "fabby_sdk.h"
#include "fabby_sdk_landmarks.h"

// Abstract handle to describe FacePropertiesDetector model. It needs to be
// initialized once with FabbySDKInitFacePropertiesDetector and destroyed with
// FabbySDKDestroyFacePropertiesDetector when it is not needed anymore.
struct FabbySDKFacePropertiesDetectorHandle {
  void* data;
};

struct FabbySDKFacePropertiesDetectorInfo {
  struct FabbySDKFacePropertiesDetectorHandle handle;
};

// Face properties. Each field is in range [0, 1].
struct FabbySDKFaceProperties {
  float male;
};

FABBY_SDK_EXPORT FABBY_SDK_RESULT FabbySDKInitFacePropertiesDetectorFromFile(
    const char* model_path,
    bool try_to_run_on_gpu,
    struct FabbySDKFacePropertiesDetectorInfo* info,
    struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT FABBY_SDK_RESULT FabbySDKInitFacePropertiesDetectorFromAsset(
    AAssetManager* asset_manager,
    const char* asset_path,
    bool try_to_run_on_gpu,
    struct FabbySDKFacePropertiesDetectorInfo* info,
    struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT void FabbySDKDestroyFacePropertiesDetector(
    struct FabbySDKFacePropertiesDetectorHandle handle);

// Detects landmarks. Memory pointed by @landmarks should be allocated to store
// at least FabbySDKLandmarksDetectorInfo::landmarks_count elements
//
// Note: it is not thread-safe with respect to the given handle, e.g. every
// handle may only be used in at most one FabbySDKDetectLandmarks call at
// the same time.
FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKDetectFaceProperties(struct FabbySDKFacePropertiesDetectorHandle handle,
                             struct FabbySDKBGRAImage input_image,
                             struct FabbySDKLandmark* landmarks,
                             struct FabbySDKFaceProperties* result,
                             struct FabbySDKControlFunctions* functions);
