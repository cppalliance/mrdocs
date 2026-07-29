#!/usr/bin/env bash
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Generate demos for each library and format.
# Expected env vars: GITHUB_EVENT_NAME, RUNNER_OS, GITHUB_ENV

# Echo each command as it runs.
set -x

# Each demo format as "<id>:<multipage>". XML is single-page; HTML and
# AsciiDoc are multipage. Pull requests only build AsciiDoc to keep CI fast.
if [[ "$GITHUB_EVENT_NAME" == 'pull_request' ]]; then
    formats=("adoc:true")
else
    formats=("xml:false" "html:true" "adoc:true")
fi

# Join the format ids into the comma-separated list mrdocs expects.
generator_ids=()
for format in "${formats[@]}"; do
    generator_ids+=("${format%%:*}")
done
generators=$(IFS=,; echo "${generator_ids[*]}")

# Collect the names of any demos that fail to build.
demo_failures=""

# Each project: display name, its mrdocs config, and an optional extra input.
for project_args in \
    "boost-url|$(pwd)/boost/libs/url/doc/mrdocs.yml|../CMakeLists.txt" \
    "beman-optional|$(pwd)/beman-optional/docs/mrdocs.yml|" \
    "nlohmann-json|$(pwd)/nlohmann-json/docs/mrdocs.yml|" \
    "mp-units|$(pwd)/mp-units/docs/mrdocs.yml|" \
    "fmt|$(pwd)/fmt/doc/mrdocs.yml|" \
    "mrdocs|$(pwd)/docs/mrdocs.yml|$(pwd)/CMakeLists.txt" \
; do
    # Split the packed fields and pick this project's output root.
    IFS='|' read -r project config extra <<< "$project_args"
    base="$(pwd)/demos/$project"

    # Set each generator's output directory and pagination explicitly.
    options=()
    for format in "${formats[@]}"; do
        id="${format%%:*}"
        page="${format##*:}"
        options+=("--generator-options.${id}.output=${base}/${id}")
        options+=("--generator-options.${id}.multipage=${page}")
    done

    # Run mrdocs once for this project, emitting every format in one pass.
    cmd=(mrdocs --config="$config" $extra --generator="$generators" "${options[@]}" --log-level=debug)
    if ! "${cmd[@]}"; then
        echo "FAILED: $project ($generators)"
        demo_failures="$demo_failures  $project ($generators)\n    ${cmd[*]}\n"
        rm -rf "$base"
        continue
    fi

    # On Linux, render the generated AsciiDoc to HTML with asciidoctor.
    if [[ "$RUNNER_OS" == 'Linux' ]]; then
        src="$base/adoc"
        dst="$base/adoc-asciidoc"
        stylesheet="$(pwd)/data/mrdocs/addons/generator/common/layouts/style.css"

        [[ -d "$src" ]] || continue

        mkdir -p "$dst"

        # Mirror the AsciiDoc tree, rendering each file in place.
        find "$src" -type f -name '*.adoc' -print0 |
        while IFS= read -r -d '' f; do
            rel="${f#"$src/"}"
            outdir="$dst/$(dirname "$rel")"
            mkdir -p "$outdir"
            asciidoctor -a stylesheet="${stylesheet}" -D "$outdir" "$f"
        done
    fi
done

# Archive all demos and hand the path back to the workflow.
tar -cjf "$(pwd)/demos.tar.gz" -C "$(pwd)/demos" --strip-components 1 .
echo "demos_path=$(pwd)/demos.tar.gz" >> "$GITHUB_ENV"

# Fail the job if any demo failed, listing each one.
if [[ -n "$demo_failures" ]]; then
    echo "The following demos failed:"
    printf "$demo_failures"
    exit 1
fi
