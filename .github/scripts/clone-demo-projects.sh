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

# Clone the third-party demo projects listed in examples/third-party/projects.json.
#
# Boost.URL needs the Boost superproject and Mr.Docs is the current checkout, so
# both are handled separately by the workflow and skipped here. Every other
# project is cloned at its "branch" from either its "documented-fork" (a
# repository that carries the curated doc comments) when that field is set, or
# its upstream "repository" otherwise. The patch from examples/third-party/<id>
# is then overlaid on top of every clone, fork included: the fork holds the
# documentation, but the configuration stays controllable from the Mr.Docs
# repository, where anyone can send a patch.

set -euo pipefail

projects=examples/third-party/projects.json

# Everything but a push to develop, master, or a release tag in the canonical
# repository runs the demos in quick mode (same rule as generate-demos.sh),
# which skips the huge projects entirely, so there is no point cloning them.
quick=true
if [[ "${GITHUB_EVENT_NAME:-}" == 'push' \
   && "${GITHUB_REPOSITORY:-}" == 'cppalliance/mrdocs' ]]; then
    case "${GITHUB_REF:-}" in
        refs/heads/develop|refs/heads/master|refs/tags/*) quick=false ;;
    esac
fi

for i in $(seq 0 $(($(jq length "$projects") - 1))); do
  id=$(jq -r ".[$i].id" "$projects")
  case "$id" in boost-url|mrdocs) continue ;; esac
  if [[ "$quick" == true ]]; then
    case "$id" in llvm|bde) echo "Skipping $id (quick mode)"; continue ;; esac
  fi
  branch=$(jq -r ".[$i].branch" "$projects")
  fork=$(jq -r ".[$i][\"documented-fork\"] // empty" "$projects")
  repo="${fork:-$(jq -r ".[$i].repository" "$projects")}"

  echo "::group::Clone $id ($repo@$branch)"
  git clone --depth 1 --branch "$branch" "$repo" "$id"
  src="examples/third-party/$id"
  [ -d "$src" ] && tar -C "$src" -cf - . | tar -C "$id" -xpf -
  echo "::endgroup::"
done
