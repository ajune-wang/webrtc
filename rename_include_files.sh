#!/bin/bash

# Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Usage: rename_include_files.sh <prefix> {header-renames}
#
# Given a file with header renames in the form:
#   d/hdr1.h --> d/hdr2.h
# Renames all include paths that match <prefix>/<header> where <header> is an
# entry in the header renames list.
#
# For example,
#   rename_include_files.sh third_party/webrtc rename-headers.txt
# Will rename lines like:
#   #include "third_party/webrtc/api/peerconnectioninterface.h"
# To:
#   #include "third_party/webrtc/api/peer_connection_interface.h"
#

# Usage: pluralize <count> <singular> <plural>
# Examples:
#   pluralize 1 file files --> file
#   pluralize 2 file files --> files
function pluralize {
  if [ "$1" -eq 1 ]; then
    echo "$2"
  else
    echo "$3"
  fi
}

if [[ $# -lt 1 ]]; then
  (>&2 echo "Usage: $0 <prefix> {header-renames}")
  exit 1
fi
prefix="${1%%/}/"

prefix_referencer_paths=$(git grep --files-with-matches "^#include \"$prefix")
num_matches=$(echo "$prefix_referencer_paths" | wc -l)

echo "Found $num_matches $(pluralize $num_matches file files) including the \
prefix: $prefix"

line_regex="([^ ]+) --> ([^ ]+)"
while ((line_no++)); read line; do
  if ! [[ $line =~ $line_regex ]]; then
    (>&2 echo "$line_no: Skipping malformed line: $line")
    continue
  fi

  old_path="${BASH_REMATCH[1]}"
  new_path="${BASH_REMATCH[2]}"

  count=0
  while read -r referencer_path && [[ -n "$referencer_path" ]]; do
    sed -i "s!^#include \"$prefix$old_path\"!#include \"$prefix$new_path\"!" \
        "$referencer_path"
    let count=count+1
  done <<< \
      "$(echo "$prefix_referencer_paths" | \
          xargs grep -l "^#include \"$prefix$old_path\"")"

  if [[ $count -gt 0 ]]; then
    echo "Changed $count include $(pluralize $count directive directives): \
$prefix$old_path --> $prefix$new_path"
  fi
done < "${2:-/dev/stdin}"

