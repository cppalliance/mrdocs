#!/usr/bin/env python3
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

"""
Justfile run configuration generation.

Generates a ``justfile`` from the same shared task list that drives the
VS Code, CLion, and Visual Studio generators. Every IDE task/launch
configuration becomes a ``just`` recipe, giving an editor-independent,
terminal- and agent-friendly list of project commands.

The generated file is per-machine and gitignored, like the other IDE
config outputs. It is merged in place by recipe name so hand-written
recipes survive regeneration.
"""

import importlib.util
import os
import re
import shlex
import shutil

from ..core.platform import is_windows
from ..core.filesystem import write_text
from ..core.ui import get_default_ui

# just recipe names are NAME = [a-zA-Z_][a-zA-Z0-9_-]*
_VALID_RECIPE_ID = re.compile(r"^[a-zA-Z_][a-zA-Z0-9_-]*$")

# A recipe header at column 0: optional '@', a name, anything up to the
# first ':' that is not part of a ':=' assignment.
_RECIPE_HEADER = re.compile(r"^@?[A-Za-z_][A-Za-z0-9_-]*[^:]*:(?!=)")
_RECIPE_NAME = re.compile(r"^@?\s*([A-Za-z_][A-Za-z0-9_-]*)")

# justfile variable aliasing the repo root, the just equivalent of VS Code's
# ${workspaceFolder}. Recipes reference it as {{mrdocs}} instead of repeating
# the machine-specific absolute path.
_MRDOCS_VAR = "mrdocs"

# A leading "MrDocs" word (case-insensitive) plus any following separators.
# Every config name currently starts with "MrDocs"; stripping it keeps the
# generated recipe ids short and readable.
_MRDOCS_PREFIX = re.compile(r"(?i)^mrdocs\b[\s:_./-]*")


def slugify_token(text: str) -> str:
    """Lowercase ``text`` and collapse anything outside [a-z0-9] into single
    hyphens. Used for the preset suffix appended to per-preset recipe ids.
    Returns an empty string when nothing usable remains."""
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


def slugify_recipe_id(name: str) -> str:
    """Turn a config display name into a valid ``just`` recipe id.

    Strips the redundant leading "MrDocs" prefix, lowercases, and reduces
    runs of non-alphanumeric characters to single hyphens. Guarantees the
    result matches the just NAME grammar ([a-zA-Z_][a-zA-Z0-9_-]*).
    """
    stripped = _MRDOCS_PREFIX.sub("", name.strip())
    # If stripping the prefix leaves nothing (the name was just "MrDocs"),
    # fall back to the full name so we still produce a recipe id.
    candidate = stripped if stripped.strip() else name.strip()

    slug = slugify_token(candidate)
    if not slug:
        slug = "recipe"
    # The grammar requires the first character to be a letter or underscore.
    if not re.match(r"[a-z_]", slug):
        slug = "_" + slug
    return slug


def recipe_id_for(config: dict) -> str:
    """Resolve the base recipe id for a config entry.

    An explicit ``id`` field is honored as an escape hatch when it is a
    valid recipe name; otherwise the display name is auto-slugged.
    """
    explicit = config.get("id")
    if explicit and _VALID_RECIPE_ID.match(explicit):
        return explicit
    if explicit:
        # Explicit but not a legal recipe name: slugify it so we never emit
        # an invalid recipe header.
        return slugify_recipe_id(explicit)
    return slugify_recipe_id(config["name"])


def assign_recipe_ids(configs, suffix: str = "", reserved=None) -> list:
    """Assign a unique recipe id to each config entry.

    Returns a list of ``(config, recipe_id)`` tuples in input order.
    ``suffix`` (e.g. a preset name) is appended to every base id. ``reserved``
    is a set of ids already taken by generator-injected recipes (e.g.
    ``build``/``configure``) so config ids never collide with them. Collisions
    are resolved deterministically by appending ``-2``, ``-3``, and so on.
    """
    suffix_slug = slugify_token(suffix) if suffix else ""
    used = set(reserved or ())
    result = []
    for cfg in configs:
        base = recipe_id_for(cfg)
        if suffix_slug:
            base = f"{base}-{suffix_slug}"
        rid = base
        n = 2
        while rid in used:
            rid = f"{base}-{n}"
            n += 1
        used.add(rid)
        result.append((cfg, rid))
    return result


# ── shell quoting / command building (host-appropriate) ──────────────

def _q(s: str) -> str:
    """Quote a single shell argument for the host shell."""
    if is_windows():
        return '"' + str(s).replace('"', '""') + '"'
    return shlex.quote(str(s))


def _doc_attr(text: str) -> str:
    """Render a [doc("...")] attribute, escaping for a just double-quoted string."""
    escaped = text.replace("\\", "\\\\").replace('"', '\\"')
    return f'[doc("{escaped}")]'


def _group_attr(folder: str) -> str:
    escaped = folder.replace("\\", "\\\\").replace('"', '\\"')
    return f'[group("{escaped}")]'


def _env_prefix(env: dict) -> str:
    """Inline environment assignments to prepend to a command."""
    if not env:
        return ""
    if is_windows():
        return "".join(f"$env:{k}={_q(v)}; " for k, v in env.items())
    return " ".join(f"{k}={_q(v)}" for k, v in env.items()) + " "


def _relativize(line: str, source_dir: str) -> str:
    """Replace the absolute source directory with the `{{mrdocs}}` alias.

    Mirrors the `${workspaceFolder}` substitution the VS Code generator does,
    so recipes read cleanly and are not tied to one machine's absolute path.
    """
    return line.replace(source_dir, "{{" + _MRDOCS_VAR + "}}")


def _configure_if_unconfigured(build_dir: str, preset_slug: str) -> str:
    """A body line that runs ``just configure-<preset>`` only when the build dir
    is absent. Like the build guard, it delegates to the configure recipe
    rather than inlining cmake, so the relationship is explicit and the cmake
    command lives in exactly one place."""
    if is_windows():
        return f"@if (!(Test-Path {_q(build_dir)})) {{ just configure-{preset_slug} }}"
    return f"@test -d {_q(build_dir)} || just configure-{preset_slug}"


def _cd_prefix(cwd: str) -> str:
    """A `cd <dir> &&` (or `;` on Windows) prefix for running inside ``cwd``."""
    return f"cd {_q(cwd)} && " if not is_windows() else f"cd {_q(cwd)}; "


def _script_invocation(config: dict) -> str:
    """Build the command string that runs a script config."""
    script = config["script"]
    args = config.get("args", []) or []
    if script.endswith(".py"):
        parts = ["python3", script]
    elif script.endswith(".js"):
        parts = ["node", script]
    elif script.endswith(".sh"):
        parts = ["bash", script]
    else:
        # npm, find, java, .bat, bare executables: run directly.
        parts = [script]
    parts += args
    return " ".join(_q(p) for p in parts)


def _target_program(config: dict, build_dir: str) -> str:
    """Resolve the program path for a target config under ``build_dir``.

    CMake places target binaries in ``<build_dir>/bin`` (CMAKE_RUNTIME_OUTPUT_
    DIRECTORY), so the program is rebased there. Recomputing from ``build_dir``
    (rather than trusting the config's baked-in active-preset path) keeps the
    per-preset variants correct.
    """
    program = config.get("program")
    base = os.path.basename(program) if program else config["target"]
    return os.path.join(build_dir, "bin", base)


def _build_recipe_id(target: str, preset_slug: str) -> str:
    """Id of the per-target cmake build recipe a run recipe depends on."""
    return f"build-{slugify_token(target)}-{preset_slug}"


def _recipe_body(config: dict, source_dir: str, build_dir: str) -> list:
    """Return the indented body lines (without the leading indent) for a config.

    The build prerequisite is a just dependency in the recipe header (see
    _render_recipe), not a body guard, so the artifact is brought up to date
    via an incremental `cmake --build` instead of a one-time existence check.
    """
    cwd = config.get("cwd")
    if "target" in config:
        program = _target_program(config, build_dir)
        # IDE target launches run with cwd = build_dir, so relative args
        # (e.g. the self-reference "../CMakeLists.txt") resolve the same way.
        run_cwd = cwd or build_dir
        args = config.get("args", []) or []
        line = " ".join([_q(program)] + [_q(a) for a in args])
        return [(_cd_prefix(run_cwd) + line).rstrip()]
    cd_prefix = ""
    if cwd and os.path.normpath(cwd) != os.path.normpath(source_dir):
        cd_prefix = _cd_prefix(cwd)
    env_prefix = _env_prefix(config.get("env") or {})
    return [(cd_prefix + env_prefix + _script_invocation(config)).rstrip()]


def _attrs(config: dict) -> list:
    """The attribute lines (doc + optional group) shown above a recipe.

    The group is the semantic category from the config's `group` field (the
    just `[group(...)]`), which is distinct from `folder` (the JetBrains
    folder used to collapse near-identical target variants).
    """
    lines = [_doc_attr(config["name"])]
    if config.get("group"):
        lines.append(_group_attr(config["group"]))
    return lines


def _render_recipe(rid: str, config: dict, source_dir: str, build_dir: str,
                   deps=()) -> list:
    """Render a full generated recipe block (attributes, header, body).

    ``deps`` are recipe ids placed in the header as just dependencies (e.g. the
    build recipe a run/test recipe needs), so they always run first.
    """
    lines = _attrs(config)
    header = f"{rid}:" + ((" " + " ".join(deps)) if deps else "")
    lines.append(header)
    for body_line in _recipe_body(config, source_dir, build_dir):
        lines.append(f"    {body_line}")
    return lines


def _render_delegating_recipe(rid: str, target_rid: str, config: dict) -> list:
    """Render a preset-agnostic recipe that delegates to a per-preset recipe."""
    lines = _attrs(config)
    lines.append(f"{rid}: {target_rid}")
    return lines


# ── justfile parsing (line scanner) ──────────────────────────────────

def _is_comment_or_attr(line: str) -> bool:
    s = line.strip()
    return s.startswith("#") or s.startswith("[")


def _is_recipe_header(line: str) -> bool:
    if not line or line[0] in (" ", "\t"):
        return False
    if _is_comment_or_attr(line):
        return False
    return bool(_RECIPE_HEADER.match(line))


def _recipe_name(line: str):
    m = _RECIPE_NAME.match(line)
    return m.group(1) if m else None


# A generated recipe always carries a [doc("...")] attribute (see _attrs); a
# hand-written user recipe normally does not. We use that to tell a leftover
# generated recipe (e.g. a variant for a valid but non-active preset) apart
# from a user recipe, so the former can be re-sorted while the latter stays put.
_DOC_ATTR = re.compile(r"^\s*\[doc\(")
_GROUP_ATTR = re.compile(r'^\s*\[group\("(.*)"\)\]\s*$')


def _block_has_doc_attr(lines: list) -> bool:
    return any(_DOC_ATTR.match(line) for line in lines)


def _block_group(lines: list) -> str:
    """The [group("...")] value declared in a recipe block, or '' if none."""
    for line in lines:
        m = _GROUP_ATTR.match(line)
        if m:
            return m.group(1).replace('\\"', '"').replace("\\\\", "\\")
    return ""


def _block_deps(lines: list) -> list:
    """Dependency ids from a recipe block's header (the tokens after the ':')."""
    for line in lines:
        if _is_recipe_header(line):
            _, _, rest = line.partition(":")
            return rest.split()
    return []


def _is_body_line(lines: list, idx: int) -> bool:
    line = lines[idx]
    if line.startswith((" ", "\t")):
        return True
    if line.strip() == "":
        j = idx + 1
        while j < len(lines) and lines[j].strip() == "":
            j += 1
        return j < len(lines) and lines[j].startswith((" ", "\t"))
    return False


def _parse_segments(text: str) -> list:
    """Split a justfile into ordered segments.

    Each segment is a dict with kind 'raw' or 'recipe'. Recipe segments carry
    their id and full text (header, body, and the attribute/comment lines
    directly above the header). Ownership is decided by name at merge time, so
    no in-file marker is needed.
    """
    lines = text.splitlines()
    segments = []
    pending = []  # raw lines not yet attached to anything
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        if _is_recipe_header(line):
            # Peel the contiguous comment/attribute lines directly above the
            # header off `pending`; everything earlier is its own raw segment.
            j = len(pending)
            while j > 0 and _is_comment_or_attr(pending[j - 1]):
                j -= 1
            raw_before = pending[:j]
            prefix = pending[j:]
            if raw_before:
                segments.append({"kind": "raw", "lines": raw_before})
            body_end = i + 1
            while body_end < n and _is_body_line(lines, body_end):
                body_end += 1
            block = prefix + lines[i:body_end]
            segments.append({
                "kind": "recipe",
                "lines": block,
                "id": _recipe_name(line),
            })
            pending = []
            i = body_end
        else:
            pending.append(line)
            i += 1
    if pending:
        segments.append({"kind": "raw", "lines": pending})
    return segments


def _is_stale_variant(rid: str, generated_bases, valid_slugs) -> bool:
    """True when ``rid`` is a generated per-preset recipe for a preset that no
    longer exists (e.g. ``version-debug-macos`` after ``debug-macos`` is gone).

    A recipe is stale only when one of the bases we generate prefixes it and
    no base interpretation yields a still-valid preset token. Recipes whose
    preset token is still valid are kept; ids that match no generated base are
    treated as user recipes and preserved.
    """
    matched = False
    for base in generated_bases:
        if rid.startswith(base + "-"):
            token = rid[len(base) + 1:]
            if token in valid_slugs:
                return False
            matched = True
    return matched


# ── preset discovery / default selection ─────────────────────────────

def load_preset_names(source_dir: str) -> list:
    """Read configure preset names from CMakeUserPresets.json (best effort)."""
    from ..core.filesystem import load_json_file
    path = os.path.join(source_dir, "CMakeUserPresets.json")
    data = load_json_file(path) or {}
    return [p.get("name") for p in data.get("configurePresets", []) if p.get("name")]


def pick_default_preset(source_dir: str, active_preset: str) -> str:
    """Choose the preset the preset-agnostic recipes delegate to.

    Reuses the scoring in utils/testing/run_all_tests.py so the default matches what
    `python utils/testing/run_all_tests.py` would pick. Falls back to the active
    preset when scoring is unavailable.
    """
    try:
        runner_path = os.path.join(source_dir, "utils", "testing/run_all_tests.py")
        spec = importlib.util.spec_from_file_location("_mrdocs_run_all_tests", runner_path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        presets = module.load_presets()
        return module.pick_preset(presets, None)
    except Exception:
        return active_preset


def build_dir_for(source_dir: str, active_preset: str, active_build_dir: str,
                  preset: str) -> str:
    """Build directory for a preset, using the convention build/<preset>."""
    if preset == active_preset:
        return active_build_dir
    return os.path.join(source_dir, "build", preset)


def _shell_setting() -> str:
    """Host-appropriate shell setting line."""
    if is_windows():
        return 'set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]'
    return 'set shell := ["bash", "-uc"]'


def _mrdocs_var_line() -> str:
    return f"{_MRDOCS_VAR} := justfile_directory()"


def _preamble() -> list:
    """Header lines emitted when creating a fresh justfile."""
    return [
        "# Generated by mrdocs bootstrap. The recipes below are managed by",
        "# bootstrap and rewritten on each run, matched by name. Add your own",
        "# recipes anywhere and they are preserved. `just --list` shows everything.",
        "",
        _shell_setting(),
        _mrdocs_var_line(),
        "",
    ]


def _unique_targets(configs) -> list:
    """Build targets referenced by target configs or `needs_target`, in order."""
    targets, seen = [], set()
    for c in configs:
        names = []
        if "target" in c:
            names.append(c["target"])
        if c.get("needs_target"):
            names.append(c["needs_target"])
        for t in names:
            if t not in seen:
                seen.add(t)
                targets.append(t)
    return targets


_BUILD_GROUP = "Build"


def _build_generated_recipes(configs, source_dir, build_dir, active_preset,
                             default_preset):
    """Build the fresh recipes and the set of generated bases.

    Returns ``(recipes, generated_bases)`` where each recipe is a dict with
    ``id``, ``block`` (lines), ``group`` (semantic category, '' = ungrouped),
    and ``deps`` (recipe ids it depends on). ``generated_bases`` is the set of
    base ids that have per-preset variants (used to prune stale recipes).
    """
    recipes = []

    def add(rid, block, group="", deps=()):
        recipes.append({"id": rid, "block": block, "group": group, "deps": list(deps)})

    # The default recipe (ungrouped, listed first): bare `just` prints the
    # recipe list in file order rather than alphabetically.
    add("default", [_doc_attr("List available recipes"),
                    "default:", "    @just --list --unsorted"])

    active_slug = slugify_token(active_preset)
    default_slug = slugify_token(default_preset)
    default_build_dir = build_dir_for(source_dir, active_preset, build_dir, default_preset)

    # Presets we emit per-preset recipes for this run: the active one, plus
    # the default delegation target when it differs (so delegation never
    # points at a missing recipe).
    preset_variants = [(active_preset, active_slug, build_dir)]
    if default_preset and default_slug != active_slug:
        preset_variants.append((default_preset, default_slug, default_build_dir))

    targets = _unique_targets(configs)

    # Reserve the cmake recipe ids up front (they are emitted last, but their
    # ids must be reserved so a config that slugs to the same name, e.g. a
    # "MrDocs Build" entry -> "build", is suffixed rather than colliding).
    cmake_ids = {"configure", "build"}
    for _pn, pslug, _bd in preset_variants:
        cmake_ids.add(f"configure-{pslug}")
        cmake_ids.add(f"build-{pslug}")
        for t in targets:
            cmake_ids.add(_build_recipe_id(t, pslug))

    reserved = {"default"} | cmake_ids
    base_pairs = assign_recipe_ids(configs, reserved=reserved)
    name_to_id = {c["name"]: rid for c, rid in base_pairs}

    # Bases that have per-preset variants, for stale-by-name pruning.
    generated_bases = {"configure", "build"}
    generated_bases.update(f"build-{slugify_token(t)}" for t in targets)

    # Config-derived recipes first, so their groups (Test, Run, Documentation,
    # ...) appear before the Build group in first-appearance order.
    for config, base_id in base_pairs:
        group = config.get("group") or ""
        is_aggregate = ("depends" in config and "target" not in config
                        and "script" not in config)
        if is_aggregate:
            # An interface target: depends on other recipes (resolved by name),
            # making the relationship between tasks explicit in the file.
            dep_ids = [name_to_id[d] for d in config["depends"] if d in name_to_id]
            header = f"{base_id}:" + ((" " + " ".join(dep_ids)) if dep_ids else "")
            add(base_id, _attrs(config) + [header], group, dep_ids)
        elif "target" in config:
            generated_bases.add(base_id)
            for preset_name, pslug, bdir in preset_variants:
                rid = f"{base_id}-{pslug}"
                build_dep = _build_recipe_id(config["target"], pslug)
                add(rid, _render_recipe(rid, config, source_dir, bdir, deps=[build_dep]),
                    group, [build_dep])
            add(base_id, _render_delegating_recipe(base_id, f"{base_id}-{default_slug}", config),
                group, [f"{base_id}-{default_slug}"])
        else:
            # Script configs are preset-independent: a single recipe. A
            # `needs_target` becomes a build dependency in the header.
            deps = []
            if config.get("needs_target"):
                deps = [_build_recipe_id(config["needs_target"], default_slug)]
            add(base_id, _render_recipe(base_id, config, source_dir, default_build_dir, deps=deps),
                group, deps)

    # CMake recipes last, so the Build group sorts after the config groups:
    # `just configure`, `just build` (everything), and a per-target
    # `build-<target>` that the run/test recipes depend on. Run/test recipes
    # depend on these so an incremental `cmake --build` keeps the artifact
    # current (rather than a one-time existence check that goes stale).
    for preset_name, pslug, bdir in preset_variants:
        configure_cmd = (f"cmake --preset {preset_name}" if preset_name
                         else f"cmake -S {_q(source_dir)} -B {_q(bdir)}")
        add(f"configure-{pslug}", [
            _doc_attr(f"CMake Configure ({preset_name})"), _group_attr(_BUILD_GROUP),
            f"configure-{pslug}:", f"    {configure_cmd}"], _BUILD_GROUP)
        add(f"build-{pslug}", [
            _doc_attr(f"CMake Build ({preset_name})"), _group_attr(_BUILD_GROUP),
            f"build-{pslug}:",
            f"    {_configure_if_unconfigured(bdir, pslug)}",
            f"    cmake --build {_q(bdir)}"], _BUILD_GROUP, [f"configure-{pslug}"])
        for t in targets:
            bid = _build_recipe_id(t, pslug)
            add(bid, [
                _doc_attr(f"CMake Build {t} ({preset_name})"), _group_attr(_BUILD_GROUP),
                f"{bid}:",
                f"    {_configure_if_unconfigured(bdir, pslug)}",
                f"    cmake --build {_q(bdir)} --target {t}"],
                _BUILD_GROUP, [f"configure-{pslug}"])
    add("configure", [
        _doc_attr("CMake Configure"), _group_attr(_BUILD_GROUP),
        f"configure: configure-{default_slug}"], _BUILD_GROUP, [f"configure-{default_slug}"])
    add("build", [
        _doc_attr("CMake Build"), _group_attr(_BUILD_GROUP),
        f"build: build-{default_slug}"], _BUILD_GROUP, [f"build-{default_slug}"])

    # Replace the absolute repo path with the {{mrdocs}} alias everywhere.
    for r in recipes:
        r["block"] = [_relativize(line, source_dir) for line in r["block"]]
    return recipes, generated_bases


def _order_recipes(recipes, group_order):
    """Order recipes for display: ungrouped first, then groups in
    ``group_order`` (then any extra groups), and within each group a stable
    topological order so a recipe that depends on another comes before it."""
    by_id = {r["id"]: r for r in recipes}
    ungrouped = [r for r in recipes if not r["group"]]
    grouped = {}
    for r in recipes:
        if r["group"]:
            grouped.setdefault(r["group"], []).append(r)

    def topo(members):
        ids = {m["id"] for m in members}
        deps = {m["id"]: [d for d in m["deps"] if d in ids] for m in members}
        indeg = {i: 0 for i in ids}
        for node, ds in deps.items():
            for d in ds:
                indeg[d] += 1  # d is depended on by node
        order = [m["id"] for m in members]
        out, emitted = [], set()

        def sort_key(i):
            # Among eligible recipes, order by most dependencies first, then
            # alphabetically. So umbrella targets (e.g. `test`, `golden-tests`)
            # sit above the zero-dependency leaves they group, and same-prefix
            # recipes still land adjacently. Topology is unaffected: a recipe
            # only becomes eligible once nothing un-emitted depends on it, so a
            # depender still precedes its dependencies.
            return (-len(by_id[i]["deps"]), i)

        while len(emitted) < len(order):
            available = [i for i in order if i not in emitted and indeg[i] == 0]
            if not available:  # cycle guard: emit the rest in input order
                for i in order:
                    if i not in emitted:
                        out.append(by_id[i])
                        emitted.add(i)
                break
            chosen = min(available, key=sort_key)
            out.append(by_id[chosen])
            emitted.add(chosen)
            for d in deps[chosen]:
                indeg[d] -= 1
        return out

    result = list(ungrouped)
    names = [g for g in (group_order or []) if g in grouped]
    names += [g for g in grouped if g not in names]
    for g in names:
        result.extend(topo(grouped[g]))
    return result


def generate_justfile_run_configs(
    configs,
    source_dir,
    build_dir,
    preset,
    dry_run=False,
    ui=None,
    all_presets=None,
    default_preset=None,
    group_order=None,
    justfile_path=None,
):
    """
    Generate (or merge into) a per-machine ``justfile`` at the repo root.

    Every surviving config entry becomes a recipe. Target configs get a
    per-preset recipe for the active preset plus a preset-agnostic recipe that
    delegates to the closest-to-default preset. Ownership is by name: a recipe
    whose id matches a generated one is overwritten, a generated per-preset
    recipe for a removed preset is pruned, and everything else is preserved as
    a user recipe. No in-file marker is used.
    """
    if ui is None:
        ui = get_default_ui()

    if justfile_path is None:
        justfile_path = os.path.join(source_dir, "justfile")

    if all_presets is None:
        all_presets = load_preset_names(source_dir)
    # The active preset is always valid even if CMakeUserPresets.json is absent.
    valid_slugs = {slugify_token(p) for p in all_presets}
    valid_slugs.add(slugify_token(preset))

    if default_preset is None:
        default_preset = pick_default_preset(source_dir, preset)
    # Only delegate to a default whose recipes will exist; otherwise the
    # active preset is the safe choice.
    if slugify_token(default_preset) not in valid_slugs:
        default_preset = preset

    recipes, generated_bases = _build_generated_recipes(
        configs, source_dir, build_dir, preset, default_preset)
    fresh_by_id = {r["id"]: r["block"] for r in recipes}

    existing = ""
    if os.path.exists(justfile_path):
        with open(justfile_path, "r", encoding="utf-8") as f:
            existing = f.read()

    out_blocks = []  # list of list-of-lines, plus the GENERATED sentinel
    # Marks where the ordered block of bootstrap-managed recipes is spliced in.
    # The managed recipes are pulled out of their old positions and re-emitted
    # as one block in sorted order, so the file is fully resorted on each run
    # (in-place name-matching would otherwise freeze their original order).
    GENERATED = object()
    # Generated recipes carried over from a previous run that the current config
    # set does not re-emit (e.g. a variant for a valid but non-active preset).
    # Their bodies are kept, but they join the sorted block so they sit with
    # their same-named siblings instead of trailing wherever they happened to be.
    leftovers = []

    if not existing.strip():
        out_blocks.append(_preamble())
        out_blocks.append(GENERATED)
    else:
        # Ensure the shell setting and the {{mrdocs}} alias exist, adding only
        # whichever is missing so we never duplicate an existing setting.
        missing = []
        if not re.search(r"(?m)^\s*set\s+(windows-)?shell\b", existing):
            missing.append(_shell_setting())
        if not re.search(rf"(?m)^\s*{re.escape(_MRDOCS_VAR)}\s*:=", existing):
            missing.append(_mrdocs_var_line())
        if missing:
            out_blocks.append(missing)

        placed_generated = False

        def reserve_slot():
            nonlocal placed_generated
            if not placed_generated:
                out_blocks.append(GENERATED)
                placed_generated = True

        for seg in _parse_segments(existing):
            if seg["kind"] != "recipe":
                out_blocks.append(seg["lines"])
                continue
            rid = seg["id"]
            if rid in fresh_by_id:
                # A managed recipe: drop it here and reserve the splice point at
                # the first one seen, so all managed recipes re-emit together in
                # sorted order rather than staying frozen in their old spots.
                reserve_slot()
            elif _is_stale_variant(rid, generated_bases, valid_slugs):
                continue                                  # prune stale per-preset recipe
            elif _block_has_doc_attr(seg["lines"]):
                # Leftover generated recipe: keep its body but re-sort it with
                # the rest of the managed block rather than pinning it in place.
                leftovers.append({
                    "id": rid,
                    "block": seg["lines"],
                    "group": _block_group(seg["lines"]),
                    "deps": _block_deps(seg["lines"]),
                })
                reserve_slot()
            else:
                out_blocks.append(seg["lines"])           # hand-written user recipe

        # No managed recipe was present yet (fresh-ish file): append them last.
        if not placed_generated:
            out_blocks.append(GENERATED)

    # Order the managed recipes (fresh + carried-over leftovers) together, then
    # splice the result into the reserved slot.
    ordered = _order_recipes(recipes + leftovers, group_order)
    fresh = [r["block"] for r in ordered]
    expanded = []
    for item in out_blocks:
        if item is GENERATED:
            expanded.extend(fresh)
        else:
            expanded.append(item)

    rendered = _render_blocks(expanded)
    write_text(justfile_path, rendered, dry_run=dry_run, ui=ui)

    if not dry_run and not shutil.which("just"):
        _warn_just_missing(ui)


def _render_blocks(blocks: list) -> str:
    """Join blocks with single blank-line separators and a trailing newline."""
    chunks = []
    for block in blocks:
        text = "\n".join(block).strip("\n")
        if text:
            chunks.append(text)
    return "\n\n".join(chunks) + "\n"


def _warn_just_missing(ui):
    """Print a one-time hint when the `just` command is not installed."""
    ui.info(
        "Generated a justfile, but the `just` command was not found on PATH. "
        "Install it to run recipes (e.g. `brew install just`, `cargo install just`, "
        "`scoop install just`, or see https://just.systems). "
        "Then run `just --list` from the repo root."
    )
