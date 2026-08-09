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
# Regenerate (or, with --check, verify) the reflected DOM schema artifacts:
# the RELAX NG XML schema, the JSON schema, and the textual reference. These
# are produced by running MrDocs over its own headers with the script-driven
# `schema` generator (docs/mrdocs/extensions/schema.js). The JS engine has a
# small heap, so each artifact is produced in its own run.
#
# Usage:
#   MRDOCS=<path> ADDONS=<path> utils/codegen/generate-schema.sh [--check]
# Defaults: MRDOCS from PATH, ADDONS=<repo>/data/mrdocs/addons.
# Optional env: MRDOCS_INPUT (a compilation-database source such as the repo
# CMakeLists.txt) and MRDOCS_EXTRA_ARGS (extra flags, e.g. --stdlib-includes),
# so a CI check can mirror the mrdocs-self-doc invocation exactly.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
MRDOCS="${MRDOCS:-mrdocs}"
ADDONS="${ADDONS:-$repo/data/mrdocs/addons}"
CHECK=0
[[ "${1:-}" == "--check" ]] && CHECK=1

schemas="$repo/docs/modules/ROOT/attachments/schemas/generators"
partials="$repo/docs/modules/ROOT/partials"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

run() { # only-kind -> output dir
  # shellcheck disable=SC2086
  "$MRDOCS" ${MRDOCS_INPUT:+"$MRDOCS_INPUT"} \
    --config="$repo/docs/mrdocs.yml" --addons="$ADDONS" \
    --generator=schema --generator-options.schema.only="$1" \
    --output="$tmp/$1" ${MRDOCS_EXTRA_ARGS:-} >/dev/null
}
run rng
run json
run adoc

status=0
place() { # dst src
  local dst="$1" s="$2"
  if [[ "$CHECK" == 1 ]]; then
    if ! diff -q "$dst" "$s" >/dev/null 2>&1; then
      echo "Schema out of date: $dst (run utils/codegen/generate-schema.sh)"
      status=1
    fi
  else
    mkdir -p "$(dirname "$dst")"
    cp "$s" "$dst"
    echo "wrote $dst"
  fi
}
place "$schemas/mrdocs.rng" "$tmp/rng/generators/mrdocs.rng"
place "$schemas/mrdocs.schema.json" "$tmp/json/generators/mrdocs.schema.json"
place "$partials/dom-schema.adoc" "$tmp/adoc/reference/dom-schema.adoc"
exit $status
