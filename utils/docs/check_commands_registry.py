#!/usr/bin/env python3
"""Check that the commands registry stays in sync with the source.

Four checks:

  1. Every node type declared in BlockNodes.inc, BlockCommandNodes.inc,
     or InlineNodes.inc has a registry entry (or is marked `planned`).
     An entry names its node by the AST class: `ParagraphBlock` for
     `INFO(Paragraph)` in a block file, `EmphInline` for `INFO(Emph)` in
     the inline file.
  2. Every parser-dispatched command name from the C++ source appears
     in some entry's `syntaxes` list.
  3. Every entry carries the fields commands-registry-extension.js
     renders: a `description`, a `partial` of the form
     `commands/<slug>.adoc`, and at least one syntax.
  4. Every entry that is not `planned` has at least one example, and
     each example resolves to a golden fixture under tests/golden/
     fixtures/snippets/: the `.cpp` source and the `.adoc` output the
     golden test generates. The extension includes both through the
     `example$snippets/` mount (docs/extensions/repo-snippets-extension.js),
     so there is no separate copy under docs/modules/ROOT/examples/.

Runs in the utility-tests CI step. Reads source files only; does not
need mrdocs built.

Usage:
    python utils/docs/check_commands_registry.py [--repo-root DIR]

Exits 0 on success, non-zero on any violation.
"""

import argparse
import json
import os
import re
import sys


def parse_args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--repo-root",
        default=os.path.dirname(os.path.dirname(
            os.path.dirname(os.path.abspath(__file__)))),
        help="Repository root (default: auto-detected from script location)",
    )
    return p.parse_args()


# ---------------------------------------------------------------------------
# 1. Parse .inc files to extract declared node type names
# ---------------------------------------------------------------------------

def parse_inc_file(path):
    """Extract INFO(Name) entries from an X-macro .inc file."""
    types = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"^\s*INFO\(\s*(\w+)\s*\)", line)
            if m:
                types.append(m.group(1))
    return types


def collect_declared_types(root):
    """Return the AST class names declared in the three .inc files.

    `INFO(Paragraph)` in a block file is the class `ParagraphBlock`;
    `INFO(Emph)` in the inline file is `EmphInline`. These are the names
    the registry stores in `node_type`.
    """
    block_inc = os.path.join(
        root, "include/mrdocs/Metadata/DocComment/Block/BlockNodes.inc"
    )
    block_cmd_inc = os.path.join(
        root, "include/mrdocs/Metadata/DocComment/Block/BlockCommandNodes.inc"
    )
    inline_inc = os.path.join(
        root, "include/mrdocs/Metadata/DocComment/Inline/InlineNodes.inc"
    )

    types = set()
    for p, suffix in ((block_inc, "Block"),
                      (block_cmd_inc, "Block"),
                      (inline_inc, "Inline")):
        types.update(name + suffix for name in parse_inc_file(p))

    # `Text` holds plain text inside another node; users do not type it as
    # a command, so it has no registry entry.
    types.discard("TextInline")

    return types


# ---------------------------------------------------------------------------
# 2. Parse source files for parser-dispatched command strings
# ---------------------------------------------------------------------------

def extract_kci_commands(path):
    """Extract KCI_xxx command names from ExtractDocComment.cpp."""
    cmds = set()
    with open(path, encoding="utf-8") as f:
        for line in f:
            for m in re.finditer(r"T::KCI_(\w+)", line):
                cmds.add(m.group(1))
    return cmds


def extract_runtime_commands(path):
    """Pull command names that ExtractDocComment.cpp checks at runtime.

    Two places to look:

    1. The customFlagCommands[] array (today: `functionobject`, `functor`).
       Reading entries from the array body means adding a new flag command
       to the array picks it up here too.

    2. Inline `name == "xxx"` comparisons. In ExtractDocComment.cpp this
       hits the admonition checks (`name == "tip"`, `"important"`,
       `"caution"`). The regex requires a string literal on the right, so
       member access like `p->name == param.name` does not match.
    """
    cmds = set()
    with open(path, encoding="utf-8") as f:
        content = f.read()

    array_match = re.search(
        r"customFlagCommands\[\]\s*=\s*\{(.*?)\};", content, re.DOTALL)
    if not array_match:
        sys.exit(
            f"error: customFlagCommands[] array not found in {path}. "
            f"If the array was renamed or moved, update this script.")
    for m in re.finditer(r'\{\s*"(\w+)"', array_match.group(1)):
        cmds.add(m.group(1))

    for m in re.finditer(r'name\s*==\s*"(\w+)"', content):
        cmds.add(m.group(1))

    return cmds


def extract_krules_syntaxes(path):
    """Pull inline syntax delimiters from the kRules table."""
    with open(path, encoding="utf-8") as f:
        content = f.read()

    krules_match = re.search(r"kRules\[\]\s*=\s*\{(.*?)\};", content, re.DOTALL)
    if not krules_match:
        sys.exit(
            f"error: kRules[] table not found in {path}. If the table "
            f"was renamed or its declaration shape changed, update this "
            f"script.")

    syntaxes = set()
    for m in re.finditer(r'"([^"]+)"', krules_match.group(1)):
        syntaxes.add(m.group(1))
    return syntaxes


def extract_html_tag_handlers(path):
    """Pull HTML tag names handled by parseInlines.hpp outside the kRules table.

    Matches the dispatch pattern the file uses: `if (... name == "a")`
    and `if (... name == "img")`, not any occurrence of the strings
    `"a"` or `"img"`.
    """
    tags = set()
    with open(path, encoding="utf-8") as f:
        content = f.read()
    if re.search(r'name\s*==\s*"a"', content):
        tags.add("<a>")
    if re.search(r'name\s*==\s*"img"', content):
        tags.add("<img>")
    return tags


def collect_dispatched_commands(root):
    """Return the set of command names the parser handles.

    These are the strings users type (without the `@`) that the parser
    routes somewhere specific.
    """
    extract_path = os.path.join(root, "src/mrdocs/AST/ExtractDocComment.cpp")
    parse_inlines_path = os.path.join(
        root, "src/mrdocs/Metadata/Finalizers/DocComment/parseInlines.hpp"
    )

    commands = set()

    # KCI_ constants: the suffix is the command name.
    commands.update(extract_kci_commands(extract_path))

    # Runtime string checks (customFlagCommands plus inline name == "..." tests).
    commands.update(extract_runtime_commands(extract_path))

    # Render-kind commands (`@b`, `@strong`, `@c`, `@p`): the default branch
    # in visitInlineCommand routes them by Clang's render kind, so there is
    # no `KCI_` constant or string literal to grep. Hard-coded here because
    # walking CommandTraits at script time would require building Clang.
    commands.update({"b", "strong", "c", "p"})

    # Clang aliases without their own KCI_ identifier: `@sa` aliases `@see`
    # and `@arg` aliases `@li`. Clang routes both to the aliased command's
    # KCI_ constant, so the dispatch never mentions them by name. Hard-coded
    # for the same reason as above. (`@related` has its own `KCI_related`
    # constant; extract_kci_commands finds it without help.)
    commands.update({"sa", "arg"})

    # Output-format verbatim commands: visitVerbatimBlock strips the
    # `only` suffix to get the code block language, so no literal names them.
    commands.update({"htmlonly", "xmlonly", "latexonly",
                     "docbookonly", "manonly", "rtfonly"})

    # kRules carries the inline delimiters (Markdown plus HTML opening tags
    # like `<em>`, `<strong>`, `<br>`). The Markdown ones (`*`, `**`, `~~`,
    # ...) are documented by their structure, not their literal characters,
    # so the only kRules entries that need a registry match are the HTML
    # opening tags.
    krules = extract_krules_syntaxes(parse_inlines_path)
    commands_from_html = {
        s for s in krules
        if s.startswith("<") and not s.startswith("</")
    }
    # `<a>` and `<img>` live outside kRules; pick them up from the
    # separate dispatch.
    commands_from_html.update(extract_html_tag_handlers(parse_inlines_path))

    return commands, commands_from_html


# ---------------------------------------------------------------------------
# 3. Parse the registry
# ---------------------------------------------------------------------------

def load_registry(root):
    """Load the registry JSON and return one flat list of entries."""
    path = os.path.join(
        root, "docs/modules/ROOT/partials/commands-registry.json")
    with open(path, encoding="utf-8") as f:
        data = json.load(f)

    entries = []
    for top_key in ("block_commands", "inline_commands"):
        section = data.get(top_key, {})
        if not section:
            continue
        for sub_key, entry_list in section.items():
            if not entry_list:
                continue
            for entry in entry_list:
                entry["_category"] = f"{top_key}.{sub_key}"
                entries.append(entry)
    return entries


def registry_node_types(entries):
    """Return every `node_type` value across the registry as one set."""
    return {e["node_type"] for e in entries if e.get("node_type")}


def syntax_to_command_name(syntax):
    """Reduce a registry syntax string to a key comparable with the dispatch.

    Examples:

        "@brief"                       -> "brief"
        "@code[{.lang}] ... @endcode"  -> "code"
        "<em>text</em>"                -> "<em>"
        "<br>"                         -> "<br>"
        "*text*"                       -> "*text*"  (Markdown; left as-is)
    """
    s = syntax.strip()

    if s.startswith("@"):
        # The first word after `@` is the command; a `[{.lang}]` suffix
        # or a closing `@endcode` does not change it.
        m = re.match(r"@(\w+)", s)
        return m.group(1) if m else s

    m = re.match(r"<(\w+)", s)
    if m:
        return f"<{m.group(1)}>"

    # Markdown delimiter or freeform string: return unchanged.
    return s


# ---------------------------------------------------------------------------
# 4. The four checks
# ---------------------------------------------------------------------------

def check_node_types_covered(declared_types, entries):
    """Check 1: Every declared type has a registry entry."""
    registry_types = registry_node_types(entries)
    errors = []
    for t in sorted(declared_types):
        if t not in registry_types:
            errors.append(
                f"  Declared node type '{t}' has no registry entry"
            )
    # The other direction: a `node_type` must name a declared class, so a
    # renamed or misspelled class cannot hide behind a stale entry.
    for e in entries:
        t = e.get("node_type")
        if t and t not in declared_types:
            errors.append(
                f"  Entry '{e.get('heading', '?')}' names node type '{t}', "
                f"which no .inc file declares"
            )
    return errors


def check_commands_covered(dispatched_commands, html_commands, entries):
    """Check 2: every parser-dispatched command appears in a registry entry."""
    registry_cmds = {
        syntax_to_command_name(s)
        for e in entries
        for s in e.get("syntaxes", [])
    }

    errors = []

    for cmd in sorted(dispatched_commands):
        # `@n` is parser-dispatched but only emits a newline inside a
        # TextInline; it has no documentation entry, like the `Text` node
        # type itself.
        if cmd == "n":
            continue
        if cmd not in registry_cmds:
            errors.append(
                f"  Parser-dispatched command '@{cmd}' not found in registry syntaxes"
            )

    for tag in sorted(html_commands):
        if tag not in registry_cmds:
            errors.append(
                f"  Parser-handled HTML tag '{tag}' not found in registry syntaxes"
            )

    return errors


def check_payload_present(entries, root):
    """Check 3: every entry carries the fields the extension renders.

    The extension writes one partial per entry at the path named by
    `partial`, using `heading`, `syntaxes` and `description`. It keys
    partials by slug and anchors by heading, so both must be unique, and
    each `see_also` name must be the heading of some entry.
    """
    del root  # No filesystem reads here; the registry itself is the source.
    errors = []
    headings = [e.get("heading", "?") for e in entries]
    partials = [e.get("partial") for e in entries if e.get("partial")]
    for h in sorted({h for h in headings if headings.count(h) > 1}):
        errors.append(f"  Heading '{h}' is used by more than one entry")
    for pt in sorted({pt for pt in partials if partials.count(pt) > 1}):
        errors.append(f"  Partial '{pt}' is used by more than one entry")
    for e in entries:
        for name in e.get("see_also", []):
            if name not in headings:
                errors.append(
                    f"  Entry '{e.get('heading', '?')}' lists see_also "
                    f"'{name}', which is not the heading of any entry")
    for e in entries:
        heading = e.get("heading", "?")
        if not e.get("description"):
            errors.append(f"  Entry '{heading}' has no `description`")
        if not e.get("syntaxes"):
            errors.append(f"  Entry '{heading}' has no `syntaxes`")
        if not re.fullmatch(r"commands/[\w-]+\.adoc", e.get("partial", "")):
            errors.append(
                f"  Entry '{heading}' has no `partial` of the form "
                f"`commands/<slug>.adoc`")
    return errors


def check_examples_exist(entries, root):
    """Check 4: every entry that is not `planned` has at least one example,
    and each example is a golden fixture.

    An example `commands/x` names tests/golden/fixtures/snippets/commands/
    x.cpp (the source) and x.adoc (the output the golden test generates).
    The extension includes both through the `example$snippets/` mount, so
    a missing file is an include error in the docs build.
    """
    errors = []
    snippets_dir = os.path.join(root, "tests/golden/fixtures/snippets")
    for e in entries:
        examples = e.get("examples", [])
        heading = e.get("heading", "?")

        if e.get("planned"):
            continue

        if not examples:
            errors.append(
                f"  Entry '{heading}' has no examples and is not planned "
                f"(at least one example is required)"
            )
            continue

        for ex in examples:
            # The extension strips `commands/` and includes
            # `example$snippets/commands/<slug>`, so any other prefix
            # points the docs at a file this check never looked at.
            if not re.fullmatch(r"commands/[\w-]+", ex):
                errors.append(
                    f"  Example '{ex}' for entry '{heading}' is not of the "
                    f"form `commands/<slug>`"
                )
                continue
            for ext in (".cpp", ".adoc"):
                path = os.path.join(snippets_dir, ex + ext)
                if not os.path.isfile(path):
                    errors.append(
                        f"  Example '{ex}' for entry '{heading}' is missing "
                        f"tests/golden/fixtures/snippets/{ex}{ext}"
                    )
    return errors


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    args = parse_args()
    root = args.repo_root

    print("Commands registry coverage check")
    print(f"Repository root: {root}")
    print()

    declared_types = collect_declared_types(root)
    dispatched_commands, html_commands = collect_dispatched_commands(root)
    entries = load_registry(root)

    all_errors = []

    print("Check 1: declared node types have a registry entry...")
    errs = check_node_types_covered(declared_types, entries)
    all_errors.extend(errs)
    print(f"  {'FAIL' if errs else 'OK'} ({len(declared_types)} types, {len(errs)} gaps)")
    for e in errs:
        print(e)

    print("Check 2: parser-dispatched commands appear in the registry...")
    errs = check_commands_covered(dispatched_commands, html_commands, entries)
    all_errors.extend(errs)
    cmd_count = len(dispatched_commands) + len(html_commands)
    print(f"  {'FAIL' if errs else 'OK'} ({cmd_count} commands, {len(errs)} gaps)")
    for e in errs:
        print(e)

    print("Check 3: entries carry the fields the extension renders...")
    errs = check_payload_present(entries, root)
    all_errors.extend(errs)
    print(f"  {'FAIL' if errs else 'OK'} ({len(entries)} entries, {len(errs)} missing)")
    for e in errs:
        print(e)

    print("Check 4: example fixtures exist (source and golden output)...")
    errs = check_examples_exist(entries, root)
    all_errors.extend(errs)
    print(f"  {'FAIL' if errs else 'OK'} ({len(entries)} entries, {len(errs)} missing)")
    for e in errs:
        print(e)

    print()
    if all_errors:
        print(f"FAILED: {len(all_errors)} issue(s) found")
        return 1
    print("PASSED: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
