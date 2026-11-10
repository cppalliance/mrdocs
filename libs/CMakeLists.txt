#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Self-contained libraries, each with its own include/ + src/ (+ tests/) and its
# own CMakeLists. These depend ONLY on the standard library, on the `polyfills`
# library (standard-library features not yet available on every supported
# compiler), and, where noted, on each other. They never depend on mrdocs-core.
# See docs/modules/ROOT/pages/contribute/layout/libs.adoc for the
# polyfills-vs-Support distinction and the dependency rule.

# Header-only libraries are INTERFACE targets; compiled libraries are STATIC.
# (dom / handlebars are added here as they are extracted.)

# These libraries inherit the global compile flags (including -Werror / -Wall on
# Clang and GCC, /W4 /WX on MSVC) from the parent scope, but not the per-target
# options mrdocs-core sets for its own sources, so the relevant ones are set
# here.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # mrdocs-core suppresses covered-switch-default for its intentional covered
    # switches; do the same so the decoupled libraries build under -Werror too.
    add_compile_options(-Wno-covered-switch-default)
elseif (MSVC)
    # <chrono> and other standard headers use exceptions, and without /EHs MSVC
    # raises C4530 (fatal under /WX). It matches mrdocs-core's option.
    add_compile_options(/EHs)
    # Conformance mode, so /W4 diagnostics match those under which mrdocs-core
    # builds this same code.
    add_compile_options(/permissive-)
    # A parallel build, as in mrdocs-core.
    add_compile_options(/MP)
    # /W4 diagnostics these libraries' own std-based code raises. Unlike the LLVM
    # header suppressions (which ride on mrdocs-llvm in src/), these are about our
    # code, so they belong with the libraries. Kept narrow on purpose: C4100
    # unreferenced parameter, C4244/C4245/C4267 conversions, C4127 constant
    # condition, C4456/C4457/C4458/C4459 shadowing, C4701/C4702/C4703 flow.
    add_compile_options(/wd4100 /wd4127 /wd4244 /wd4245 /wd4267
        /wd4456 /wd4457 /wd4458 /wd4459 /wd4701 /wd4702 /wd4703)
    # A class marked MRDOCS_DECL can hold a std:: member (C4251: Error has a
    # std::string) or derive from a std:: type (C4275: Exception derives from
    # std::exception). MSVC warns because a client with another standard library
    # would see another layout. The plugin loader refuses such a plugin: it
    # checks the build id and standard-library tag. So the warnings are turned
    # off. A program that links mrdocs-core directly has no such check and must
    # use the same compiler and standard library as the core.
    add_compile_options(/wd4251 /wd4275)
    # These libraries never link LLVM, so they set the Windows defines the
    # global add_definitions(${LLVM_DEFINITIONS}) gives mrdocs-core directly.
    # _CRT_SECURE_NO_WARNINGS accepts the standard C functions MSVC deprecates
    # (e.g. std::localtime, C4996).
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # Quiets the C++20 <ciso646> removal warning.
    add_compile_definitions(_SILENCE_CXX20_CISO646_REMOVED_WARNING)
endif()

# The unit-test framework is only needed when building tests. Added before the
# libraries below so their own test executables can link it.
if (MRDOCS_BUILD_TESTS)
    add_subdirectory(test_suite)
endif()

# Standard-library polyfills (header-only INTERFACE library). std-only.
add_subdirectory(polyfill)

# The document object model (compiled STATIC library). std + polyfills only.
add_subdirectory(dom)

# The Handlebars template engine (compiled STATIC library). std + polyfills + dom.
add_subdirectory(handlebars)
