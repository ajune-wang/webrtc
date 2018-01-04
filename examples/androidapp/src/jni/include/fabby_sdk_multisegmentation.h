//  Copyright © 2017 Fabby Inc. All rights reserved.
#pragma once

#include <android/asset_manager.h>

#include "fabby_sdk.h"
#include "fabby_sdk_image.h"
#include "fabby_sdk_landmarks.h"

// Abstract handle to describe MultiSegmenter model. It needs to be initialized
// once with FabbySDKInitMultiSegmenter and destroyed with
// FabbySDKDestroyMultiSegmenter when it is not needed anymore.
struct FabbySDKMultiSegmenterHandle {
  void* data;
};

struct FabbySDKMultiSegmenterInfo {
  // Both sides need to be divisible by this number to ensure segmenter is
  // working correctly.
  int side_should_be_divisible_by;

  // MultiSegmentation model was trained for this size of an image,
  // therefore, it is recommended to scale longest side of an input image
  // to this side for better performance.
  int recommended_long_side_size;

  int number_of_classes;

  FabbySDKMultiSegmenterHandle handle;
};

// Output of multisegmentation. All layer pointers point into memory passed by a
// user as @mask parameter of FabbySDKMultiSegmentObject
struct FabbySDKMultiSegmenterResult {
  float* background_layer;
  float* hair_layer;
  float* beard_layer;
  float* face_skin_layer;
  float* face_other_layer;

  float* right_eye_pupil_layer;
  float* right_eye_iris_layer;
  float* right_eye_sclera_layer;

  float* left_eye_pupil_layer;
  float* left_eye_iris_layer;
  float* left_eye_sclera_layer;

  float* lips_layer;
  float* tongue_layer;
  float* teeth_layer;

  // Bounding boxes for facial features. Bounding boxes are not guaranteed to be
  // tight.
  struct FabbySDKRect face_bound;
  struct FabbySDKRect right_eye_bound;
  struct FabbySDKRect left_eye_bound;
  struct FabbySDKRect mouth_bound;

  // Set to true if multisegmentation result is wildly inconsistent with
  // landmarks.
  bool inconsistent_with_landmarks;
};

// Loads given segmenter model and initializes all internal structures.
FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKInitMultiSegmenterFromFile(const char* path_to_segmenter_model,
                                   bool try_to_run_on_gpu,
                                   struct FabbySDKMultiSegmenterInfo* info,
                                   struct FabbySDKControlFunctions* functions);

FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKInitMultiSegmenterFromAsset(AAssetManager* asset_manager,
                                    const char* asset_path,
                                    bool try_to_run_on_gpu,
                                    struct FabbySDKMultiSegmenterInfo* info,
                                    struct FabbySDKControlFunctions* functions);

// Destroys internal structures needed for segmenter for the given handle.
FABBY_SDK_EXPORT void FabbySDKDestroyMultiSegmenter(
    struct FabbySDKMultiSegmenterHandle handle);

// Masks should be array of size width*height*numberOfClasses
// @landmarks can be null. in this case right_eye_*, left_eye_* lips, tongue and
// teeth layers will be filled with zeroes.
FABBY_SDK_EXPORT FABBY_SDK_RESULT
FabbySDKMultiSegmentObject(struct FabbySDKMultiSegmenterHandle handle,
                           struct FabbySDKBGRAImage input_image,
                           const FabbySDKLandmark* landmarks,
                           float* mask,
                           struct FabbySDKMultiSegmenterResult* result,
                           struct FabbySDKControlFunctions* functions);
