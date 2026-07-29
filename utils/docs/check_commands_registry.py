#!/usr/bin/env python3
"""Check that the commands registry stays in sync with the source.

Four checks:

  1. Every node type declared in BlockNodes.inc, BlockCommandNodes.inc,
     or InlineNodes.inc has a registry entry (or is marked Planned).
  2. Every parser-dispatched command name from the C++ source appears
     in some entry's `syntaxes` list.
  3. Every entry carries the prose payload the page needs: a
     `description` string for every entry, and a `preview` block
     (with `title` and `caption`) for every non-Planned entry.
  4. Every non-Planned entry has at least one example, and each example
     resolves to its golden-test mirror under tests/golden/fixtures/
     snippets/, its Antora source under docs/modules/ROOT/examples/,
     and its rendered .adoc next to that source.

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
    """Return the set of node type names declared in the three .inc files."""
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
    for p in (block_inc, block_cmd_inc, inline_inc):
        types.update(parse_inc_file(p))

    # `Text` holds plain text inside another node; users do not type it as
    # a command, so it has no registry entry.
    types.discard("Text")

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
    and `if (... name == "img")`. An earlier version of this function
    fired on any occurrence of the strings `"a"` or `"img"`, which would
    match for the wrong reason if those literals showed up elsewhere.
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
    """Return every `node_types` value across the registry as one set."""
    types = set()
    for e in entries:
        for nt in e.get("node_types", []):
            types.add(nt)
    return types


def registry_all_syntaxes(entries):
    """Return every `syntaxes` value across the registry as one set."""
    syntaxes = set()
    for e in entries:
        for s in e.get("syntaxes", []):
            syntaxes.add(s)
    return syntaxes


def syntax_to_command_name(syntax):
    """Reduce a registry syntax string to a key comparable with the dispatch.

    Examples:

        "@brief"           -> "brief"
        "@code / @endcode" -> "code"
        "<em>text</em>"    -> "<em>"
        "<br>"             -> "<br>"
        "*text*"           -> "*text*"    (Markdown; left as-is)
    """
    s = syntax.strip()

    if s.startswith("@"):
        # Strip the `@`; for `@code / @endcode`, keep the first command.
        name = s[1:].split()[0].rstrip(",/")
        return name

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
    """Check 3: every entry carries the prose payload the page needs.

    `description` is required everywhere. `preview.title` and
    `preview.caption` are required for non-Planned entries (Planned
    entries have no example, so they have no preview block).
    """
    del root  # No filesystem reads here; the registry itself is the source.
    errors = []
    for e in entries:
        heading = e.get("heading", "?")
        if not e.get("description"):
            errors.append(f"  Entry '{heading}' has no `description`")
        if "Planned" in e.get("status", ""):
            continue
        preview = e.get("preview") or {}
        if not preview.get("title"):
            errors.append(f"  Entry '{heading}' has no `preview.title`")
        if not preview.get("caption"):
            errors.append(f"  Entry '{heading}' has no `preview.caption`")
    return errors


def check_examples_exist(entries, root):
    """Check 4: Every non-Planned entry has at least one example, and every
    referenced example resolves to a real snippet under
    tests/golden/fixtures/snippets/.

    Per spec R9, examples are mandatory for non-Planned entries; missing
    examples are an error, not a warning. Each example also has to back
    a partial that includes it via `include::example$commands/<slug>.cpp[]`,
    so the Antora-side source at docs/modules/ROOT/examples/commands/
    must exist too.
    """
    errors = []
    snippets_dir = os.path.join(root, "tests/golden/fixtures/snippets")
    antora_examples_dir = os.path.join(
        root, "docs/modules/ROOT/examples")
    for e in entries:
        examples = e.get("examples", [])
        status = e.get("status", "")
        heading = e.get("heading", "?")

        # Planned entries are exempt from the example requirement
        if "Planned" in status:
            continue

        if not examples:
            errors.append(
                f"  Entry '{heading}' has no examples and is not Planned "
                f"(at least one example is required)"
            )
            continue

        for ex in examples:
            mirror_cpp = os.path.join(snippets_dir, ex + ".cpp")
            antora_cpp = os.path.join(antora_examples_dir, ex + ".cpp")
            rendered = os.path.join(antora_examples_dir, ex + ".rendered.adoc")
            if not os.path.isfile(mirror_cpp):
                errors.append(
                    f"  Example '{ex}' for entry '{heading}' is missing its "
                    f"golden-test mirror at "
                    f"tests/golden/fixtures/snippets/{ex}.cpp"
                )
            if not os.path.isfile(antora_cpp):
                errors.append(
                    f"  Example '{ex}' for entry '{heading}' is missing its "
                    f"Antora source at docs/modules/ROOT/examples/{ex}.cpp"
                )
            if not os.path.isfile(rendered):
                errors.append(
                    f"  Example '{ex}' for entry '{heading}' is missing its "
                    f"rendered output at "
                    f"docs/modules/ROOT/examples/{ex}.rendered.adoc "
                    f"(run `python utils/render_snippets.py --update`)"
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

    print("Check 3: entries carry description + preview payload...")
    errs = check_payload_present(entries, root)
    all_errors.extend(errs)
    print(f"  {'FAIL' if errs else 'OK'} ({len(entries)} entries, {len(errs)} missing)")
    for e in errs:
        print(e)

    print("Check 4: example files exist (mirror, source, rendered)...")
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
