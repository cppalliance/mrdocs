#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Generator examples. Each runs with the example's own configuration, and the
# output it commits, which the docs include, is compared with what the run
# writes.

# Data-driven generators: examples/generators/data-driven/<id> demonstrates a
# Handlebars generator defined entirely in an addon. The committed output is
# named after the example's source rather than the generator's
# `reference.<id>`.
foreach (data_driven IN ITEMS md tex jsonl)
    mrdocs_add_example(
        DIRECTORY data-driven/${data_driven}
        CONFIG mrdocs.yml ARGS simple.cpp --log-level=warn
        OUTPUTS simple.${data_driven}=reference.${data_driven})
endforeach ()

# Script-driven generator examples.
mrdocs_add_example(DIRECTORY script-driven/search-index CONFIG mrdocs.yml
    OUTPUTS search-index.json)
mrdocs_add_example(DIRECTORY script-driven/json CONFIG mrdocs.yml
    OUTPUTS reference.json)
mrdocs_add_example(DIRECTORY script-driven/toc CONFIG mrdocs.yml
    OUTPUTS toc.adoc toc.html toc.md)
mrdocs_add_example(DIRECTORY script-driven/describe CONFIG mrdocs.yml
    OUTPUTS circle.described.hpp level.described.hpp
            shape.described.hpp vec2.described.hpp)
# snippets runs two generators: the reference comes from the adoc one, and
# the exported examples from the snippets one.
mrdocs_add_example(DIRECTORY script-driven/snippets CONFIG mrdocs.yml
    OUTPUTS reference.adoc=adoc/reference.adoc
            exported/arithmetic.cpp exported/text.cpp)

# No-op generator examples (the documentation check and the AI repair loop).
# Each owns its own CMakeLists, so add the directory rather than a test here.
add_subdirectory(noop)
