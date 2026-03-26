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

# Verify that snippet .cpp files match golden tests.
set -euo pipefail
shopt -s nullglob

SRC="docs/website/snippets"
DST="test-files/golden-tests/snippets"

[[ -d "$SRC" ]] || { echo "Source directory not found: $SRC"; exit 2; }
[[ -d "$DST" ]] || { echo "Destination directory not found: $DST"; exit 2; }

missing=()
mismatched=()

while IFS= read -r -d '' src; do
    rel="${src#$SRC/}"
    dst="$DST/$rel"
    if [[ ! -f "$dst" ]]; then
        missing+=("$rel")
        continue
    fi
    if ! git diff --no-index --ignore-cr-at-eol --quiet -- "$src" "$dst"; then
        mismatched+=("$rel")
    fi
done < <(find "$SRC" -type f -name '*.cpp' -print0)

if (( ${#missing[@]} || ${#mismatched[@]} )); then
    if (( ${#missing[@]} )); then
        echo "Missing corresponding golden files:"
        printf '  %s\n' "${missing[@]}"
    fi
    if (( ${#mismatched[@]} )); then
        echo "Content mismatches:"
        printf '  %s\n' "${mismatched[@]}"
    fi
    exit 1
fi
echo "All snippet .cpp files are present and match."
