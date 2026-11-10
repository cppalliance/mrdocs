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

"""
Program paths from CMake's file API.

Bootstrap writes a query before it configures a preset, CMake answers it
under ``<build>/.cmake/api/v1/reply/`` when it configures, and the run
configurations take the path of each executable from that answer. The path
is therefore the one the generator chose for this build directory, on every
platform and for single- and multi-configuration generators alike.
"""

import glob
import json
import os
from typing import Optional

from ..core.filesystem import write_text

# The client name CMake files the reply under. The query file is empty: the
# name of the file is the request.
CLIENT = "client-mrdocs"
_QUERY_KIND = "codemodel-v2"


class CMakeReplyError(RuntimeError):
    """The file API reply of a build directory cannot name the program."""


def query_path(build_dir: str) -> str:
    """Where the codemodel query of a build directory goes."""
    return os.path.join(build_dir, ".cmake", "api", "v1", "query", CLIENT, _QUERY_KIND)


def write_query(build_dir: str, dry_run: bool = False, ui=None) -> None:
    """Ask CMake for the code model of ``build_dir`` the next time it configures."""
    write_text(query_path(build_dir), "", dry_run=dry_run, ui=ui)


def _reply_dir(build_dir: str) -> str:
    return os.path.join(build_dir, ".cmake", "api", "v1", "reply")


def has_reply(build_dir: str) -> bool:
    """Whether CMake has answered a query in ``build_dir``."""
    return bool(glob.glob(os.path.join(_reply_dir(build_dir), "index-*.json")))


def _read(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _codemodel(build_dir: str) -> dict:
    """The code model of the newest reply of ``build_dir``."""
    reply = _reply_dir(build_dir)
    indexes = sorted(glob.glob(os.path.join(reply, "index-*.json")))
    if not indexes:
        raise CMakeReplyError(
            f"no CMake file API reply in {build_dir!r}: configure the build "
            f"directory with bootstrap (or cmake) after the query "
            f"{query_path(build_dir)!r} is written")
    index = _read(indexes[-1])
    entry = (index.get("reply", {}).get(CLIENT, {}) or {}).get(_QUERY_KIND)
    if not entry:
        for obj in index.get("objects", []):
            if obj.get("kind") == "codemodel" and obj.get("version", {}).get("major") == 2:
                entry = obj
                break
    if not entry or "jsonFile" not in entry:
        raise CMakeReplyError(
            f"the CMake file API reply in {build_dir!r} has no code model: "
            f"the query {query_path(build_dir)!r} was not there when CMake "
            f"last configured; configure the build directory again")
    return _read(os.path.join(reply, entry["jsonFile"]))


def target_artifact(build_dir: str, target: str, config: Optional[str] = None) -> str:
    """Path of the executable of ``target`` relative to ``build_dir``.

    ``config`` picks the configuration of a multi-configuration generator
    (compared without regard to case); without a match the first one is used.
    Raises ``CMakeReplyError`` when there is no reply, no such target, or the
    target is not an executable.
    """
    model = _codemodel(build_dir)
    configurations = model.get("configurations", [])
    if not configurations:
        raise CMakeReplyError(f"the code model of {build_dir!r} has no configurations")
    chosen = configurations[0]
    if config:
        for candidate in configurations:
            if candidate.get("name", "").lower() == config.lower():
                chosen = candidate
                break
    for entry in chosen.get("targets", []):
        if entry.get("name") != target:
            continue
        info = _read(os.path.join(_reply_dir(build_dir), entry["jsonFile"]))
        if info.get("type") != "EXECUTABLE":
            raise CMakeReplyError(
                f"target {target!r} in {build_dir!r} is a {info.get('type')}, not an EXECUTABLE")
        artifacts = info.get("artifacts") or []
        if not artifacts:
            raise CMakeReplyError(f"target {target!r} in {build_dir!r} has no artifacts")
        return artifacts[0]["path"]
    raise CMakeReplyError(
        f"target {target!r} is not in the CMake file API reply of {build_dir!r}")


def multi_config_name(build_dir: str, config: Optional[str] = None) -> Optional[str]:
    """The configuration a multi-configuration build directory builds.

    Returns ``None`` for a single-configuration generator and for a build
    directory without a reply. Otherwise it returns the name that matches
    ``config`` (without regard to case), else the first one: the same choice
    ``target_artifact`` makes.
    """
    try:
        configurations = _codemodel(build_dir).get("configurations", [])
    except CMakeReplyError:
        return None
    if len(configurations) < 2:
        return None
    if config:
        for candidate in configurations:
            if candidate.get("name", "").lower() == config.lower():
                return candidate["name"]
    return configurations[0]["name"]
