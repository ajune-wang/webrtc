#!/bin/bash

source ../settings.sh

for clip in "${CLIPS[@]}"; do
  for resolution in "${RESOLUTIONS[@]}"; do
    merged_file_name=merged-${clip}_r${resolution}.log
    rm -f ${merged_file_name}
    touch ${merged_file_name}
    for framerate in "${FRAMERATES[@]}"; do
      test_name=${clip}_r${resolution}_f${framerate}
      log_name=${test_name}.log
      cat ${log_name} >> ${merged_file_name}
    done
  done
done
