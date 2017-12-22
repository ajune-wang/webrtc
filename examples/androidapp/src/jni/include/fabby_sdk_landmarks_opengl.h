//  Copyright © 2017 Fabby Inc. All rights reserved.

#pragma once

#include <android/asset_manager.h>

#include "fabby_sdk.h"
#include "fabby_sdk_image.h"
#include "fabby_sdk_landmarks.h"

FABBY_SDK_EXPORT FABBY_SDK_RESULT FabbySDKInitOpenGlLandmarksDetectorFromFile(
    const char* model_path,
    struct FabbySDKLandmarksDetectorInfo* info,
    struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT FABBY_SDK_RESULT FabbySDKInitOpenGlLandmarksDetectorFromAsset(
    AAssetManager* asset_manager,
    const char* asset_path,
    struct FabbySDKLandmarksDetectorInfo* info,
    struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT void FabbySDKDestroyOpenGlLandmarksDetector(
    struct FabbySDKLandmarksDetectorHandle handle);

FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKDetectOpenGlLandmarks(struct FabbySDKLandmarksDetectorHandle handle,
                              const struct FabbySDKTexture& texture,
                              const struct FabbySDKRotatedRect& faceRegion,
                              int cameraAngle,
                              int flip,
                              struct FabbySDKLandmark* landmarks,
                              struct FabbySDKControlFunctions* functions);
