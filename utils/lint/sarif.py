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

"""Write the findings of one invariant guard as a SARIF 2.1.0 file.

Each guard collects its findings in a `Log` and writes it with `--sarif FILE`.
A file holds one run of one tool, and the run has the id `<tool>/` in its
`automationDetails`, so that code scanning keeps the runs of the guards apart
when it gets them in one upload. The rule ids are the anchors of the
invariants subpages, or of a page of the layout section, and the `helpUri` of
a rule is the page that holds the anchor, with the anchor as the fragment, so
a reader of a code scanning alert lands on the rule the finding breaks.

The shell guard that has no Python of its own calls this script instead. It
reads one finding per line from standard input, as tab-separated fields
`ruleId`, `level`, `uri`, `startLine` and `message`, where `uri` and
`startLine` may be empty:

    python3 -I utils/lint/sarif.py --tool NAME --rule ID --output FILE < findings.tsv
"""

import argparse
import json
import sys
from pathlib import Path

SCHEMA = "https://json.schemastore.org/sarif-2.1.0.json"
VERSION = "2.1.0"
CONTRIBUTE_URL = "https://mrdocs.com/docs/mrdocs/develop/contribute"
INVARIANTS_URL = CONTRIBUTE_URL + "/invariants"

# The page that holds the anchor of each rule id: a subpage of the invariants page. A
# page that starts with `layout/` is a page of the layout section, which sits
# beside the invariants.
RULE_PAGES = {
    "layout": "layout",
    "unit-test-mirror": "layout/tests",
    "readme-length": "documentation",
    "page-length": "documentation",
    "section-title": "documentation",
    "commands-registry": "documentation",
    "header-rules": "naming-and-layout",
    "clang-format": "formatting",
    "yaml-schema": "codegen",
    "bootstrap-options": "codegen",
    "json-schemas": "codegen",
    "linux-release": "binary-compatibility",
}
LEVELS = ("error", "warning", "note", "none")


def help_uri(rule_id):
    """Return the URL of the invariant that `rule_id` names."""
    page = RULE_PAGES.get(rule_id)
    if page is None:
        return "%s.html#%s" % (INVARIANTS_URL, rule_id)
    base = CONTRIBUTE_URL if page.startswith("layout/") else INVARIANTS_URL
    return "%s/%s.html#%s" % (base, page, rule_id)


class Log:
    """The findings of one tool, to be written as one SARIF run."""

    def __init__(self, tool, rule_ids):
        self.tool = tool
        self.rule_ids = list(rule_ids)
        self.results = []

    def add(self, rule_id, message, level="error", uri=None, start_line=None):
        """Record a finding of the rule `rule_id`, which the tool declared."""
        if rule_id not in self.rule_ids:
            raise ValueError("unknown rule id: %s" % rule_id)
        if level not in LEVELS:
            raise ValueError("unknown level: %s" % level)
        result = {
            "ruleId": rule_id,
            "level": level,
            "message": {"text": message},
        }
        if uri:
            physical = {"artifactLocation": {"uri": uri}}
            if start_line:
                physical["region"] = {"startLine": int(start_line)}
            result["locations"] = [{"physicalLocation": physical}]
        self.results.append(result)

    def to_dict(self):
        """Return the SARIF document."""
        rules = [{"id": rule_id, "helpUri": help_uri(rule_id)} for rule_id in self.rule_ids]
        return {
            "$schema": SCHEMA,
            "version": VERSION,
            "runs": [
                {
                    "tool": {"driver": {"name": self.tool, "rules": rules}},
                    "automationDetails": {"id": self.tool + "/"},
                    "results": self.results,
                }
            ],
        }

    def write(self, path):
        """Write the document to `path`, creating its directory."""
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(self.to_dict(), indent=2) + "\n", encoding="utf-8")


def merge(paths, output):
    """Write to `output` one SARIF file whose runs are those of the files `paths`, in order."""
    runs = []
    for path in paths:
        runs.extend(json.loads(Path(path).read_text(encoding="utf-8"))["runs"])
    target = Path(output)
    target.parent.mkdir(parents=True, exist_ok=True)
    document = {"$schema": SCHEMA, "version": VERSION, "runs": runs}
    target.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def read_findings(lines, log):
    """Add to `log` the findings of tab-separated `lines`."""
    for line in lines:
        line = line.rstrip("\n")
        if not line:
            continue
        fields = line.split("\t", 4)
        if len(fields) != 5:
            raise ValueError("a finding has five tab-separated fields: %r" % line)
        rule_id, level, uri, start_line, message = fields
        log.add(rule_id, message, level or "error", uri or None, start_line or None)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tool", required=True, help="the name of the guard")
    parser.add_argument("--rule", action="append", required=True, dest="rules",
                        help="a rule id the guard can report; repeat for each")
    parser.add_argument("--output", required=True, help="the SARIF file to write")
    args = parser.parse_args(argv)
    log = Log(args.tool, args.rules)
    try:
        read_findings(sys.stdin, log)
    except ValueError as error:
        print("sarif: %s" % error, file=sys.stderr)
        return 2
    log.write(args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
