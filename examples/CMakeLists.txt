#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Examples are ordinary consumer code compiled under the project's /W4 /WX. Some
# include mrdocs's public headers, whose tag_invoke overloads (the ADL tag is an
# unreferenced parameter, C4100) and generated schema (C4245) raise the same /W4
# diagnostics the libraries do; the rest hit the usual conversion/shadowing
# noise. Suppress that set here at the examples' directory scope, the same way
# libs/ does for its own code, so it stays out of the root list and off
# mrdocs-core's exported interface. LLVM header noise is not needed: examples do
# not compile LLVM headers.
if (MSVC)
    add_compile_options(/wd4100 /wd4127 /wd4244 /wd4245 /wd4267
        /wd4456 /wd4457 /wd4458 /wd4459 /wd4701 /wd4702 /wd4703)
endif()

# Each example subdirectory owns the tests that exercise it.
add_subdirectory(generators)      # data-driven + script-driven generator examples
add_subdirectory(library)         # standalone programs that link mrdocs-core
add_subdirectory(configuration)   # per-option configuration examples from the docs
add_subdirectory(getting-started) # starter projects invoked as the docs instruct
add_subdirectory(dependencies)    # dependency-handling fixtures
