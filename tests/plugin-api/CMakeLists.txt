#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# A module library built and linked the way a plugin is, so that building it
# is the test: see LinkProbe.cpp for what that catches and what it does not.
# It is deliberately not registered with ctest, since it runs nothing.
add_library(mrdocs-plugin-api-link-test MODULE LinkProbe.cpp)
target_link_libraries(mrdocs-plugin-api-link-test PRIVATE mrdocs)
target_compile_features(mrdocs-plugin-api-link-test PRIVATE cxx_std_23)

# The refusal to load a plugin built against another interface version.
# Every other plugin in the tree reports the version its own headers
# define, so nothing else can reach that path.
add_library(mrdocs-plugin-foreign-version MODULE ForeignVersion.cpp)
target_link_libraries(mrdocs-plugin-foreign-version PRIVATE mrdocs)
target_compile_features(mrdocs-plugin-foreign-version PRIVATE cxx_std_23)

# The library goes into an addon root of its own, so that it is offered to
# the run below and to no other test. The generator expression keeps CMake
# from appending a per-configuration subdirectory, which would put the
# library somewhere the test cannot name.
set(MRDOCS_FOREIGN_PLUGIN_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/addons")
set_target_properties(mrdocs-plugin-foreign-version PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "$<1:${MRDOCS_FOREIGN_PLUGIN_ADDONS}/plugins>")

add_test(NAME mrdocs-plugin-foreign-version-refused
    COMMAND
        mrdocs
        "--config=${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml"
        "--output=${CMAKE_CURRENT_BINARY_DIR}/refused-output"
        ${MRDOCS_BUILTIN_DIR_ARGS}
        "--addons-supplemental=${MRDOCS_FOREIGN_PLUGIN_ADDONS}"
)
set_tests_properties(mrdocs-plugin-foreign-version-refused PROPERTIES
    PASS_REGULAR_EXPRESSION "was built against version")
