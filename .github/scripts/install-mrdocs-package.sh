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

# Extract and install MrDocs from a build artifact package.
# Expected env vars: RUNNER_OS, GITHUB_ENV, GITHUB_PATH
set -eu

MRDOCS_INSTALL_DIR="$(pwd)/.install/mrdocs"
mkdir -p "$MRDOCS_INSTALL_DIR"

rm -rf packages/_CPack_Packages

echo "::group::Extract MrDocs package to $MRDOCS_INSTALL_DIR"
if [[ "$RUNNER_OS" != 'Windows' ]]; then
    find packages -maxdepth 1 -name 'MrDocs-*.tar.gz' -exec tar -xzf {} -C "$MRDOCS_INSTALL_DIR" --strip-components=1 \;
else
    package=$(find packages -maxdepth 1 -name "MrDocs-*.7z" -print -quit)
    filename=$(basename "$package")
    name="${filename%.*}"
    7z x "${package}" -o"${MRDOCS_INSTALL_DIR}"
    set +e
    robocopy "${MRDOCS_INSTALL_DIR}/${name}" "${MRDOCS_INSTALL_DIR}" //move //e //np //nfl
    exit_code=$?
    set -e
    if (( exit_code >= 8 )); then
        exit 1
    fi
fi
echo "::endgroup::"

echo "::group::Verify installation"
echo "Install root: $MRDOCS_INSTALL_DIR"
echo ""
echo "Top-level contents:"
ls -1 "$MRDOCS_INSTALL_DIR"
echo ""
echo "Binaries:"
ls -1 "$MRDOCS_INSTALL_DIR/bin/"
echo "::endgroup::"

echo "::group::Export environment variables"
echo "MRDOCS_ROOT=$MRDOCS_INSTALL_DIR"
echo "MRDOCS_ROOT=$MRDOCS_INSTALL_DIR" >> "$GITHUB_ENV"
# CMake-valid absolute prefix for find_package(mrdocs). MRDOCS_ROOT above is the
# Git Bash /d/... form on Windows, which CMake rejects; derive this one from
# GITHUB_WORKSPACE and normalize backslashes instead.
MRDOCS_PREFIX="${GITHUB_WORKSPACE//\\//}/.install/mrdocs"
echo "MRDOCS_PREFIX=$MRDOCS_PREFIX"
echo "MRDOCS_PREFIX=$MRDOCS_PREFIX" >> "$GITHUB_ENV"
echo "PATH += $MRDOCS_INSTALL_DIR/bin"
echo "$MRDOCS_INSTALL_DIR/bin" >> "$GITHUB_PATH"
echo "::endgroup::"

$MRDOCS_INSTALL_DIR/bin/mrdocs --version
