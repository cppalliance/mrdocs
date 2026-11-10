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

"""A scratch CMake file API reply for the tests of the run configurations."""

import json
import os

# The executables the run configurations name, as CMake reports them.
PROGRAMS = {
    "mrdocs": "tools/mrdocs/mrdocs",
    "mrdocs-unit-tests": "tests/unit/mrdocs-unit-tests",
    "mrdocs-golden-tests": "tests/golden/mrdocs-golden-tests",
}


def write_reply(build_dir, programs=None, configurations=("",), stamp="2026-01-01T000000-0000"):
    """Write the smallest reply that names ``programs`` under ``build_dir``.

    ``programs`` maps a target to the path of its executable relative to the
    build directory. With more than one name in ``configurations`` the reply
    is the one of a multi-configuration generator, and each executable path
    gets the configuration as a directory before its file name.
    """
    programs = PROGRAMS if programs is None else programs
    reply = os.path.join(build_dir, ".cmake", "api", "v1", "reply")
    os.makedirs(reply, exist_ok=True)

    def put(name, data):
        with open(os.path.join(reply, name), "w") as f:
            json.dump(data, f)
        return name

    model_configs = []
    for config in configurations:
        targets = []
        for target, path in programs.items():
            if config:
                head, tail = os.path.split(path)
                path = f"{head}/{config}/{tail}"
            info = put(f"target-{target}-{config or 'x'}-abc.json", {
                "name": target, "type": "EXECUTABLE", "artifacts": [{"path": path}]})
            targets.append({"name": target, "id": f"{target}::@1", "jsonFile": info})
        model_configs.append({"name": config, "targets": targets})
    model = put(f"codemodel-v2-{stamp}.json", {
        "kind": "codemodel", "version": {"major": 2, "minor": 8},
        "configurations": model_configs})
    put(f"index-{stamp}.json", {
        "objects": [{"kind": "codemodel", "version": {"major": 2, "minor": 8}, "jsonFile": model}],
        "reply": {"client-mrdocs": {"codemodel-v2": {"kind": "codemodel", "jsonFile": model}}}})
