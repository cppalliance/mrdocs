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

"""Tests for utils/lint/sarif.py."""

import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import sarif


class SarifTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

    def test_a_layout_rule_links_to_a_page_of_the_layout_section(self):
        self.assertEqual(
            sarif.help_uri("unit-test-mirror"),
            sarif.CONTRIBUTE_URL + "/layout/tests.html#unit-test-mirror",
        )

    def test_document_shape(self):
        log = sarif.Log("guard", ["layout", "clang-format"])
        log.add("layout", "x: not listed", uri="x")
        log.add("clang-format", "would change", level="warning", uri="a/b.cpp", start_line=3)
        log.add("layout", "no location")
        doc = log.to_dict()
        self.assertEqual(doc["$schema"], "https://json.schemastore.org/sarif-2.1.0.json")
        self.assertEqual(doc["version"], "2.1.0")
        (run,) = doc["runs"]
        self.assertEqual(run["tool"]["driver"]["name"], "guard")
        self.assertEqual(run["automationDetails"], {"id": "guard/"})
        self.assertEqual(
            run["tool"]["driver"]["rules"],
            [
                {"id": "layout", "helpUri": sarif.INVARIANTS_URL + "/layout.html#layout"},
                {"id": "clang-format", "helpUri": sarif.INVARIANTS_URL + "/formatting.html#clang-format"},
            ],
        )
        first, second, third = run["results"]
        self.assertEqual(first, {
            "ruleId": "layout",
            "level": "error",
            "message": {"text": "x: not listed"},
            "locations": [{"physicalLocation": {"artifactLocation": {"uri": "x"}}}],
        })
        self.assertEqual(second["level"], "warning")
        self.assertEqual(
            second["locations"][0]["physicalLocation"],
            {"artifactLocation": {"uri": "a/b.cpp"}, "region": {"startLine": 3}},
        )
        self.assertNotIn("locations", third)

    def test_help_uri_is_the_subpage_that_holds_the_anchor(self):
        base = "https://mrdocs.com/docs/mrdocs/develop/contribute/invariants"
        self.assertEqual(sarif.help_uri("page-length"), base + "/documentation.html#page-length")
        self.assertEqual(sarif.help_uri("linux-release"), base + "/binary-compatibility.html#linux-release")
        self.assertEqual(sarif.help_uri("other"), base + ".html#other")

    def test_unknown_rule_and_level_are_refused(self):
        log = sarif.Log("guard", ["layout"])
        with self.assertRaises(ValueError):
            log.add("other", "message")
        with self.assertRaises(ValueError):
            log.add("layout", "message", level="fatal")

    def test_write_creates_the_directory(self):
        path = os.path.join(self.tmp, "sarif", "guard.sarif")
        sarif.Log("guard", ["layout"]).write(path)
        with open(path, encoding="utf-8") as f:
            doc = json.load(f)
        self.assertEqual(doc["runs"][0]["results"], [])

    def test_command_line_reads_tab_separated_findings(self):
        out = os.path.join(self.tmp, "cli.sarif")
        text = (
            "linux-release\terror\tbin/mrdocs\t\tneeds GLIBCXX_3.4.30\n"
            "linux-release\twarning\t\t\tno location\n"
        )
        script = os.path.join(os.path.dirname(HERE), "sarif.py")
        subprocess.run(
            [sys.executable, "-I", script, "--tool", "t", "--rule", "linux-release",
             "--output", out],
            input=text, text=True, check=True,
        )
        with open(out, encoding="utf-8") as f:
            results = json.load(f)["runs"][0]["results"]
        self.assertEqual(len(results), 2)
        self.assertEqual(results[0]["message"]["text"], "needs GLIBCXX_3.4.30")
        self.assertEqual(
            results[0]["locations"][0]["physicalLocation"]["artifactLocation"]["uri"],
            "bin/mrdocs",
        )
        self.assertNotIn("locations", results[1])

    def test_merge_keeps_the_runs_in_order(self):
        paths = []
        for name in ("one", "two", "three"):
            path = os.path.join(self.tmp, name + ".sarif")
            log = sarif.Log(name, ["layout"])
            log.add("layout", name)
            log.write(path)
            paths.append(path)
        out = os.path.join(self.tmp, "merged", "all.sarif")
        sarif.merge(paths, out)
        with open(out, encoding="utf-8") as f:
            doc = json.load(f)
        self.assertEqual(doc["version"], "2.1.0")
        self.assertEqual(
            [run["tool"]["driver"]["name"] for run in doc["runs"]],
            ["one", "two", "three"],
        )
        self.assertEqual(doc["runs"][2]["results"][0]["message"]["text"], "three")

    def test_malformed_finding_is_an_error(self):
        script = os.path.join(os.path.dirname(HERE), "sarif.py")
        out = os.path.join(self.tmp, "bad.sarif")
        done = subprocess.run(
            [sys.executable, "-I", script, "--tool", "t", "--rule", "r", "--output", out],
            input="r\terror\n", text=True, capture_output=True,
        )
        self.assertEqual(done.returncode, 2)
        self.assertFalse(os.path.exists(out))


if __name__ == "__main__":
    unittest.main()
