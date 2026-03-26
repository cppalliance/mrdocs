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

# Generate demos for each (project, variant, generator) combination.
# Expected env vars: GITHUB_EVENT_NAME, RUNNER_OS, GITHUB_ENV
set -x

declare -a generators=("adoc")
if [[ "$GITHUB_EVENT_NAME" != 'pull_request' ]]; then
    generators+=("xml" "html")
fi

demo_failures=""

for variant in single multi; do
    for generator in "${generators[@]}"; do
        [[ $generator = xml && $variant = multi ]] && continue
        [[ $variant = multi ]] && multipage="true" || multipage="false"
        for project_args in \
            "boost-url|$(pwd)/boost/libs/url/doc/mrdocs.yml|../CMakeLists.txt" \
            "beman-optional|$(pwd)/beman-optional/docs/mrdocs.yml|" \
            "nlohmann-json|$(pwd)/nlohmann-json/docs/mrdocs.yml|" \
            "mp-units|$(pwd)/mp-units/docs/mrdocs.yml|" \
            "fmt|$(pwd)/fmt/doc/mrdocs.yml|" \
            "mrdocs|$(pwd)/docs/mrdocs.yml|$(pwd)/CMakeLists.txt" \
        ; do
            IFS='|' read -r project config extra <<< "$project_args"
            outdir="$(pwd)/demos/$project/$variant/$generator"
            cmd=(mrdocs --config="$config" $extra --output="$outdir" --multipage=$multipage --generator="$generator" --log-level=debug)
            if ! "${cmd[@]}"; then
                echo "FAILED: $project/$variant/$generator"
                demo_failures="$demo_failures  $project/$variant/$generator\n    ${cmd[*]}\n"
                rm -rf "$outdir"
            fi
        done
    done

    if [[ "$RUNNER_OS" == 'Linux' ]]; then
        for project in boost-url beman-optional mrdocs fmt nlohmann-json mp-units; do
            root="$(pwd)/demos/$project/$variant"
            src="$root/adoc"
            dst="$root/adoc-asciidoc"
            stylesheet="$(pwd)/share/mrdocs/addons/generator/common/layouts/style.css"

            [[ -d "$src" ]] || continue

            mkdir -p "$dst"

            find "$src" -type f -name '*.adoc' -print0 |
            while IFS= read -r -d '' f; do
                rel="${f#"$src/"}"
                outdir="$dst/$(dirname "$rel")"
                mkdir -p "$outdir"
                asciidoctor -a stylesheet="${stylesheet}" -D "$outdir" "$f"
            done
        done
    fi
done

tar -cjf "$(pwd)/demos.tar.gz" -C "$(pwd)/demos" --strip-components 1 .
echo "demos_path=$(pwd)/demos.tar.gz" >> "$GITHUB_ENV"

if [[ -n "$demo_failures" ]]; then
    echo "The following demos failed:"
    printf "$demo_failures"
    exit 1
fi
