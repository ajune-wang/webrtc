//  Copyright © 2017 Fabby Inc. All rights reserved.
#pragma once

#if defined(FABBY_SDK_EXPORT_DISABLED)
#define FABBY_SDK_EXPORT
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define FABBY_SDK_EXPORT extern "C" __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define FABBY_SDK_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define FABBY_SDK_EXPORT extern "C"
#endif
#endif

#include <stdbool.h>

typedef enum {
  FABBY_SDK_SUCCESS,
  FABBY_SDK_FAILURE,
  FABBY_SDK_CANCELED
} FABBY_SDK_RESULT;

typedef bool (*FabbySDKShouldCancelFn)(void* /* data you provide */);
typedef void (*FabbySDKProgressFn)(float /* [0.0, 1.0] value */,
                                   void* /* data you provide */);

// NOTE: when using and passing this structure to various APIs, make sure all
// functions are initialized with function or NULL, otherwise it's undefined
// behavior
struct FabbySDKControlFunctions {
  // function that will be used as a signal for model playback
  // cancellation if it returns true
  //
  // NOTE: make sure it's initialized with function or NULL, otherwise
  // it's undefined behavior
  FabbySDKShouldCancelFn cancel_fn;

  // any data, which is passed to "cancel_fn" when cancellation
  // is checked (you can use that as a tag)
  void* cancel_data;

  // function that will be invoked to each time model playback
  // progress is changed
  //
  // NOTE: make sure it's initialized with function or NULL, otherwise
  // it's undefined behavior
  FabbySDKProgressFn progress_fn;

  // any data, which is passed to "progress_fn" when
  // progress is changed (you can use that as a tag)
  void* progress_data;
};
