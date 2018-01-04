//  Copyright © 2017 Fabby Inc. All rights reserved.
#pragma once

#include <android/asset_manager.h>

#include "fabby_sdk.h"
#include "fabby_sdk_image.h"

// Abstract handle to describe ImageStyle. It needs to be initialized once with
// FabbySDKInitImageStyle and destroyed with FabbySDKDestroyImageStyle when it
// is not needed anymore.
struct FabbySDKImageStyleHandle {
  void* data;
};

struct FabbySDKImageStyleInfo {
  FabbySDKImageStyleHandle handle;
};

// Loads given image style and initializes all internal structures.
FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKInitImageStyleFromFile(const char* path_to_image_style,
                               bool try_to_run_on_gpu,
                               struct FabbySDKImageStyleInfo* info,
                               struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKInitImageStyleFromAsset(AAssetManager* asset_manager,
                                const char* asset_path,
                                bool try_to_run_on_gpu,
                                struct FabbySDKImageStyleInfo* info,
                                struct FabbySDKControlFunctions* functions);

// Destroys internal structures needed for image style for the given handle.
FABBY_SDK_EXPORT void FabbySDKDestroyImageStyle(
    struct FabbySDKImageStyleHandle handle);

// Modifies input_image by transferring style described by a handle and
// outputs new image to output_data. Output image will have same dimensions as
// input image in BGRA format. It is possible to use the same pixel buffer as
// input and output. Channel 'A' will always be set to 255.
//
// Note: it is not thread-safe with respect to the given handle, e.g. every
// handle may only be used in at most one FabbySDKTransferImageStyle call at
// the same time.
FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKTransferImageStyle(struct FabbySDKImageStyleHandle handle,
                           struct FabbySDKBGRAImage input_image,
                           uint8_t* output_data,
                           struct FabbySDKControlFunctions* functions);
