#!/usr/bin/env python3
"""Check the commands registry and the Doxygen page against the source.

Six checks:

  1. Every node type declared in BlockNodes.inc, BlockCommandNodes.inc,
     or InlineNodes.inc has a registry entry. An entry names its node
     type in `node_type`, spelled as the node name plus `Block` or
     `Inline` (`Param` in BlockCommandNodes.inc is `ParamBlock`). Planned
     entries count: the node exists even if the parser does not yet
     produce it.
  2. Every parser-dispatched command name from the C++ source appears
     in some entry's `syntaxes` list, except those in `EXEMPT_COMMANDS`.
  3. Every entry carries what commands-registry-extension.js needs to
     render it: a `heading`, a `description` string, and a `partial` of
     the form commands/<slug>.adoc (the extension only generates a
     per-entry partial for entries whose `partial` has that shape), with
     each slug unique.
  4. Every non-Planned entry has at least one example, and its first
     example resolves to the .cpp source and the golden-generated .adoc
     output under tests/golden/fixtures/snippets/commands/, the two files
     the generated partial includes (the Antora playbook mounts
     tests/golden/fixtures/snippets as example$snippets).
  5. Every registry syntax that begins with `@<word>` names a command
     the parser dispatches (the reverse of check 2). Only the opening
     command of a syntax is checked: in `@code ... @endcode` the closer
     `@endcode` is part of the same construct and is not looked up.
     Syntaxes that do not begin with `@` (Markdown, HTML tags, the
     backslash math spellings) are not covered.
  6. The command tables of the Doxygen compatibility page agree with the
     parser, except for the commands in `EXEMPT_COMMANDS`. The row
     format and the rules are described in the comment at the top of
     the page.

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


# Each .inc file with the suffix that turns one of its names into the
# `node_type` spelling used by the registry. The suffix is needed because
# the block and inline node lists both declare `Code` and `Math`.
NODE_INC_FILES = (
    ("include/mrdocs/Metadata/DocComment/Block/BlockNodes.inc", "Block"),
    ("include/mrdocs/Metadata/DocComment/Block/BlockCommandNodes.inc", "Block"),
    ("include/mrdocs/Metadata/DocComment/Inline/InlineNodes.inc", "Inline"),
)


def collect_declared_types(root):
    """Return the registry `node_type` spelling of every declared node type."""
    types = set()
    for rel_path, suffix in NODE_INC_FILES:
        for name in parse_inc_file(os.path.join(root, rel_path)):
            types.add(name + suffix)

    # `Text` holds plain text inside another node; users do not type it as
    # a command, so it has no registry entry.
    types.discard("TextInline")

    return types


# ---------------------------------------------------------------------------
# 2. Parse source files for parser-dispatched command strings
# ---------------------------------------------------------------------------

# Commands that the registry documents and that ExtractDocComment.cpp
# handles without ever naming them, so the source scans below cannot see
# them, and checks 5 and 6 would wrongly report the registry syntaxes and
# page rows that name them. This is not a list of every command Clang
# delivers by comment kind (other verbatim block commands also end up as
# code blocks). Walking Clang's `CommandTraits` at script time would
# require building Clang, so each entry is listed here with the code path
# that handles it. Add an entry only after tracing that path.
# A command the extractor ignores must not be listed, whether Clang knows it
# but no `case` handles it, or Clang does not know it at all (`@strong`).
KIND_HANDLED_COMMANDS = {
    # `ParamCommandComment`: `visitNode` -> `visitParam`.
    "param",
    # `TParamCommandComment`: `visitNode` -> `visitTParam`.
    "tparam",
    # `VerbatimBlockComment`: `visitNode` -> `visitVerbatimBlock`, whose final
    # branch turns a verbatim block without a special name into a
    # `CodeBlock` with no language.
    "verbatim",
    # `InlineCommandComment`: the default branch of `visitInlineCommand` maps
    # Clang's render kind through `convertStyle`: bold to `StrongInline`
    # for `@b`, monospaced to `CodeInline` for `@c` and `@p`.
    "b",
    "c",
    "p",
}


# Dispatched commands that have neither a registry entry (check 2) nor a
# `Supported` page row (check 6). The parser dispatches `@n` but emits only a
# newline text that merges into a space, so the page lists it as
# `Not supported`.
EXEMPT_COMMANDS = {"n"}


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

    1. The hand-written entries of the `customFlagCommands[]` array (today:
       the `functor` alias). The array's other entries come from
       FlagCommands.inc, which `extract_flag_commands` reads.

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


def extract_flag_commands(path):
    """Extract the lowercase command names from FlagCommands.inc.

    ExtractDocComment.cpp expands each `INFO(PascalName)` entry into a
    `customFlagCommands[]` entry spelled in lowercase, so `FunctionObject`
    becomes `functionobject`.
    """
    return {name.lower() for name in parse_inc_file(path)}


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
    flag_inc_path = os.path.join(
        root, "include/mrdocs/Metadata/DocComment/FlagCommands.inc"
    )

    commands = set()

    # KCI_ constants: the suffix is the command name.
    commands.update(extract_kci_commands(extract_path))

    # Runtime string checks (customFlagCommands plus inline name == "..." tests).
    commands.update(extract_runtime_commands(extract_path))

    # Flag commands (`@functionobject`, `@seebelow`, ...) come from the
    # FlagCommands.inc X-macro table.
    commands.update(extract_flag_commands(flag_inc_path))

    # Commands routed by comment kind or render kind rather than by name.
    commands.update(KIND_HANDLED_COMMANDS)

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


def registry_all_syntaxes(entries):
    """Return every `syntaxes` value across the registry as one set."""
    syntaxes = set()
    for e in entries:
        for s in e.get("syntaxes", []):
            syntaxes.add(s)
    return syntaxes


# The opening `@word` of a syntax or of a page spelling. The formula
# spellings `@f$`, `@f[`, `@f{` and `@f(` begin with `@f` but name no
# command, so they do not match.
OPENING_COMMAND_RE = re.compile(r"@(?!f[$\[{(])(\w+)")


def syntax_to_command_name(syntax):
    """Reduce a registry syntax string to a key comparable with the dispatch.

    Examples:

        "@brief"           -> "brief"
        "@code / @endcode" -> "code"
        "@param[<dir>] <name>" -> "param"
        "<em>text</em>"    -> "<em>"
        "<br>"             -> "<br>"
        "*text*"           -> "*text*"    (Markdown; left as-is)
    """
    s = syntax.strip()

    result = s

    # Keep the first command only, without its `[...]` or `{...}` suffix.
    m = OPENING_COMMAND_RE.match(s)
    if m:
        result = m.group(1)
    else:
        m = re.match(r"<(\w+)", s)
        if m:
            result = f"<{m.group(1)}>"

    # Markdown delimiter or freeform string: returned unchanged.
    return result


# ---------------------------------------------------------------------------
# 4. The checks
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

    for cmd in sorted(dispatched_commands - EXEMPT_COMMANDS):
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


def check_syntaxes_dispatched(dispatched_commands, entries):
    """Check 5: every `@word` registry syntax names a dispatched command.

    The reverse of check 2: a registry entry must not advertise a command
    the parser ignores. Only the opening command of each syntax is looked
    up, so `@code ... @endcode` is checked as `code`.
    """
    errors = []
    for e in entries:
        for s in e.get("syntaxes", []):
            cmd = syntax_to_command_name(s)
            if s.strip().startswith("@") and cmd not in dispatched_commands:
                errors.append(
                    f"  Registry syntax '{s}' in entry "
                    f"'{e.get('heading', '?')}' names command '@{cmd}', "
                    f"which the parser does not dispatch"
                )
    return errors


def check_payload_present(entries):
    """Check 3: every entry carries what the registry extension renders.

    commands-registry-extension.js turns each entry into a partial named
    after the slug in its `partial` field, headed by `heading` and
    followed by `description`. An entry whose `partial` does not match
    commands/<slug>.adoc gets no per-entry partial (the reference page
    still renders it, through the aggregate), and when two entries share a
    slug only one of them gets its per-entry partial.
    """
    errors = []
    slugs = {}
    for e in entries:
        heading = e.get("heading", "?")
        if not e.get("heading"):
            errors.append(f"  Entry '{heading}' has no `heading`")
        if not e.get("description"):
            errors.append(f"  Entry '{heading}' has no `description`")
        errors.extend(partial_errors(e, slugs))
    return errors


def partial_errors(entry, slugs):
    """Return the problems with an entry's `partial`, recording its slug."""
    heading = entry.get("heading", "?")
    errors = []
    m = re.match(r"^commands/(.+)\.adoc$", entry.get("partial") or "")
    if not m:
        errors.append(
            f"  Entry '{heading}' has no `partial` of the form "
            f"commands/<slug>.adoc"
        )
    elif m.group(1) in slugs:
        errors.append(
            f"  Entry '{heading}' reuses the partial of entry "
            f"'{slugs[m.group(1)]}'"
        )
    else:
        slugs[m.group(1)] = heading
    return errors


def check_examples_exist(entries, root):
    """Check 4: every non-Planned entry has at least one example, and its
    first example resolves to a .cpp source and a .adoc output under
    tests/golden/fixtures/snippets/commands/.

    Examples are mandatory for non-Planned entries; a missing example is an
    error, not a warning. The extension includes only the first example of
    an entry, as example$snippets/commands/<slug>.cpp and .adoc, where
    <slug> is the example minus a leading commands/ (the Antora playbook
    mounts tests/golden/fixtures/snippets as example$snippets). The golden
    tests regenerate the .adoc from the .cpp.
    """
    errors = []
    for e in entries:
        if not e.get("planned"):
            errors.extend(entry_example_errors(e, root))
    return errors


def entry_example_errors(entry, root):
    """Return the problems with a non-Planned entry's example."""
    examples = entry.get("examples") or []
    if examples:
        errors = example_file_errors(
            examples[0], entry.get("heading", "?"), root)
    else:
        errors = [
            f"  Entry '{entry.get('heading', '?')}' has no examples and is "
            f"not Planned (at least one example is required)"
        ]
    return errors


def example_file_errors(example, heading, root):
    """Return one error per missing file of an example.

    The path is built the way commands-registry-extension.js builds it.
    """
    slug = example.removeprefix("commands/")
    errors = []
    for ext in (".cpp", ".adoc"):
        rel = f"tests/golden/fixtures/snippets/commands/{slug}{ext}"
        if not os.path.isfile(os.path.join(root, rel)):
            errors.append(
                f"  Example '{example}' for entry '{heading}' is missing {rel}"
            )
    return errors


# ---------------------------------------------------------------------------
# 6. The Doxygen compatibility page
# ---------------------------------------------------------------------------

PAGE_PATH = "docs/modules/ROOT/pages/commands/doxygen.adoc"

# A row is one cell per line: spellings, status, effect, link.
PAGE_ROW_CELLS = 4

# The statuses that say the parser handles the command.
SUPPORTED_STATUSES = ("Supported", "MrDocs only")
PAGE_STATUSES = SUPPORTED_STATUSES + ("Different idiom", "Not supported")


def page_tables(text):
    """Return (line number, cells, stray lines) for each table of the page.

    A table runs between two `|===` lines. Its first line is the header
    row, written on one line, and is skipped. Every other line that starts
    with `|` is one cell, kept with its line number. Any other non-blank
    line is a stray (a wrapped cell), kept with its line number.
    """
    tables = []
    in_table = False
    header_pending = False
    for number, line in enumerate(text.splitlines(), start=1):
        if line.startswith("|==="):
            in_table = not in_table
            header_pending = in_table
            if in_table:
                tables.append((number, [], []))
        elif in_table and line.startswith("|") and header_pending:
            header_pending = False
        elif in_table and line.startswith("|"):
            tables[-1][1].append((number, line[1:].strip()))
        elif in_table and line.strip():
            tables[-1][2].append((number, line.strip()))
    return tables


def table_rows(start, cells, strays):
    """Return (rows, errors) for one table; a malformed table has no rows."""
    errors = [
        f"  {PAGE_PATH}:{number}: line inside a table does not start with "
        f"'|' (a cell must sit on one line): {text}"
        for number, text in strays
    ]
    if len(cells) % PAGE_ROW_CELLS:
        errors.append(
            f"  {PAGE_PATH}:{start}: the table has {len(cells)} cells, not "
            f"a multiple of {PAGE_ROW_CELLS}: a row has the wrong number "
            f"of cells"
        )
    rows = []
    if not errors:
        rows = [
            (cells[i][0], [c for _, c in cells[i:i + PAGE_ROW_CELLS]])
            for i in range(0, len(cells), PAGE_ROW_CELLS)
        ]
    return rows, errors


def page_rows(text):
    """Return (rows, errors): each row is (line number, four cells)."""
    rows = []
    errors = []
    for start, cells, strays in page_tables(text):
        table, table_errors = table_rows(start, cells, strays)
        rows.extend(table)
        errors.extend(table_errors)
    return rows, errors


def row_commands(first_cell):
    """Return the command names looked up in a row's first cell."""
    spans = re.findall(r"`\+?([^`]+)`", first_cell)
    matches = map(OPENING_COMMAND_RE.match, spans)
    return [m.group(1) for m in matches if m]


def page_row_errors(dispatched, required, row):
    """Return the problems with one table row."""
    number, (first, status, _effect, _link) = row
    errors = []
    if status not in PAGE_STATUSES:
        errors.append(
            f"  {PAGE_PATH}:{number}: unknown status '{status}' (expected "
            f"one of: {', '.join(PAGE_STATUSES)})"
        )
    else:
        errors.extend(
            status_errors(dispatched, required, number, first, status))
    return errors


def status_errors(dispatched, required, number, first, status):
    """Return the commands of a row whose status contradicts the parser."""
    supported = status in SUPPORTED_STATUSES
    errors = []
    for cmd in row_commands(first):
        if supported and cmd not in dispatched:
            errors.append(
                f"  {PAGE_PATH}:{number}: row marked '{status}' names "
                f"'@{cmd}', which the parser does not dispatch"
            )
        elif not supported and cmd in required:
            errors.append(
                f"  {PAGE_PATH}:{number}: row marked '{status}' names "
                f"'@{cmd}', which the parser dispatches"
            )
    return errors


def page_supported_commands(rows):
    """Return every command named by a `Supported` or `MrDocs only` row."""
    return {
        cmd
        for _, (first, status, _, _) in rows
        if status in SUPPORTED_STATUSES
        for cmd in row_commands(first)
    }


def page_coverage_errors(required, rows):
    """Return one error per required command without a supporting row."""
    supported = page_supported_commands(rows)
    return [
        f"  Parser-dispatched command '@{cmd}' has no Supported or "
        f"MrDocs only row in {PAGE_PATH}"
        for cmd in sorted(required - supported)
    ]


def page_mismatches(dispatched, text):
    """Return (errors, row count) for the text of the page."""
    required = dispatched - EXEMPT_COMMANDS
    rows, errors = page_rows(text)
    malformed = bool(errors)
    for row in rows:
        errors.extend(page_row_errors(dispatched, required, row))
    if not malformed:
        # A malformed table hides its rows, so coverage would only repeat
        # the damage as a list of missing commands.
        errors.extend(page_coverage_errors(required, rows))
    return errors, len(rows)


def check_page_matches_parser(dispatched, root):
    """Check 6: the page's command tables agree with the parser."""
    path = os.path.join(root, PAGE_PATH)
    if os.path.isfile(path):
        with open(path, encoding="utf-8") as f:
            result = page_mismatches(dispatched, f.read())
    else:
        result = ([f"  {PAGE_PATH} does not exist"], 0)
    return result


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def build_checks(root):
    """Return the checks as (title, errors, total, noun) rows.

    `total` describes what was examined and `noun` names what the errors
    count, so each row prints as `OK (<total>, <n> <noun>)`.
    """
    declared_types = collect_declared_types(root)
    dispatched, html_commands = collect_dispatched_commands(root)
    entries = load_registry(root)
    page_errors, page_row_count = check_page_matches_parser(dispatched, root)
    return (
        ("declared node types have a registry entry",
         check_node_types_covered(declared_types, entries),
         f"{len(declared_types)} types", "gaps"),
        ("parser-dispatched commands appear in the registry",
         check_commands_covered(dispatched, html_commands, entries),
         f"{len(dispatched) + len(html_commands)} commands", "gaps"),
        ("entries carry what the registry extension renders",
         check_payload_present(entries),
         f"{len(entries)} entries", "missing"),
        ("example files exist (source and rendered output)",
         check_examples_exist(entries, root),
         f"{len(entries)} entries", "missing"),
        ("registry syntaxes name dispatched commands",
         check_syntaxes_dispatched(dispatched, entries),
         f"{len(entries)} entries", "undispatched"),
        ("Doxygen page rows match the dispatched commands",
         page_errors, f"{page_row_count} rows", "mismatches"),
    )


def report_check(number, check):
    """Print one check's outcome and return its errors."""
    title, errors, total, noun = check
    print(f"Check {number}: {title}...")
    print(f"  {'FAIL' if errors else 'OK'} ({total}, {len(errors)} {noun})")
    for error in errors:
        print(error)
    return errors


def report_summary(all_errors):
    """Print the final verdict and return the process exit code."""
    print()
    if all_errors:
        print(f"FAILED: {len(all_errors)} issue(s) found")
        result = 1
    else:
        print("PASSED: all checks passed")
        result = 0
    return result


def main():
    args = parse_args()
    print("Commands registry coverage check")
    print(f"Repository root: {args.repo_root}")
    print()
    all_errors = []
    for number, check in enumerate(build_checks(args.repo_root), start=1):
        all_errors.extend(report_check(number, check))
    return report_summary(all_errors)


if __name__ == "__main__":
    sys.exit(main())
