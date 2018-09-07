/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/linear_least_squares.h"

#include <numeric>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

template <class T>
using Matrix = std::valarray<std::valarray<T>>;

namespace {

template <typename R, typename T>
R DotProduct(const std::valarray<T>& a, const std::valarray<T>& b) {
  RTC_CHECK_EQ(a.size(), b.size());
  return std::inner_product(std::begin(a), std::end(a), std::begin(b), R(0));
}

// Calculates a^T * b.
template <typename R, typename T>
Matrix<R> MatrixMultiply(const Matrix<T>& a, const Matrix<T>& b) {
  Matrix<R> result(std::valarray<R>(b.size()), a.size());
  for (size_t i = 0; i < a.size(); ++i) {
    for (size_t j = 0; j < b.size(); ++j)
      result[i][j] = DotProduct<R>(a[i], b[j]);
  }

  return result;
}

template <typename T>
Matrix<T> Transpose(const Matrix<T>& matrix) {
  if (matrix.size() == 0)
    return Matrix<T>();
  const size_t rows = matrix.size();
  const size_t columns = matrix[0].size();
  Matrix<T> result(std::valarray<T>(rows), columns);

  for (size_t i = 0; i < rows; ++i) {
    for (size_t j = 0; j < columns; ++j)
      result[j][i] = matrix[i][j];
  }

  return result;
}

// Convert valarray from type T to type R.
template <typename R, typename T>
std::valarray<R> ConvertTo(const std::valarray<T>& v) {
  std::valarray<R> result(v.size());
  for (size_t i = 0; i < v.size(); ++i)
    result[i] = static_cast<R>(v[i]);
  return result;
}

// Convert valarray Matrix from type T to type R.
template <typename R, typename T>
Matrix<R> ConvertTo(const Matrix<T>& mat) {
  Matrix<R> result(mat.size());
  for (size_t i = 0; i < mat.size(); ++i)
    result[i] = ConvertTo<R>(mat[i]);
  return result;
}

// Convert from valarray Matrix back to the more conventional std::vector.
template <typename T>
std::vector<std::vector<T>> ToVectorMatrix(const Matrix<T>& m) {
  std::vector<std::vector<T>> result;
  for (const std::valarray<T>& v : m)
    result.emplace_back(std::begin(v), std::end(v));
  return result;
}

// Create a valarray Matrix from a conventional std::vector.
template <typename T>
Matrix<T> FromVectorMatrix(const std::vector<std::vector<T>>& mat) {
  Matrix<T> result(mat.size());
  for (size_t i = 0; i < mat.size(); ++i)
    result[i] = std::valarray<T>(mat[i].data(), mat[i].size());
  return result;
}

// Returns matrix_to_invert^-1 * right_hand_matrix.
Matrix<double> GaussianElimination(Matrix<double> matrix_to_invert,
                                   Matrix<double> right_hand_matrix) {
  RTC_CHECK_EQ(matrix_to_invert.size(), right_hand_matrix.size());
  const size_t n = right_hand_matrix.size();

  // Different from normal Gaussian elimination, we work on the columns instead
  // of the rows since that is how we efficiently store the data in the
  // matrices. This requires us to transpose the result before returning it.
  for (size_t i = 0; i < n; ++i) {
    // Swap columns to get the highest absolute value as pivot.
    int pivot = i;
    for (size_t column = i; column < n; ++column) {
      if (abs(matrix_to_invert[pivot][i]) < abs(matrix_to_invert[column][i]))
        pivot = column;
    }
    std::swap(matrix_to_invert[pivot], matrix_to_invert[i]);
    std::swap(right_hand_matrix[pivot], right_hand_matrix[i]);

    // Reduce the pivot to be 1.
    if (matrix_to_invert[i][i] == 0.0)
      continue;
    const double alpha = matrix_to_invert[i][i];
    matrix_to_invert[i] /= alpha;
    right_hand_matrix[i] /= alpha;

    // Eliminate the other entries in column |i|.
    for (size_t column = 0; column < n; ++column) {
      if (column == i)
        continue;
      const double alpha = matrix_to_invert[column][i];
      matrix_to_invert[column] -= alpha * matrix_to_invert[i];
      right_hand_matrix[column] -= alpha * right_hand_matrix[i];
    }
  }

  // Transpoes the result before returning it, explained in comment above.
  return Transpose(right_hand_matrix);
}

}  // namespace

IncrementalLinearLeastSquares::IncrementalLinearLeastSquares() = default;
IncrementalLinearLeastSquares::~IncrementalLinearLeastSquares() = default;

void IncrementalLinearLeastSquares::AddObservations(
    const std::vector<std::vector<uint8_t>>& x,
    const std::vector<std::vector<uint8_t>>& y) {
  // We will multiply the uint8_t values together, so we need to expand to a
  // type that can safely store those values, i.e. uint16_t.
  const Matrix<uint16_t> unpacked_x = ConvertTo<uint16_t>(FromVectorMatrix(x));
  const Matrix<uint16_t> unpacked_y = ConvertTo<uint16_t>(FromVectorMatrix(y));

  const Matrix<uint64_t> xx = MatrixMultiply<uint64_t>(unpacked_x, unpacked_x);
  const Matrix<uint64_t> xy = MatrixMultiply<uint64_t>(unpacked_x, unpacked_y);
  if (sum_xx && sum_xy) {
    *sum_xx += xx;
    *sum_xy += xy;
  } else {
    sum_xx = xx;
    sum_xy = xy;
  }
}

std::vector<std::vector<double>>
IncrementalLinearLeastSquares::GetBestSolution() const {
  RTC_CHECK(sum_xx && sum_xy) << "No observations have been added";
  return ToVectorMatrix(GaussianElimination(ConvertTo<double>(*sum_xx),
                                            ConvertTo<double>(*sum_xy)));
}

}  // namespace test
}  // namespace webrtc
