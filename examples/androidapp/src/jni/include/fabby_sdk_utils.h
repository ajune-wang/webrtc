//  Copyright © 2017 Fabby Inc. All rights reserved.
#pragma once

#include <cstring>
#include <string>

#include <android/asset_manager.h>

#include "common/execution_switch.h"
#include "common/progress.h"
#include "fabby_sdk.h"
#include "fabby_sdk_image.h"
#include "sdk/image.h"

namespace aim {
namespace sdk {

template <class TInnerHandle, class TOuterHandle>
TInnerHandle ToInnerHandle(TOuterHandle outer_handle) {
  return TInnerHandle{outer_handle.data};
}

inline BGRAImage ToInnerImage(FabbySDKBGRAImage outer_image) {
  return aim::sdk::BGRAImage{outer_image.width, outer_image.height, outer_image.data};
}

inline Texture ToInnerTexture(FabbySDKTexture externalTexture) {
  return aim::sdk::Texture{externalTexture.textureId, externalTexture.type, externalTexture.width,
                           externalTexture.height};
}

inline RotatedRect ToInnerRect(FabbySDKRotatedRect externalRect) {
  return RotatedRect{externalRect.x, externalRect.y, externalRect.width, externalRect.height, externalRect.angle};
}

std::tuple<std::shared_ptr<common::ExecutionSwitch>, std::unique_ptr<common::Progress>> MakeControlFunctions(
    struct FabbySDKControlFunctions* functions);

inline FABBY_SDK_RESULT ToOuterResult(common::FunctionOutcome inner_result) {
  switch (inner_result) {
    case common::FunctionOutcome::SUCCESS:
      return FABBY_SDK_SUCCESS;
    case common::FunctionOutcome::FAILURE:
      return FABBY_SDK_FAILURE;
    case common::FunctionOutcome::CANCELED:
      return FABBY_SDK_CANCELED;
  }
}

bool ReadDataFromAsset(AAssetManager* asset_manager, const char* asset_path, std::string* data);

bool ReadDataFromFile(const char* file_path, std::string* data);

}  // namespace sdk
}  // namespace aim
