/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <numeric>

#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/downsample.h"
#include "common_audio/rnn_vad/features_extraction.h"
#include "common_audio/rnn_vad/test_utils.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

using rnn_vad::ComputeBandEnergies;
using rnn_vad::ComputeHalfVorbisWindow;
using rnn_vad::ComputeForwardFft;
using rnn_vad::ComputeOpusBandsIndexes;
using rnn_vad::Decimate48k24k;
// using rnn_vad::kHalfFrameSize;
// using rnn_vad::kFrameSize;
using rnn_vad::kInputFrameSize;
using rnn_vad::kFeatureVectorSize;
using rnn_vad::kNumOpusBands;
using rnn_vad::ProcessHighPassFilter;
using rnn_vad::RnnVadFeaturesExtractor;

namespace {

constexpr size_t kSampleRate48k = 48000;
constexpr size_t k48k10msFrameSize = 480;
constexpr size_t k48k20msFrameSize = 960;
constexpr size_t k48k20msFftSize = 1024;
constexpr size_t k48k20msNumFftCoeffs = k48k20msFftSize / 2 + 1;

constexpr float kHighPassFilterMem[2] = {-45.993244171142578125f,
                                         45.930263519287109375f};

constexpr std::array<float, k48k10msFrameSize> kExpectedHpFilterOutput = {
    -18.993244f,  -18.980064f,  -18.966747f,  -16.953293f,  -14.947723f,
    -15.950027f,  -15.948166f,  -14.946152f,  -12.947998f,  -12.957706f,
    -13.967247f,  -14.97261f,   -11.973804f,  -8.986874f,   -7.011799f,
    -7.044552f,   -7.077103f,   -5.109455f,   -6.149628f,   -9.185581f,
    -9.209297f,   -7.232826f,   -6.264191f,   -7.299366f,   -8.330326f,
    -7.357079f,   -5.38765f,    -5.426044f,   -5.46423f,    -6.502209f,
    -4.535973f,   -4.577557f,   -5.618927f,   -7.656075f,   -6.684998f,
    -5.717739f,   -3.754295f,   -4.798668f,   -3.838818f,   -1.882771f,
    -1.934528f,   -4.986057f,   -8.025333f,   -12.052372f,  -11.063183f,
    -8.077843f,   -7.104366f,   -10.134712f,  -12.152836f,  -11.162766f,
    -10.176544f,  -9.194168f,   -13.21563f,   -13.220875f,  -11.225967f,
    -8.238926f,   -11.263748f,  -13.27636f,   -13.280788f,  -12.285065f,
    -11.293201f,  -12.305195f,  -13.313019f,  -14.316681f,  -13.316185f,
    -14.319557f,  -16.318771f,  -18.309826f,  -18.292736f,  -19.275532f,
    -20.254204f,  -19.22876f,   -18.207226f,  -20.189598f,  -21.163841f,
    -20.133976f,  -18.108028f,  -17.090004f,  -16.075882f,  -17.065655f,
    -16.0513f,    -13.040844f,  -10.042301f,  -10.055653f,  -11.068851f,
    -11.077888f,  -7.086781f,   -4.111565f,   -3.148209f,   -4.188675f,
    -4.224937f,   -3.261013f,   -3.300911f,   -4.340614f,   -4.376114f,
    0.588573f,    1.533394f,    -1.525581f,   -2.572304f,   -3.614815f,
    -2.653118f,   -3.69524f,    -4.733158f,   -5.76688f,    -5.79641f,
    -5.825764f,   -5.854942f,   -6.883945f,   -6.908764f,   -5.933414f,
    -2.961906f,   -3.002254f,   -4.042412f,   -5.078369f,   -5.110134f,
    -4.14172f,    -3.177135f,   -4.216377f,   -2.251419f,   -0.294296f,
    -0.345001f,   0.6045f,      2.550198f,    7.488087f,    10.406151f,
    11.312439f,   11.214993f,   10.117825f,   11.024944f,   11.928326f,
    13.827976f,   15.719891f,   18.60408f,    19.476551f,   18.345337f,
    18.218468f,   19.091927f,   20.9617f,     22.823784f,   20.678192f,
    17.540977f,   15.416138f,   17.299644f,   18.175446f,   16.047562f,
    13.928028f,   13.816833f,   15.705948f,   15.587349f,   14.46907f,
    10.355122f,   10.257526f,   11.160217f,   11.059189f,   6.958454f,
    2.87405f,     1.805954f,    1.742115f,    -0.321484f,   -4.376823f,
    -8.415897f,   -8.438728f,   -8.461384f,   -12.483864f,  -19.490128f,
    -26.46817f,   -31.418037f,  -31.347794f,  -33.277519f,  -37.199196f,
    -43.104816f,  -46.986385f,  -48.851959f,  -52.709583f,  -55.55125f,
    -60.380997f,  -65.190826f,  -67.980766f,  -71.758865f,  -73.521133f,
    -75.27562f,   -74.022339f,  -74.773331f,  -76.520569f,  -77.260056f,
    -79.995819f,  -81.719841f,  -82.436157f,  -80.148788f,  -78.869774f,
    -80.595093f,  -83.312706f,  -84.018616f,  -80.720871f,  -77.435516f,
    -76.162537f,  -77.89389f,   -78.617538f,  -77.337509f,  -74.061829f,
    -72.798508f,  -72.53952f,   -71.280846f,  -69.026489f,  -63.780464f,
    -59.55479f,   -57.345428f,  -54.144333f,  -50.955502f,  -45.778919f,
    -42.622589f,  -38.478462f,  -32.350529f,  -27.246788f,  -22.163193f,
    -19.099709f,  -15.04829f,   -9.012928f,   -4.001617f,   0.989685f,
    3.961014f,    4.920418f,    7.875935f,    8.81955f,     9.759304f,
    11.695202f,   13.623238f,   14.543423f,   11.459782f,   7.388363f,
    5.333153f,    5.28611f,     6.239201f,    3.188416f,    -0.850201f,
    -1.872654f,   1.109001f,    4.078716f,    4.036514f,    2.994442f,
    5.956509f,    10.90667f,    15.836922f,   18.747292f,   22.645832f,
    26.528553f,   30.395481f,   33.246639f,   36.086063f,   41.913773f,
    46.717754f,   46.502056f,   46.286758f,   49.071861f,   54.845333f,
    59.595165f,   62.325405f,   63.044102f,   65.759293f,   71.462967f,
    78.143112f,   80.795761f,   82.436996f,   84.070847f,   88.697327f,
    92.304413f,   94.896149f,   95.47657f,    98.053719f,   101.619583f,
    105.170174f,  103.705513f,  102.245682f,  101.790672f,  103.336472f,
    102.875061f,  98.414474f,   92.970749f,   91.547867f,   91.129761f,
    90.712418f,   87.295837f,   83.892059f,   80.501068f,   77.122833f,
    72.757339f,   68.408585f,   64.076546f,   59.7612f,     55.462521f,
    52.180481f,   48.911053f,   45.654213f,   41.409946f,   38.182243f,
    31.967072f,   26.776443f,   22.606312f,   20.452637f,   17.307369f,
    15.174511f,   13.050034f,   12.933926f,   11.818153f,   12.706726f,
    14.591618f,   13.468822f,   11.350384f,   8.240307f,    6.14259f,
    7.053204f,    9.960106f,    8.855282f,    6.754787f,    3.662624f,
    2.582794f,    3.507259f,    2.42799f,     2.353016f,    0.27832f,
    -0.788082f,   -0.850212f,   -1.912083f,   0.030312f,    1.964935f,
    2.8918f,      1.814926f,    -0.25766f,    -3.321953f,   -3.373955f,
    -3.425713f,   -1.47723f,    -2.536526f,   -3.59156f,    -3.642342f,
    -3.692883f,   -2.743183f,   -2.797253f,   -4.851078f,   -7.896641f,
    -8.929943f,   -10.959023f,  -9.979877f,   -10.004547f,  -8.029022f,
    -9.061317f,   -10.089394f,  -11.113258f,  -12.132919f,  -14.148384f,
    -15.155647f,  -19.15873f,   -21.145607f,  -22.124329f,  -25.098915f,
    -27.061356f,  -30.015678f,  -33.957886f,  -37.883987f,  -42.794006f,
    -45.683956f,  -46.561893f,  -44.435856f,  -44.317883f,  -46.19994f,
    -46.074009f,  -45.948124f,  -42.822285f,  -42.708523f,  -46.594791f,
    -50.46505f,   -54.319324f,  -58.157642f,  -60.980026f,  -61.790516f,
    -64.597153f,  -67.391922f,  -69.174835f,  -71.949928f,  -74.713203f,
    -77.464676f,  -80.204376f,  -81.93232f,   -83.652542f,  -86.365051f,
    -86.065849f,  -85.766991f,  -86.468475f,  -87.16629f,   -89.860443f,
    -91.542923f,  -92.217758f,  -91.888977f,  -93.560593f,  -95.224594f,
    -95.880997f,  -96.533813f,  -96.18306f,   -95.832748f,  -95.48288f,
    -95.133461f,  -95.784492f,  -95.431969f,  -96.07991f,   -98.724304f,
    -99.357132f,  -99.986435f,  -98.612228f,  -95.242538f,  -96.885376f,
    -98.520676f,  -100.148453f, -98.768723f,  -97.393524f,  -99.022858f,
    -104.644684f, -109.242981f, -110.821793f, -109.393188f, -111.969208f,
    -113.533806f, -114.091019f, -113.644867f, -112.199371f, -112.758545f,
    -112.314362f, -108.870834f, -104.439995f, -101.025841f, -98.624336f,
    -97.231453f,  -93.84317f,   -89.467506f,  -87.108459f,  -84.75798f,
    -82.416061f,  -76.082687f,  -72.773895f,  -69.477615f,  -67.193832f,
    -65.918518f,  -64.647644f,  -62.38121f,   -60.123222f,  -57.873669f,
    -52.632538f,  -48.411846f,  -44.207554f,  -40.019642f,  -35.848083f,
    -30.692856f,  -26.557945f,  -23.439312f,  -21.33292f,   -18.234741f,
    -15.148773f,  -14.075001f,  -12.005386f,  -9.943932f,   -8.890625f,
    -6.841442f,   -5.800388f,   -2.763443f,   -0.738617f,   0.278114f,
    -1.709221f,   -1.688587f,   -2.668018f,   -1.643505f,   0.376926f,
    3.389275f,    6.389538f,    5.377735f,    6.369926f,    6.358086f,
    6.346226f,    9.334351f,    10.310432f,   10.282509f,   9.254593f};

constexpr std::array<float, k48k20msNumFftCoeffs> kExpectedFftCoeffsReal = {
    -4.6236f, 4.7342f,  -4.6857f, -0.6570f, 10.1290f, -9.8816f, 0.0908f,
    2.7273f,  -0.2640f, 3.4679f,  -5.0903f, 1.8102f,  -0.4294f, 0.1618f,
    -0.0786f, 1.3723f,  -0.8347f, -1.1301f, 0.4822f,  0.9220f,  -0.4847f,
    -0.1167f, -0.3907f, 0.8645f,  -0.2896f, -0.1069f, -0.1973f, 0.2208f,
    0.1127f,  -0.2377f, 0.0730f,  -0.1385f, 0.1004f,  0.2941f,  -0.1617f,
    -0.2737f, 0.2511f,  -0.1248f, 0.2642f,  -0.2516f, 0.0055f,  0.0227f,
    0.1311f,  -0.1168f, -0.0723f, 0.0926f,  -0.0177f, 0.0878f,  -0.0879f,
    -0.0902f, 0.2073f,  -0.1131f, -0.0366f, 0.0539f,  0.0153f,  -0.0303f,
    0.0129f,  -0.0048f, -0.0111f, 0.0405f,  -0.0700f, 0.0753f,  -0.0649f,
    0.0294f,  0.0227f,  -0.0389f, 0.0017f,  0.0494f,  -0.0434f, 0.0102f,
    0.0036f,  -0.0213f, -0.0085f, 0.0533f,  -0.0455f, 0.0116f,  0.0186f,
    0.0042f,  -0.0136f, -0.0395f, 0.0660f,  -0.0621f, 0.0355f,  -0.0214f,
    0.0727f,  -0.0640f, 0.0241f,  -0.0189f, -0.0166f, 0.0160f,  0.0071f,
    0.0219f,  -0.0121f, -0.0650f, 0.0362f,  0.0954f,  -0.1197f, 0.0281f,
    0.0387f,  -0.0566f, 0.0233f,  0.0265f,  -0.0236f, 0.0064f,  -0.0142f,
    0.0147f,  -0.0139f, 0.0128f,  0.0061f,  0.0020f,  0.0014f,  -0.0411f,
    0.0481f,  -0.0300f, 0.0120f,  0.0059f,  -0.0039f, 0.0023f,  -0.0108f,
    0.0174f,  -0.0099f, -0.0119f, 0.0196f,  -0.0122f, -0.0223f, 0.0272f,
    -0.0007f, 0.0094f,  -0.0113f, -0.0222f, 0.0256f,  0.0113f,  -0.0146f,
    0.0112f,  -0.0134f, 0.0023f,  0.0128f,  -0.0264f, -0.0062f, 0.0405f,
    -0.0344f, 0.0445f,  -0.0161f, -0.0206f, -0.0049f, 0.0080f,  0.0008f,
    -0.0232f, 0.0555f,  -0.0351f, 0.0031f,  0.0077f,  0.0012f,  -0.0131f,
    0.0139f,  -0.0070f, -0.0173f, 0.0057f,  0.0325f,  -0.0023f, -0.0489f,
    0.0802f,  -0.1017f, 0.0374f,  0.0322f,  0.0441f,  -0.0993f, 0.0269f,
    0.0090f,  0.0141f,  -0.0283f, 0.0113f,  0.0019f,  0.0130f,  -0.0147f,
    0.0121f,  -0.0143f, 0.0069f,  0.0059f,  -0.0047f, -0.0207f, 0.0352f,
    -0.0229f, 0.0133f,  -0.0438f, 0.0743f,  -0.0412f, -0.0013f, -0.0003f,
    0.0248f,  -0.0183f, 0.0321f,  -0.0659f, 0.0069f,  0.0472f,  -0.0226f,
    0.0061f,  0.0133f,  -0.0386f, -0.0012f, -0.0065f, 0.0628f,  -0.0307f,
    0.0069f,  -0.0116f, -0.0139f, 0.0349f,  -0.0415f, 0.0412f,  0.0061f,
    -0.0490f, 0.0182f,  0.0052f,  0.0066f,  -0.0217f, 0.0054f,  0.0497f,
    -0.0349f, 0.0107f,  -0.0894f, 0.0694f,  0.0246f,  -0.0289f, 0.0121f,
    -0.0209f, 0.0312f,  -0.0075f, -0.0056f, 0.0025f,  -0.0029f, 0.0042f,
    -0.0138f, 0.0091f,  -0.0015f, 0.0026f,  -0.0006f, -0.0081f, 0.0083f,
    0.0021f,  0.0010f,  -0.0134f, 0.0183f,  -0.0212f, 0.0280f,  -0.0182f,
    -0.0042f, 0.0061f,  0.0049f,  0.0001f,  -0.0157f, 0.0304f,  -0.0196f,
    -0.0025f, -0.0063f, 0.0115f,  -0.0032f, -0.0023f, 0.0035f,  -0.0074f,
    0.0151f,  -0.0061f, 0.0026f,  -0.0050f, -0.0041f, 0.0024f,  0.0019f,
    -0.0034f, 0.0048f,  0.0043f,  -0.0085f, 0.0014f,  0.0071f,  -0.0037f,
    -0.0018f, -0.0076f, 0.0126f,  -0.0023f, -0.0047f, -0.0024f, 0.0028f,
    0.0053f,  -0.0096f, 0.0134f,  -0.0082f, -0.0041f, 0.0086f,  -0.0094f,
    0.0086f,  -0.0018f, 0.0009f,  0.0033f,  -0.0114f, 0.0071f,  -0.0009f,
    0.0028f,  -0.0071f, 0.0105f,  -0.0144f, 0.0079f,  -0.0010f, 0.0032f,
    0.0023f,  -0.0120f, 0.0208f,  -0.0142f, 0.0037f,  -0.0083f, 0.0093f,
    -0.0089f, 0.0023f,  0.0042f,  0.0009f,  -0.0044f, 0.0061f,  -0.0076f,
    0.0004f,  0.0048f,  0.0038f,  -0.0091f, 0.0092f,  -0.0071f, -0.0014f,
    0.0045f,  0.0004f,  0.0013f,  -0.0092f, 0.0133f,  -0.0159f, 0.0112f,
    0.0047f,  -0.0181f, 0.0162f,  -0.0054f, -0.0058f, 0.0084f,  -0.0040f,
    0.0026f,  0.0044f,  -0.0115f, 0.0058f,  0.0025f,  -0.0073f, 0.0064f,
    0.0014f,  -0.0086f, 0.0141f,  -0.0047f, -0.0063f, -0.0000f, 0.0065f,
    -0.0083f, 0.0121f,  -0.0187f, 0.0168f,  -0.0082f, -0.0001f, 0.0053f,
    -0.0001f, -0.0030f, 0.0036f,  -0.0092f, 0.0101f,  -0.0095f, 0.0078f,
    -0.0062f, 0.0080f,  -0.0074f, 0.0099f,  -0.0112f, 0.0057f,  -0.0014f,
    -0.0030f, 0.0060f,  -0.0026f, 0.0002f,  -0.0019f, 0.0029f,  -0.0020f,
    -0.0003f, -0.0042f, 0.0142f,  -0.0154f, 0.0047f,  0.0081f,  -0.0070f,
    -0.0065f, 0.0012f,  0.0166f,  -0.0181f, 0.0128f,  -0.0068f, -0.0080f,
    0.0176f,  -0.0086f, -0.0005f, -0.0033f, 0.0067f,  -0.0085f, 0.0086f,
    -0.0017f, -0.0037f, 0.0015f,  0.0124f,  -0.0193f, 0.0058f,  -0.0028f,
    0.0056f,  0.0075f,  -0.0132f, 0.0032f,  0.0056f,  -0.0059f, 0.0050f,
    -0.0023f, -0.0065f, 0.0070f,  0.0014f,  -0.0100f, 0.0101f,  0.0039f,
    -0.0106f, 0.0053f,  -0.0023f, -0.0036f, 0.0086f,  -0.0027f, -0.0023f,
    0.0019f,  -0.0019f, -0.0017f, 0.0068f,  -0.0084f, 0.0077f,  0.0007f,
    -0.0112f, 0.0138f,  -0.0099f, 0.0080f,  -0.0114f, 0.0012f,  0.0054f,
    0.0055f,  -0.0020f, -0.0030f, -0.0002f, -0.0109f, 0.0172f,  -0.0065f,
    0.0006f,  0.0046f,  -0.0102f, 0.0049f,  0.0022f,  -0.0001f, -0.0040f,
    0.0076f,  -0.0131f, 0.0067f,  0.0049f,  -0.0091f, 0.0041f,  0.0046f,
    -0.0065f, 0.0023f,  0.0018f,  0.0021f,  -0.0105f, 0.0122f,  0.0011f,
    -0.0121f, 0.0102f,  -0.0068f, 0.0010f,  -0.0021f, 0.0059f,  0.0015f,
    -0.0052f, 0.0010f,  -0.0009f, 0.0008f,  -0.0006f, -0.0024f, 0.0056f,
    -0.0002f, -0.0016f, 0.0009f,  0.0022f,  -0.0080f, 0.0078f,  -0.0011f,
    -0.0019f, -0.0002f, 0.0047f,  -0.0096f, 0.0027f,  0.0049f,  -0.0043f,
    0.0043f,  -0.0024f, -0.0033f, 0.0014f,  0.0023f,  0.0005f,  -0.0018f,
    -0.0014f, 0.0127f,  -0.0176f, 0.0079f,  -0.0080f, 0.0153f,  -0.0118f,
    -0.0031f, 0.0135f};

constexpr std::array<float, k48k20msNumFftCoeffs> kExpectedFftCoeffsImag = {
    0.0000f,  3.8850f,  -5.4477f, 7.3073f,  -5.2311f, -1.7965f, 3.5078f,
    1.1172f,  -0.0988f, -4.2159f, 2.5748f,  0.1299f,  -0.2481f, 0.0342f,
    0.7023f,  -0.2496f, -1.6821f, 1.0860f,  0.8234f,  -0.4348f, -0.4300f,
    -0.2231f, 1.1850f,  -0.9472f, 0.2060f,  -0.0887f, 0.0888f,  0.0310f,
    -0.0459f, -0.0441f, 0.0349f,  -0.0424f, 0.4004f,  -0.3532f, -0.1769f,
    0.1920f,  0.0551f,  0.0835f,  -0.2208f, 0.0508f,  0.0117f,  0.0788f,
    -0.0450f, -0.0940f, 0.1137f,  -0.0100f, 0.0228f,  -0.0221f, -0.2140f,
    0.4553f,  -0.3396f, 0.0171f,  0.1076f,  -0.0332f, -0.0110f, 0.0263f,
    -0.0621f, 0.0192f,  0.0496f,  -0.0225f, -0.0411f, 0.0472f,  -0.0343f,
    0.0369f,  -0.0040f, -0.0747f, 0.1207f,  -0.0450f, -0.0759f, 0.0859f,
    -0.0352f, -0.0049f, 0.0359f,  -0.0440f, 0.0534f,  -0.0251f, -0.0157f,
    0.0077f,  -0.0004f, 0.0173f,  -0.0731f, 0.1061f,  -0.0549f, 0.0473f,
    -0.0203f, -0.0901f, 0.0891f,  -0.0360f, 0.0131f,  0.0288f,  -0.0415f,
    0.0400f,  -0.0591f, 0.0245f,  0.0355f,  -0.0032f, -0.0911f, 0.1291f,
    -0.0815f, 0.0021f,  0.0441f,  -0.0113f, -0.0438f, 0.0485f,  -0.0203f,
    -0.0061f, 0.0204f,  0.0012f,  -0.0166f, 0.0114f,  -0.0206f, 0.0005f,
    0.0221f,  0.0009f,  -0.0084f, -0.0083f, 0.0071f,  0.0059f,  -0.0260f,
    0.0428f,  -0.0471f, 0.0351f,  -0.0352f, 0.0267f,  0.0107f,  -0.0328f,
    0.0438f,  -0.0432f, 0.0456f,  -0.0544f, 0.0624f,  -0.0437f, 0.0089f,
    -0.0048f, 0.0070f,  0.0127f,  -0.0411f, 0.0261f,  -0.0046f, 0.0265f,
    -0.0056f, 0.0002f,  -0.0612f, 0.0326f,  0.0096f,  0.0181f,  -0.0222f,
    0.0110f,  0.0093f,  -0.0259f, 0.0178f,  -0.0079f, 0.0107f,  -0.0166f,
    0.0039f,  -0.0041f, 0.0099f,  0.0163f,  0.0026f,  -0.0525f, 0.0312f,
    0.0082f,  -0.0502f, 0.1171f,  -0.0685f, -0.0545f, 0.0345f,  0.0372f,
    -0.0307f, 0.0054f,  0.0033f,  -0.0164f, 0.0377f,  -0.0116f, -0.0318f,
    0.0291f,  -0.0106f, 0.0135f,  -0.0205f, -0.0110f, 0.0466f,  -0.0268f,
    -0.0192f, 0.0327f,  -0.0115f, 0.0080f,  0.0051f,  -0.0610f, 0.0783f,
    -0.0132f, -0.0141f, -0.0546f, 0.0525f,  0.0008f,  0.0166f,  -0.0088f,
    -0.0503f, 0.0462f,  -0.0128f, -0.0252f, 0.0972f,  -0.0484f, -0.0482f,
    0.0343f,  -0.0059f, 0.0016f,  -0.0041f, 0.0179f,  0.0065f,  -0.0435f,
    0.0061f,  0.0047f,  0.0323f,  -0.0082f, -0.0277f, 0.0203f,  0.0270f,
    -0.0572f, -0.0307f, 0.0682f,  0.0246f,  -0.0408f, 0.0010f,  0.0155f,
    -0.0066f, 0.0097f,  -0.0257f, 0.0171f,  -0.0130f, 0.0098f,  0.0018f,
    -0.0192f, 0.0152f,  0.0027f,  0.0057f,  -0.0148f, -0.0071f, 0.0345f,
    -0.0270f, 0.0103f,  -0.0130f, 0.0088f,  0.0072f,  -0.0085f, -0.0058f,
    0.0137f,  -0.0031f, -0.0133f, 0.0149f,  -0.0005f, -0.0088f, 0.0037f,
    -0.0151f, 0.0194f,  -0.0029f, -0.0007f, -0.0033f, 0.0120f,  -0.0146f,
    0.0114f,  -0.0068f, 0.0005f,  -0.0052f, 0.0045f,  -0.0041f, 0.0160f,
    -0.0196f, 0.0181f,  -0.0108f, 0.0023f,  -0.0040f, 0.0063f,  -0.0055f,
    -0.0000f, 0.0053f,  -0.0082f, 0.0081f,  -0.0096f, 0.0122f,  -0.0045f,
    -0.0069f, 0.0130f,  -0.0103f, 0.0046f,  -0.0024f, 0.0005f,  0.0038f,
    -0.0048f, 0.0051f,  -0.0046f, 0.0001f,  -0.0006f, 0.0008f,  -0.0022f,
    0.0081f,  -0.0038f, -0.0108f, 0.0080f,  0.0076f,  0.0015f,  -0.0101f,
    0.0031f,  -0.0051f, 0.0128f,  -0.0136f, 0.0035f,  -0.0009f, -0.0029f,
    0.0084f,  -0.0010f, -0.0014f, 0.0024f,  -0.0031f, -0.0032f, -0.0013f,
    0.0127f,  -0.0045f, -0.0067f, 0.0057f,  -0.0024f, -0.0016f, -0.0050f,
    0.0141f,  -0.0107f, 0.0105f,  -0.0175f, 0.0156f,  -0.0096f, 0.0052f,
    -0.0013f, -0.0005f, 0.0049f,  -0.0115f, 0.0144f,  -0.0113f, 0.0107f,
    -0.0079f, -0.0004f, -0.0000f, 0.0050f,  0.0008f,  -0.0076f, 0.0066f,
    -0.0020f, -0.0023f, 0.0071f,  -0.0093f, 0.0051f,  0.0000f,  -0.0076f,
    0.0076f,  -0.0040f, 0.0039f,  -0.0005f, 0.0044f,  -0.0095f, 0.0138f,
    -0.0184f, 0.0111f,  -0.0013f, -0.0040f, 0.0061f,  -0.0044f, 0.0045f,
    -0.0047f, 0.0062f,  -0.0075f, 0.0017f,  0.0049f,  -0.0028f, -0.0045f,
    0.0027f,  0.0027f,  0.0022f,  -0.0055f, -0.0036f, 0.0093f,  -0.0046f,
    -0.0019f, 0.0051f,  -0.0027f, -0.0005f, 0.0010f,  -0.0015f, -0.0040f,
    0.0066f,  0.0042f,  -0.0082f, -0.0008f, 0.0099f,  -0.0121f, 0.0089f,
    -0.0054f, 0.0022f,  -0.0036f, 0.0066f,  -0.0010f, -0.0056f, 0.0039f,
    0.0032f,  -0.0065f, 0.0100f,  -0.0139f, 0.0011f,  0.0052f,  0.0039f,
    0.0047f,  -0.0110f, -0.0040f, 0.0099f,  -0.0032f, 0.0061f,  -0.0123f,
    0.0008f,  0.0063f,  -0.0005f, 0.0014f,  0.0043f,  -0.0124f, 0.0074f,
    -0.0012f, -0.0012f, -0.0009f, 0.0039f,  0.0012f,  -0.0052f, -0.0017f,
    0.0033f,  0.0034f,  -0.0036f, 0.0049f,  -0.0060f, -0.0008f, -0.0012f,
    0.0111f,  -0.0121f, 0.0033f,  -0.0062f, 0.0158f,  -0.0118f, 0.0090f,
    -0.0015f, -0.0114f, 0.0089f,  -0.0027f, -0.0001f, 0.0053f,  -0.0006f,
    -0.0081f, 0.0025f,  0.0072f,  -0.0075f, 0.0033f,  -0.0024f, 0.0023f,
    -0.0058f, 0.0084f,  -0.0040f, -0.0031f, 0.0037f,  0.0052f,  -0.0065f,
    0.0011f,  0.0027f,  -0.0035f, 0.0038f,  -0.0065f, 0.0085f,  -0.0132f,
    0.0161f,  -0.0109f, -0.0009f, 0.0029f,  0.0084f,  -0.0111f, 0.0023f,
    0.0061f,  -0.0067f, -0.0022f, 0.0080f,  -0.0048f, 0.0050f,  -0.0027f,
    -0.0026f, 0.0054f,  -0.0005f, -0.0091f, 0.0064f,  0.0023f,  -0.0052f,
    0.0039f,  -0.0036f, 0.0001f,  0.0007f,  0.0076f,  -0.0148f, 0.0111f,
    -0.0003f, -0.0040f, -0.0024f, 0.0105f,  -0.0097f, 0.0035f,  0.0052f,
    -0.0046f, -0.0044f, -0.0029f, 0.0099f,  0.0004f,  -0.0032f, -0.0040f,
    0.0072f,  0.0000f};

constexpr std::array<float, kNumOpusBands> kExpectedBandEnergies = {
    209.166901f, 314.954376f, 77.851593f, 20.035751f, 8.152700f, 3.612466f,
    1.134943f,   0.256439f,   0.603402f,  0.313802f,  0.395649f, 0.037905f,
    0.060894f,   0.093480f,   0.062729f,  0.029794f,  0.048662f, 0.084502f,
    0.087994f,   0.023445f,   0.011277f, 0.010227f};

void CheckFftResult(const float expected_tot_energy,
                    rtc::ArrayView<const float> expected_real,
                    rtc::ArrayView<const float> expected_imag,
                    rtc::ArrayView<const std::complex<float>> computed) {
  ASSERT_EQ(expected_real.size(), expected_imag.size());
  EXPECT_EQ(computed.size(), expected_real.size());
  const float scaling_factor = 2 * (computed.size() - 1);
  float tot_energy = 0.f;
  for (size_t i = 0; i < computed.size(); ++i) {
    SCOPED_TRACE(i);
    const float real = computed[i].real() / scaling_factor;
    const float imag = computed[i].imag() / scaling_factor;
    EXPECT_NEAR(expected_real[i], real, 1e-3);
    EXPECT_NEAR(expected_imag[i], imag, 1e-3);
    tot_energy += real * real + imag * imag;
  }
  EXPECT_NEAR(expected_tot_energy, tot_energy, 1.f);
}

}  // namespace

TEST(RnnVad, ProcessHighPassFilterBitExactness) {
  auto samples = ReadAudioFrame<k48k10msFrameSize>(18);
  ASSERT_EQ(samples[0], 27);
  // Not in-place.
  {
    std::array<float, 2> hp_filter_state = {kHighPassFilterMem[0],
                                            kHighPassFilterMem[1]};
    std::array<float, k48k10msFrameSize> samples_filtered;
    ProcessHighPassFilter({samples}, {samples_filtered},
                          {hp_filter_state.data(), hp_filter_state.size()});
    ExpectNear({kExpectedHpFilterOutput}, {samples_filtered},
               kExpectNearTolerance);
  }
  // In-place.
  {
    std::array<float, 2> hp_filter_state = {kHighPassFilterMem[0],
                                            kHighPassFilterMem[1]};
    ProcessHighPassFilter({samples}, {samples},
                          {hp_filter_state.data(), hp_filter_state.size()});
    ExpectNear({kExpectedHpFilterOutput}, {samples}, kExpectNearTolerance);
  }
}

TEST(RnnVad, FftAndBandEnergiesBitExactness) {
  // Get input frame #19.
  auto samples = ReadAudioFrame<k48k20msFrameSize>(18 / 2);
  ASSERT_EQ(samples[0], 27);
  // Apply high-pass filter to the two 10 ms chunks.
  {
    std::array<float, 2> hp_filter_state = {kHighPassFilterMem[0],
                                            kHighPassFilterMem[1]};
    rtc::ArrayView<float, 2> hp_filter_state_view(hp_filter_state.data(), 2);
    rtc::ArrayView<float> frame1(samples.data(), k48k10msFrameSize);
    rtc::ArrayView<float> frame2(samples.data() + k48k10msFrameSize,
                                 k48k10msFrameSize);
    ProcessHighPassFilter(frame1, frame1, hp_filter_state_view);
    ProcessHighPassFilter(frame2, frame2, hp_filter_state_view);
    ASSERT_EQ(kExpectedHpFilterOutput[0], samples[0]);
    ASSERT_NEAR(7.230698f, samples[k48k10msFrameSize], kExpectNearTolerance);
  }
  // Compute FFT.
  const std::array<float, k48k20msFrameSize / 2> half_analysis_win =
      ComputeHalfVorbisWindow<k48k20msFrameSize / 2>();
  std::unique_ptr<RealFourier> fft =
      RealFourier::Create(RealFourier::FftOrder(k48k20msFrameSize));
  AlignedArray<float> fft_input_buf(1, k48k20msFrameSize,
                                    RealFourier::kFftBufferAlignment);
  AlignedArray<std::complex<float>> fft_output_buf(
      1, RealFourier::ComplexLength(fft->order()),
      RealFourier::kFftBufferAlignment);
  ASSERT_EQ(k48k20msNumFftCoeffs, fft_output_buf.cols())
      << "Unexpected FFT size.";
  ComputeForwardFft({samples}, {half_analysis_win}, fft.get(), &fft_input_buf,
                    &fft_output_buf);
  // Check computed FFT.
  CheckFftResult(532.44885f, {kExpectedFftCoeffsReal}, {kExpectedFftCoeffsImag},
                 {fft_output_buf.Row(0), fft_output_buf.cols()});
  // Compute band energies.
  const auto band_boundary_indexes =
      ComputeOpusBandsIndexes(kSampleRate48k, k48k20msFftSize);
  std::array<float, kNumOpusBands> band_energies;
  ComputeBandEnergies(&fft_output_buf, k48k20msFftSize, {band_boundary_indexes},
                      {band_energies.data(), band_energies.size()});
  // Check that the total energy is near.
  EXPECT_NEAR(std::accumulate(kExpectedBandEnergies.begin(),
                              kExpectedBandEnergies.end(), 0.f),
              std::accumulate(band_energies.begin(), band_energies.end(), 0.f),
              1.f);
  // Check the band energy coefficients.
  // ExpectNear({kExpectedBandEnergies}, {band_energies}, kExpectNearTolerance);
}

// TODO(alessiob): Enable once feature extraction is fully implemented.
TEST(RnnVad, DISABLED_FeaturesExtractorBitExactness) {
  // // PCM samples reader and buffers.
  // BinaryFileReader<int16_t, kInputAudioFrameSize, float> samples_reader(
  //     test::ResourcePath("common_audio/rnn_vad/samples", "pcm"));
  // std::array<float, kInputAudioFrameSize> samples;
  // std::array<float, kInputFrameSize> samples_decimated;
  // // Features reader and buffers.
  // BinaryFileReader<float, kFeatureVectorSize> features_reader(
  //     test::ResourcePath("common_audio/rnn_vad/features", "out"));
  // float is_silence;
  // std::array<float, kFeatureVectorSize> features;
  // rtc::ArrayView<const float, kFeatureVectorSize> expected_features_view(
  //     features.data(), features.size());
  // // Feature extractor.
  // RnnVadFeaturesExtractor features_extractor;
  // auto extracted_features_view = features_extractor.GetOutput();
  // // Process frames. The last one is discarded if incomplete.
  // const size_t num_frames = samples_reader.data_length() / kInputFrameSize;
  // for (size_t i = 0; i < num_frames; ++i) {
  //   std::ostringstream ss;
  //   ss << "frame " << i;
  //   SCOPED_TRACE(ss.str());
  //   // Read and downsample audio frame.
  //   samples_reader.ReadChunk({samples.data(), samples.size()});
  //   Decimate48k24k({samples_decimated.data(), samples_decimated.size()},
  //                  {samples.data(), samples.size()});
  //   // Compute feature vector.
  //   features_extractor.ComputeFeatures(
  //       {samples_decimated.data(), samples_decimated.size()});
  //   // Read expected feature vector.
  //   RTC_CHECK(features_reader.ReadValue(&is_silence));
  //   RTC_CHECK(features_reader.ReadChunk({features.data(), features.size()}));
  //   // Check silence flag and feature vector.
  //   EXPECT_EQ(is_silence, features_extractor.is_silence());
  //   ExpectNear(expected_features_view, extracted_features_view);
  // }
}

}  // namespace test
}  // namespace webrtc
