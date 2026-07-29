#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Handlebars golden/reference tests: render fixture templates and run the
# mustache spec fixtures. Uses LLVM (JSON parsing, file reading), so this is the
# only handlebars test target that links LLVM; the library and the unit tests do
# not. It therefore finds LLVM itself (find_package is cached, so src/ finding
# it again for mrdocs-core is a no-op) rather than relying on a dependency found
# elsewhere.
find_package(LLVM CONFIG REQUIRED)
file(GLOB_RECURSE HANDLEBARS_GOLDEN_TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(mrdocs-handlebars-golden-tests ${HANDLEBARS_GOLDEN_TEST_SOURCES})
target_link_libraries(mrdocs-handlebars-golden-tests
        PRIVATE mrdocs::handlebars test_suite LLVMSupport)
target_include_directories(mrdocs-handlebars-golden-tests
        SYSTEM PRIVATE "${LLVM_INCLUDE_DIRS}")
target_compile_definitions(mrdocs-handlebars-golden-tests
        PRIVATE -DMRDOCS_TEST_FILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME mrdocs-handlebars-golden-tests COMMAND mrdocs-handlebars-golden-tests)
