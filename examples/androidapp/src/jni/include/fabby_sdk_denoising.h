//  Copyright © 2017 Fabby Inc. All rights reserved.
#pragma once

#include <android/asset_manager.h>

#include "fabby_sdk.h"
#include "fabby_sdk_image.h"

struct FabbySDKNoiseRemoverHandle {
  void* data;
};

struct FabbySDKNoiseRemoverInfo {
  FabbySDKNoiseRemoverHandle handle;
};

FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKInitNoiseRemoverFromFile(const char* model_path,
                                 bool try_to_run_on_gpu,
                                 struct FabbySDKNoiseRemoverInfo* info,
                                 struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKInitNoiseRemoverFromAsset(AAssetManager* asset_manager,
                                  const char* asset_path,
                                  bool try_to_run_on_gpu,
                                  struct FabbySDKNoiseRemoverInfo* info,
                                  struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT void FabbySDKDestroyNoiseRemover(
    struct FabbySDKNoiseRemoverHandle handle);

FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKRunNoiseRemover(struct FabbySDKNoiseRemoverHandle handle,
                        struct FabbySDKBGRAImage input_image,
                        uint8_t* output_data,
                        struct FabbySDKControlFunctions* functions);
