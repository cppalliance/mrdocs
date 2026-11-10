#!/usr/bin/env python3
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

"""Temporary git repositories for the tests of the incremental guards."""

import subprocess
from pathlib import Path


def git(repo, *args):
    """Run a git command in `repo`."""
    subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false", *args],
                   cwd=str(repo), check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def write(repo, relative, text):
    """Write `text` to `relative` under `repo`, creating its directory."""
    path = Path(repo) / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="")
    return path


def init(repo):
    """Create the repository `repo`, and return it as a `Path`."""
    repo = Path(repo)
    repo.mkdir(parents=True, exist_ok=True)
    git(repo, "init", "-q")
    return repo


def commit_base(repo, message="base"):
    """Commit everything and point `upstream/develop` at the commit, the base of the change."""
    git(repo, "add", ".")
    git(repo, "commit", "-q", "-m", message)
    git(repo, "update-ref", "refs/remotes/upstream/develop", "HEAD")
