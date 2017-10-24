#!/bin/bash

source ../settings.sh

for clip in "${CLIPS[@]}"; do
  for framerate in "${FRAMERATES[@]}"; do
    merged_file_name=merged-${clip}_f${framerate}.log
    rm -f ${merged_file_name}
    touch ${merged_file_name}
    for resolution in "${RESOLUTIONS[@]}"; do
      test_name=${clip}_r${resolution}_f${framerate}
      log_name=${test_name}.log
      cat ${log_name} >> ${merged_file_name}
    done
  done
done
