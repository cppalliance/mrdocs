#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

set(MRDOCS_SELF_DOC_OUTPUT "${CMAKE_BINARY_DIR}/docs/self-reference")
# Self-documentation test: run mrdocs over its own build file with the noop
# generator (warn-as-error toggled by the strict flag). The addons check the
# project's own rules.
set(MRDOCS_SELF_DOC_ARGS
    "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
    "--config=${CMAKE_SOURCE_DIR}/docs/mrdocs.yml"
    ${MRDOCS_BUILTIN_DIR_ARGS}
    "--addons-supplemental=${CMAKE_CURRENT_SOURCE_DIR}/addons"
    # The export-marker check reads the visibility attribute that MRDOCS_DECL
    # and its dom and handlebars twins expand to, so the run keeps
    # non-standard attributes.
    --non-standard-attributes=true
    --file-patterns=*.hpp
    # The check also reads the .ipp files, so a function declared in a header
    # and defined inline in an .ipp counts as defined in the headers.
    --file-patterns=*.ipp
    --generator=noop
    --log-level=debug
    $<$<BOOL:${MRDOCS_BUILD_STRICT_TESTS}>:--warn-as-error=true>)
add_test(NAME mrdocs-self-doc
    COMMAND mrdocs ${MRDOCS_SELF_DOC_ARGS} "--output=${MRDOCS_SELF_DOC_OUTPUT}")

# The export-marker check also needs the private members: one with its body in
# a source file is called by the inline functions of its class and needs the
# marker. This run extracts them, so the documentation warnings, which are
# enforced by the run above, are off. The check does nothing in a run that has
# no private members.
add_test(NAME mrdocs-self-doc-private
    COMMAND mrdocs ${MRDOCS_SELF_DOC_ARGS}
        "--output=${MRDOCS_SELF_DOC_OUTPUT}-private"
        --extract-private=true
        --warn-if-undocumented=false
        --warn-no-paramdoc=false
        --warn-unnamed-param=false
        --warn-if-undoc-enum-val=false)

# The export-marker check run over the cases in export-markers/: the errors it
# reports must be the ones in expected.txt, and a MRDOCS_VISIBLE on a member or
# a function must stop the parse. The script joins the arguments with `|`,
# since a list does not survive a command line.
set(MRDOCS_EXPORT_MARKERS_ARGS
    ${MRDOCS_BUILTIN_DIR_ARGS}
    "--config=${CMAKE_CURRENT_SOURCE_DIR}/export-markers/mrdocs.yml"
    "--addons-supplemental=${CMAKE_CURRENT_SOURCE_DIR}/addons"
    --non-standard-attributes=true
    --extract-private=true
    --generator=noop)
list(JOIN MRDOCS_EXPORT_MARKERS_ARGS "|" MRDOCS_EXPORT_MARKERS_ARGS)
add_test(NAME mrdocs-self-doc-export-markers
    COMMAND ${CMAKE_COMMAND}
        "-DMRDOCS=$<TARGET_FILE:mrdocs>"
        "-DMRDOCS_ARGS=${MRDOCS_EXPORT_MARKERS_ARGS}"
        "-DCASES_DIR=${CMAKE_CURRENT_SOURCE_DIR}/export-markers"
        "-DOUTPUT_DIR=${CMAKE_BINARY_DIR}/docs/self-reference-export-markers"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/export-markers/CheckExportMarkers.cmake")
