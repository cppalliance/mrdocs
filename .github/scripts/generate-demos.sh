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
# Expected env vars: GITHUB_EVENT_NAME, GITHUB_REPOSITORY, RUNNER_OS, GITHUB_ENV

# Each demo format as "<id>:<multipage>". XML is single-page; HTML and
# AsciiDoc are multipage. XML alone runs the whole extraction phase, which is
# where anything that can crash Mr.Docs lives; the template generators are
# predictable and add no coverage on top of it. Every format is built only
# by push events in the canonical repository (develop, master, release tags),
# and not for testing: that output is what gets published to the demo server
# for people to browse. Pull requests, merge-queue runs, manual dispatches,
# and forks build only XML; on the large demos (LLVM) the extra formats add
# hours, and a merge-queue run has nothing to publish.
quick=true
if [[ "$GITHUB_EVENT_NAME" == 'push' \
   && "${GITHUB_REPOSITORY:-}" == 'cppalliance/mrdocs' ]]; then
    quick=false
fi
if [[ "$quick" == true ]]; then
    formats=("xml:false")
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
    "abseil|$(pwd)/abseil/docs/mrdocs.yml|" \
    "llvm|$(pwd)/llvm/docs/mrdocs.yml|" \
    "bitcoin|$(pwd)/bitcoin/docs/mrdocs.yml|" \
    "folly|$(pwd)/folly/docs/mrdocs.yml|" \
    "openssl|$(pwd)/openssl/docs/mrdocs.yml|" \
    "bde|$(pwd)/bde/docs/mrdocs.yml|" \
    "mrdocs|$(pwd)/docs/mrdocs.yml|$(pwd)/CMakeLists.txt" \
; do
    # Split the packed fields and pick this project's output root.
    IFS='|' read -r project config extra <<< "$project_args"
    base="$(pwd)/demos/$project"

    # LLVM and BDE take tens of minutes just to extract; on pull requests and
    # forks that is all cost and no coverage the smaller demos don't provide.
    if [[ "$quick" == true && ( "$project" == llvm || "$project" == bde ) ]]; then
        echo "Skipping $project (quick mode)"
        continue
    fi

    # Set each generator's output directory and pagination explicitly.
    options=()
    for format in "${formats[@]}"; do
        id="${format%%:*}"
        page="${format##*:}"
        options+=("--generator-options.${id}.output=${base}/${id}")
        options+=("--generator-options.${id}.multipage=${page}")
    done

    # Run mrdocs once for this project, emitting every format in one pass.
    # Each project gets one log group covering generation and, on Linux, the
    # asciidoctor rendering of the generated AsciiDoc; failures are echoed
    # outside the group so they stay visible without expanding it. Warnings
    # are off: the demos exist to prove the libraries render, and nobody acts
    # on documentation warnings from here. Command echoing is enabled only
    # around the mrdocs invocation; tracing everything (the per-file
    # asciidoctor loop especially) makes the log so large GitHub truncates it.
    echo "::group::Generate $project ($generators)"
    generated=true
    cmd=(mrdocs --config="$config" $extra --generator="$generators" "${options[@]}" --warnings=false --log-level=info)
    set -x
    "${cmd[@]}" || generated=false
    { set +x; } 2>/dev/null
    if ! $generated; then
        echo "::endgroup::"
        echo "FAILED: $project ($generators)"
        demo_failures="$demo_failures  $project ($generators)\n    ${cmd[*]}\n"
        rm -rf "$base"
        continue
    fi

    # On Linux, render the generated AsciiDoc to HTML with asciidoctor.
    if [[ "$RUNNER_OS" == 'Linux' && -d "$base/adoc" ]]; then
        src="$base/adoc"
        dst="$base/adoc-asciidoc"
        stylesheet="$(pwd)/data/mrdocs/addons/generator/common/layouts/style.css"

        mkdir -p "$dst"

        # Mirror the AsciiDoc tree, rendering each file in place.
        echo "Rendering $src with asciidoctor"
        find "$src" -type f -name '*.adoc' -print0 |
        xargs -0 -n 500 -P "$(nproc)" \
            asciidoctor -a stylesheet="${stylesheet}" -R "$src" -D "$dst"
    fi
    echo "::endgroup::"
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
