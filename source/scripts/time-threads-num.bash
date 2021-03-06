#!/bin/bash

RUN_SERIAL=false
PROG="${PROG:-./freecell-solver-multi-thread-solve}"
MAX_BOARD="32000"

while getopts "spt:" flag ; do
    # Run the serial scan first.
    if   [ "$flag" = "s" ] ; then
        RUN_SERIAL=true
    # Run the multi-process version instead of the multi-threaded version.
    elif [ "$flag" = "p" ] ; then
        PROG="./freecell-solver-fork-solve"
    # Maximal board - mnemonic - "to".
    elif [ "$flag" = "t" ] ; then
        MAX_BOARD="$OPTARG"
    fi
done

while [ $OPTIND -ne 1 ] ; do
    shift
    let OPTIND--
done

MIN="${1:-1}"
shift

MAX="${1:-6}"
shift

OUT_DIR="${OUT_DIR:-.}"
ARGS="${ARGS:--l gi}"

# strip * > /dev/null 2>&1

DUMPS_DIR="$OUT_DIR/$(date +"DUMPS-%s")"
mkdir -p "$DUMPS_DIR"

if $RUN_SERIAL ; then
    echo "Testing Serial Run"
    # For letting the screen update.
    sleep 1
    ./freecell-solver-range-parallel-solve 1 "$MAX_BOARD" 4000 $ARGS > "$DUMPS_DIR"/dump
fi


for NUM in $(seq "$MIN" "$MAX") ; do
    echo "Testing $NUM"
    # For letting the screen update.
    sleep 1
    $PROG 1 "$MAX_BOARD" 4000 \
        --num-workers "$NUM" \
        $ARGS | tee "$(printf "%s/dump%.3i" "$DUMPS_DIR" "$NUM")"
done

