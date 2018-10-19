/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_FRAME_ANALYZER_LINEAR_LEAST_SQUARES_H_
#define RTC_TOOLS_FRAME_ANALYZER_LINEAR_LEAST_SQUARES_H_

#include <valarray>
#include <vector>
#include "absl/types/optional.h"

namespace webrtc {
namespace test {

// This class is used for finding a matrix b that roughly solves the equation:
// y = x * b. This is generally impossible to do exactly, so the problem is
// rephrased as finding the matrix b that minimizes the difference:
// |y - x * b|^2. Calling multiple AddObservations() is equivalent to
// concatenating the observation vectors and calling AddObservations() once. The
// reason for doing it incrementally is that we can't store the raw YUV values
// for a whole video file in memory at once. This class has a constant memory
// footprint, regardless how may times AddObservations() is called.
class IncrementalLinearLeastSquares {
 public:
  template <class T>
  using VectorMatrix = std::vector<std::vector<T>>;

  IncrementalLinearLeastSquares();
  ~IncrementalLinearLeastSquares();

  // Add a number of observations. The subvectors of x and y must have the same
  // length.
  void AddObservations(const VectorMatrix<uint8_t>& x,
                       const VectorMatrix<uint8_t>& y);

  void AddObservations2(const VectorMatrix<int16_t>& x,
                        const VectorMatrix<int16_t>& y);

  // Calculate and return the best linear solution, given the observations so
  // far.
  VectorMatrix<double> GetBestSolution() const;

 private:
  template <class T>
  using Matrix = std::valarray<std::valarray<T>>;

  // void AddObservations(const Matrix<int16_t>& x, const Matrix<int16_t>& y);

  // Running sum of x^T * x.
  absl::optional<Matrix<int64_t>> sum_xx;
  // Running sum of x^T * y.
  absl::optional<Matrix<int64_t>> sum_xy;
};

}  // namespace test
}  // namespace webrtc

#endif  // RTC_TOOLS_FRAME_ANALYZER_LINEAR_LEAST_SQUARES_H_
