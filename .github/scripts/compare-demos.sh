#!/usr/bin/env bash
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Compare current demos against previous develop demos.
# Expected env vars: GITHUB_OUTPUT
set -x

LOCAL_DEMOS_DIR="./demos/"
PREV_DEMOS_DIR="./demos-previous/"
DIFF_DIR="./demos-diff/"

if [[ ! -d $PREV_DEMOS_DIR || -z $(ls -A $PREV_DEMOS_DIR) ]]; then
    echo "No previous demos found."
    echo "diff=false" >> "$GITHUB_OUTPUT"
    exit 0
fi

mkdir -p $PREV_DEMOS_DIR $DIFF_DIR

find $PREV_DEMOS_DIR -type f | while read previous_file; do
    local_file="${LOCAL_DEMOS_DIR}${previous_file#$PREV_DEMOS_DIR}"
    diff_output="$DIFF_DIR${previous_file#$PREV_DEMOS_DIR}"
    if [[ -f $local_file ]]; then
        mkdir -p "$(dirname "$diff_output")"
        diff "$previous_file" "$local_file" > "$diff_output"
        if [[ ! -s $diff_output ]]; then
            rm "$diff_output"
        fi
    else
        echo "LOCAL FILE $local_file DOES NOT EXITS." > "$diff_output"
        echo "PREVIOUS CONTENT OF THE FILE WAS:" >> "$diff_output"
        cat "$previous_file" >> "$diff_output"
    fi
done

find $LOCAL_DEMOS_DIR -type f | while read local_file; do
    previous_file="${PREV_DEMOS_DIR}${local_file#$LOCAL_DEMOS_DIR}"
    diff_output="$DIFF_DIR${local_file#$LOCAL_DEMOS_DIR}"
    if [[ ! -f $previous_file ]]; then
        echo "PREVIOUS $previous_file DOES NOT EXIST." > "$diff_output"
        echo "IT HAS BEEN INCLUDED IN THIS VERSION." >> "$diff_output"
        echo "NEW CONTENT OF THE FILE IS:" >> "$diff_output"
    fi
done

if [[ -z $(ls -A $DIFF_DIR) ]]; then
    echo "No differences found."
    echo "diff=false" >> "$GITHUB_OUTPUT"
else
    N_FILES=$(find $DIFF_DIR -type f | wc -l)
    echo "Differences found in $N_FILES output files."
    echo "diff=true" >> "$GITHUB_OUTPUT"
fi
