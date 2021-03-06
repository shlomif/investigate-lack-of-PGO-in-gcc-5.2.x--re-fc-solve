#!/bin/bash

export SRC_DIR=.

repeat_count="$1"
shift

num_cpus="$(cat /proc/cpuinfo | grep -P '^processor\s*:' | wc -l)"

for I in $(seq 1 "$repeat_count") ; do
    # ARGS="--worker-step 16 -l as" bash "${SRC_DIR:-.}"/scripts/time-threads-num.bash -p "$num_cpus" "$num_cpus"
    ARGS="--worker-step 16 -l as" bash "${SRC_DIR:-.}"/scripts/time-threads-num.bash "$num_cpus" "$num_cpus"
done
