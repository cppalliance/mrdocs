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
# Smoke-test every starter project under `examples/getting-started/`.
# For each pattern, run whatever prep is documented in its README,
# then invoke mrdocs against the included `docs/mrdocs.yml` and
# check the run succeeds and produces output. The same files are
# referenced from `usage.adoc`; a silent breakage here would
# surface as a broken example for every new user.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

MRDOCS_BIN="${MRDOCS_BIN:-mrdocs}"
CMAKE_BIN="${CMAKE_BIN:-cmake}"

failed=()

# (group, example) pairs. The group is the directory under examples/.
examples=(
    "getting-started/compilation-database"
    "getting-started/cmake"
    "getting-started/cmake-header-only"
    "getting-started/scanned"
    "dependencies/find"
    "dependencies/shim-files"
    "dependencies/shim-snippets"
    "dependencies/accept-missing"
)

for rel in "${examples[@]}"; do
    project="${REPO_ROOT}/examples/${rel}"
    config="${project}/docs/mrdocs.yml"
    output="${project}/docs/reference-output"
    label="$(echo "${rel}" | tr '/' '-')"

    echo "=== ${rel} ==="
    if [[ ! -f "${config}" ]]; then
        echo "  missing config: ${config}"
        failed+=("${rel}")
        continue
    fi

    rm -rf "${output}"

    if ! "${MRDOCS_BIN}" "${config}" --output "${output}" > "/tmp/mrdocs-${label}.log" 2>&1; then
        echo "  mrdocs exited non-zero; tail of log:"
        tail -20 "/tmp/mrdocs-${label}.log" | sed 's/^/    /'
        failed+=("${rel}")
        continue
    fi
    if ! ls "${output}" >/dev/null 2>&1 || [[ -z "$(ls "${output}" 2>/dev/null)" ]]; then
        echo "  no output written under ${output}"
        failed+=("${rel}")
        continue
    fi
    echo "  ok"
done

if (( ${#failed[@]} > 0 )); then
    echo
    echo "FAILED: ${failed[*]}"
    exit 1
fi

echo
echo "All starter examples ran successfully."
