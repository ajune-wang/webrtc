#  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Plots statistics from WebRTC integration test logs.

Usage: $ python plot_all.py filename.txt
"""

from plot_webrtc_test_logs import *

def main():
  filename = sys.argv[1]

  for x_metric in [BITRATE[1], ENC_BITRATE[1]]:
    for idx_metric in range(0, 10):
      idx = 3
      idx_setting = METRICS_TO_PARSE.index(SUBPLOT_SETTINGS[idx])
      setting1 = WIDTH[1]
      setting2 = METRICS_TO_PARSE[idx_setting][1]
      sub_keys = ParseSetting(filename, WIDTH[1])
      y_metrics = [RESULTS[idx_metric][1]] * len(sub_keys)

      metrics = ParseMetrics(filename, setting1, setting2)

      PlotFigure(sub_keys, y_metrics, x_metric, metrics,
                 GetTitle(filename, setting2))

      plt.tight_layout()
      plt.savefig(filename + '-' + RESULTS[idx_metric][1] +
                  '-vs-' + x_metric + '.png')

if __name__ == '__main__':
  main()
