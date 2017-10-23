# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Extraction of annotations from audio files.
"""

from __future__ import division
import enum
import logging
import os
import shutil
import struct
import subprocess
import sys
import tempfile

try:
  import numpy as np
except ImportError:
  logging.critical('Cannot import the third-party Python package numpy')
  sys.exit(1)

from . import signal_processing


class AudioAnnotationsExtractor(object):
  """Extracts annotations from audio files.
  """

  @enum.unique
  class VadType(enum.Enum):
    ENERGY_THRESHOLD = 0  # TODO(alessiob): Switch to P56 standard.
    WEBRTC = 1

  _LEVEL_FILENAME = 'level.npy'
  _VAD_FILENAME = 'vad.npy'

  # Level estimation params.
  _ONE_DB_REDUCTION = np.power(10.0, -1.0 / 20.0)
  _LEVEL_FRAME_SIZE_MS = 1.0
  # The time constants in ms indicate the time it takes for the level estimate
  # to go down/up by 1 db if the signal is zero.
  _LEVEL_ATTACK_MS = 5.0
  _LEVEL_DECAY_MS = 20.0

  # VAD params.
  _VAD_THRESHOLD = 1
  _VAD_WEBRTC_PATH = os.path.join(os.path.dirname(
      os.path.abspath(__file__)), os.pardir, os.pardir)
  _VAD_WEBRTC_BIN_PATH = os.path.join(_VAD_WEBRTC_PATH, 'vad')

  def __init__(self, vad_type):
    self._signal = None
    self._level = None
    self._vad = None
    self._level_frame_size = None
    self._c_attack = None
    self._c_decay = None

    self._vad_type = vad_type
    if self._vad_type not in self.VadType:
      # TODO(alessiob): Raise custom exception.
      raise Exception('Invalid vad type: ' + self._vad_type)

    assert os.path.exists(self._VAD_WEBRTC_BIN_PATH), self._VAD_WEBRTC_BIN_PATH

  @classmethod
  def GetLevelFileName(cls):
    return cls._LEVEL_FILENAME

  @classmethod
  def GetVadFileName(cls):
    return cls._VAD_FILENAME

  def GetLevel(self):
    return self._level

  def GetVad(self):
    return self._vad

  def Extract(self, filepath):
    # Load signal.
    self._signal = signal_processing.SignalProcessingUtils.LoadWav(filepath)
    if self._signal.channels != 1:
      raise NotImplementedError('multiple-channel annotations not implemented')

    # Level estimation params.
    self._level_frame_size = int(self._signal.frame_rate / 1000 * (
        self._LEVEL_FRAME_SIZE_MS))
    self._c_attack = 0.0 if self._LEVEL_ATTACK_MS == 0 else (
        self._ONE_DB_REDUCTION ** (
            self._LEVEL_FRAME_SIZE_MS / self._LEVEL_ATTACK_MS))
    self._c_decay = 0.0 if self._LEVEL_DECAY_MS == 0 else (
        self._ONE_DB_REDUCTION ** (
            self._LEVEL_FRAME_SIZE_MS / self._LEVEL_DECAY_MS))

    # Compute level.
    self._LevelEstimation()

    if self._vad_type == self.VadType.ENERGY_THRESHOLD:
      # Naive VAD based on level thresholding. It assumes ideal clean speech
      # with high SNR.
      # TODO(alessiob): Maybe replace with a VAD based on stationary-noise
      # detection.
      vad_threshold = np.percentile(self._level, self._VAD_THRESHOLD)
      self._vad = np.uint8(self._level > vad_threshold)
      self._vad = np.repeat(self._vad, self._level_frame_size)
    elif self._vad_type == self.VadType.WEBRTC:
      self._vad = self._RunWebRtcVad(filepath, self._signal.frame_rate)

  def Save(self, output_path):
    # TODO(alessiob): Save annotations using numpy.savez_compressed.
    np.save(os.path.join(output_path, self._LEVEL_FILENAME), self._level)
    np.save(os.path.join(output_path, self._VAD_FILENAME), self._vad)

  def _LevelEstimation(self):
    # Read samples.
    samples = signal_processing.SignalProcessingUtils.AudioSegmentToRawData(
        self._signal).astype(np.float32) / 32768.0
    num_frames = len(samples) // self._level_frame_size
    num_samples = num_frames * self._level_frame_size

    # Envelope.
    self._level = np.max(np.reshape(np.abs(samples[:num_samples]), (
        num_frames, self._level_frame_size)), axis=1)
    assert len(self._level) == num_frames

    # Envelope smoothing.
    smooth = lambda curr, prev, k: (1 - k) * curr  + k * prev
    self._level[0] = smooth(self._level[0], 0.0, self._c_attack)
    for i in range(1, num_frames):
      self._level[i] = smooth(
          self._level[i], self._level[i - 1], self._c_attack if (
              self._level[i] > self._level[i - 1]) else self._c_decay)

    # Expand to one value per sample.
    # TODO(alessiob): Avoid this, it's just a waste of memory.
    self._level = np.repeat(self._level, self._level_frame_size)

  @classmethod
  def _RunWebRtcVad(cls, wav_file_path, sample_rate):
    vad_output = None

    # Create temporary output path.
    tmp_path = tempfile.mkdtemp()
    output_file_path = os.path.join(
        tmp_path, os.path.split(wav_file_path)[1] + '_vad.tmp')

    # Call WebRTC VAD.
    try:
      subprocess.call([
          cls._VAD_WEBRTC_BIN_PATH,
          '-i', wav_file_path,
          '-o', output_file_path
      ], cwd=cls._VAD_WEBRTC_PATH)

      # Parse output.
      with open(output_file_path, 'rb') as f:
        raw_data = f.read()
      num_bytes = len(raw_data)
      frame_size_ms = struct.unpack('B', raw_data[0])[0]
      assert frame_size_ms in [10, 20, 30]
      extra_bits = struct.unpack('B', raw_data[-1])[0]
      assert 0 <= extra_bits <= 8
      num_frames = 8 * (num_bytes - 2) - extra_bits  # 8 frames for each byte.
      vad_output = np.zeros(num_frames, np.uint8)
      for i, byte in enumerate(raw_data[1:-1]):
        byte = struct.unpack('B', byte)[0]
        for j in range(8 if i < num_bytes - 3 else (8 - extra_bits)):
          vad_output[i * 8 + j] = int(byte & 1)
          byte = byte >> 1

      # Expand to one value per sample.
      # TODO(alessiob): Avoid this, it's just a waste of memory.
      vad_output = np.repeat(vad_output, frame_size_ms * sample_rate / 1000)
    except Exception as e:
      logging.error('Error while running the WebRTC VAD (' + e.message + ')')
    finally:
      if os.path.exists(tmp_path):
        shutil.rmtree(tmp_path)

    return vad_output
