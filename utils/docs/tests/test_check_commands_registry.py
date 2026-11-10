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

"""Tests for utils/docs/check_commands_registry.py, run on scratch trees.

Each test builds the few files the checker reads in a temporary directory,
changes one thing, and expects the matching check to fail with its message.
"""

import contextlib
import copy
import io
import json
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import check_commands_registry as checker  # noqa: E402

BLOCK_INC = "include/mrdocs/Metadata/DocComment/Block/BlockNodes.inc"
BLOCK_CMD_INC = "include/mrdocs/Metadata/DocComment/Block/BlockCommandNodes.inc"
INLINE_INC = "include/mrdocs/Metadata/DocComment/Inline/InlineNodes.inc"
EXTRACT = "src/mrdocs/AST/ExtractDocComment.cpp"
PARSE_INLINES = "src/mrdocs/Metadata/Finalizers/DocComment/parseInlines.hpp"
REGISTRY = "docs/modules/ROOT/partials/commands-registry.json"
SNIPPETS = "tests/golden/fixtures/snippets"

EXTRACT_CPP = """\
static constexpr std::string_view customFlagCommands[] = {
    { "functor", 1 },
};
void visit() {
    auto x = T::KCI_brief;
    if (name == "tip") {}
}
"""

PARSE_INLINES_HPP = """\
static constexpr Rule kRules[] = {
    {"*", 1}, {"<em>", 2},
};
bool f() { return name == "a"; }
"""

# Commands the checker adds without reading the source.
BUILTIN_SYNTAXES = [
    "@b", "@strong", "@c", "@p", "@sa", "@arg", "@htmlonly", "@xmlonly",
    "@latexonly", "@docbookonly", "@manonly", "@rtfonly",
]


def entry(node_type, heading, slug, syntaxes, **extra):
    e = {
        "node_type": node_type,
        "heading": heading,
        "kind": "Block",
        "syntaxes": syntaxes,
        "partial": f"commands/{slug}.adoc",
        "examples": [f"commands/{slug}"],
        "see_also": [],
        "description": f"The {heading} command.",
    }
    e.update(extra)
    return e


def good_registry():
    return {
        "block_commands": {
            "text": [
                entry("ParagraphBlock", "Paragraph", "paragraph",
                      ["@brief", "@tip", "@functor"] + BUILTIN_SYNTAXES),
            ],
        },
        "inline_commands": {
            "style": [
                entry("EmphInline", "Emph", "emph",
                      ["<em>text</em>", "<a>", "*text*"], kind="Inline"),
            ],
        },
    }


class RegistryCheckTest(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)
        self.write(BLOCK_INC, "INFO(Paragraph)\n")
        self.write(BLOCK_CMD_INC, "")
        self.write(INLINE_INC, "INFO(Emph)\nINFO(Text)\n")
        self.write(EXTRACT, EXTRACT_CPP)
        self.write(PARSE_INLINES, PARSE_INLINES_HPP)
        self.registry = good_registry()
        self.save_registry()
        for slug in ("paragraph", "emph"):
            self.write(f"{SNIPPETS}/commands/{slug}.cpp", "")
            self.write(f"{SNIPPETS}/commands/{slug}.adoc", "")

    def write(self, rel, text):
        path = self.root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def save_registry(self):
        self.write(REGISTRY, json.dumps(self.registry))

    def entries(self):
        return checker.load_registry(str(self.root))

    def run_main(self):
        out = io.StringIO()
        argv = sys.argv
        sys.argv = ["check_commands_registry.py", "--repo-root", str(self.root)]
        try:
            with contextlib.redirect_stdout(out):
                code = checker.main()
        finally:
            sys.argv = argv
        return code, out.getvalue()

    def test_good_tree_passes(self):
        code, out = self.run_main()
        self.assertEqual(code, 0, out)
        self.assertIn("PASSED: all checks passed", out)

    def test_check1_declared_type_without_entry(self):
        self.write(BLOCK_INC, "INFO(Paragraph)\nINFO(Callout)\n")
        declared = checker.collect_declared_types(str(self.root))
        errors = checker.check_node_types_covered(declared, self.entries())
        self.assertEqual(
            errors,
            ["  Declared node type 'CalloutBlock' has no registry entry"])
        code, out = self.run_main()
        self.assertEqual(code, 1)
        self.assertIn("FAILED: 1 issue(s) found", out)

    def test_check1_entry_naming_undeclared_type(self):
        self.registry["block_commands"]["text"][0]["node_type"] = "GhostBlock"
        self.save_registry()
        declared = checker.collect_declared_types(str(self.root))
        errors = checker.check_node_types_covered(declared, self.entries())
        self.assertEqual(len(errors), 2)
        self.assertIn("'ParagraphBlock' has no registry entry", errors[0])
        self.assertIn("names node type 'GhostBlock'", errors[1])

    def test_check1_text_inline_needs_no_entry(self):
        declared = checker.collect_declared_types(str(self.root))
        self.assertNotIn("TextInline", declared)
        self.assertEqual(
            declared, {"ParagraphBlock", "EmphInline"})

    def test_check2_dispatched_command_missing(self):
        self.write(EXTRACT, EXTRACT_CPP + "auto y = T::KCI_return;\n")
        dispatched, html = checker.collect_dispatched_commands(
            str(self.root))
        errors = checker.check_commands_covered(
            dispatched, html, self.entries())
        self.assertEqual(
            errors,
            ["  Parser-dispatched command '@return' not found in registry "
             "syntaxes"])

    def test_check2_html_tag_missing(self):
        syntaxes = self.registry["inline_commands"]["style"][0]["syntaxes"]
        syntaxes.remove("<a>")
        self.save_registry()
        dispatched, html = checker.collect_dispatched_commands(
            str(self.root))
        errors = checker.check_commands_covered(
            dispatched, html, self.entries())
        self.assertEqual(
            errors,
            ["  Parser-handled HTML tag '<a>' not found in registry "
             "syntaxes"])

    def test_check2_code_lang_suffix_reads_as_code(self):
        self.assertEqual(
            checker.syntax_to_command_name("@code[{.lang}] ... @endcode"),
            "code")
        self.assertEqual(checker.syntax_to_command_name("<em>text</em>"),
                         "<em>")

    def test_check3_missing_fields(self):
        broken = self.registry["block_commands"]["text"][0]
        del broken["description"]
        broken["syntaxes"] = []
        broken["partial"] = "paragraph.adoc"
        self.save_registry()
        errors = checker.check_payload_present(self.entries(), str(self.root))
        self.assertIn("  Entry 'Paragraph' has no `description`", errors)
        self.assertIn("  Entry 'Paragraph' has no `syntaxes`", errors)
        self.assertTrue(any("`commands/<slug>.adoc`" in e for e in errors))

    def test_check3_duplicates_and_dead_see_also(self):
        second = copy.deepcopy(self.registry["inline_commands"]["style"][0])
        second["see_also"] = ["Nowhere"]
        second["heading"] = "Paragraph"
        second["partial"] = "commands/paragraph.adoc"
        self.registry["inline_commands"]["style"].append(second)
        self.save_registry()
        errors = checker.check_payload_present(self.entries(), str(self.root))
        self.assertIn("  Heading 'Paragraph' is used by more than one entry",
                      errors)
        self.assertIn(
            "  Partial 'commands/paragraph.adoc' is used by more than one "
            "entry", errors)
        self.assertTrue(any("see_also 'Nowhere'" in e for e in errors))

    def test_check4_no_examples_unless_planned(self):
        entry_ = self.registry["block_commands"]["text"][0]
        entry_["examples"] = []
        self.save_registry()
        errors = checker.check_examples_exist(self.entries(), str(self.root))
        self.assertEqual(len(errors), 1)
        self.assertIn("has no examples and is not planned", errors[0])
        entry_["planned"] = True
        self.save_registry()
        self.assertEqual(
            checker.check_examples_exist(self.entries(), str(self.root)), [])

    def test_check4_missing_fixture_file(self):
        (self.root / SNIPPETS / "commands/emph.adoc").unlink()
        errors = checker.check_examples_exist(self.entries(), str(self.root))
        self.assertEqual(
            errors,
            ["  Example 'commands/emph' for entry 'Emph' is missing "
             "tests/golden/fixtures/snippets/commands/emph.adoc"])

    def test_check4_example_outside_commands_dir(self):
        self.registry["block_commands"]["text"][0]["examples"] = ["other/x"]
        self.save_registry()
        errors = checker.check_examples_exist(self.entries(), str(self.root))
        self.assertEqual(len(errors), 1)
        self.assertIn("not of the form `commands/<slug>`", errors[0])


if __name__ == "__main__":
    unittest.main()
