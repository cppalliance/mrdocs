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
mrdocs_add_plugin_module(mrdocs-plugin-api-link-test LinkProbe.cpp)

# The in-tree directory flags, with | between them for ExpectFailure.cmake,
# which splits them again: a ; would split the -D definition carrying them.
string(REPLACE ";" "|" MRDOCS_PLUGIN_TEST_DIR_ARGS "${MRDOCS_BUILTIN_DIR_ARGS}")

# Register a test that runs MrDocs with the addon root `addons` and passes
# only if the run fails with output matching `expected`.
function(mrdocs_add_plugin_failure_test name addons expected)
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            "-DMRDOCS=$<TARGET_FILE:mrdocs>"
            "-DCONFIG=${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml"
            "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${name}-output"
            "-DADDONS=${addons}"
            "-DBUILTIN_ARGS=${MRDOCS_PLUGIN_TEST_DIR_ARGS}"
            "-DEXPECTED=${expected}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ExpectFailure.cmake")
endfunction()

# A plugin MrDocs has to refuse, built from `source` into an addon root of
# its own, so that it is offered to its own test and to no other. Every
# other plugin in the tree reports what its headers and compiler define, so
# nothing else reaches these paths.
function(mrdocs_add_refused_plugin name source expected)
    set(addons "${CMAKE_CURRENT_BINARY_DIR}/${name}")
    mrdocs_add_plugin_module(mrdocs-plugin-${name} ${source} ADDONS "${addons}")
    mrdocs_add_plugin_failure_test(
        mrdocs-plugin-${name}-refused "${addons}" "${expected}")
endfunction()

mrdocs_add_refused_plugin(foreign-version ForeignVersion.cpp
    "was built against version")
mrdocs_add_refused_plugin(foreign-tag ForeignTag.cpp
    "was built with .foreign-toolchain.")
mrdocs_add_refused_plugin(missing-main MissingMain.cpp
    "does not export mrdocs_plugin_main")

# A plugin with a shared library of its own, kept in the subdirectory of the
# plugins directory named after the plugin, which the search for plugins
# skips. The plugin finds it through its run path on Linux and macOS, and
# on Windows because MrDocs searches that subdirectory while it loads the
# plugin. The entry point fails on purpose, with the value the library
# returned, which stops the run before it needs any input, and shows both
# that the library was found and that the error names the plugin.
set(dependent_addons "${CMAKE_CURRENT_BINARY_DIR}/with-dependency")
set(dependency_dir "${dependent_addons}/plugins/with-dependency")
add_library(mrdocs-plugin-dependency SHARED Dependency.cpp)
set_target_properties(mrdocs-plugin-dependency PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    RUNTIME_OUTPUT_DIRECTORY "$<1:${dependency_dir}>"
    LIBRARY_OUTPUT_DIRECTORY "$<1:${dependency_dir}>")

set_ternary(dependency_rpath APPLE
    "@loader_path/with-dependency" "$ORIGIN/with-dependency")
mrdocs_add_plugin_module(mrdocs-plugin-with-dependency WithDependency.cpp
    ADDONS "${dependent_addons}")
target_link_libraries(mrdocs-plugin-with-dependency
    PRIVATE mrdocs-plugin-dependency)
set_target_properties(mrdocs-plugin-with-dependency PROPERTIES
    PREFIX ""
    OUTPUT_NAME with-dependency
    INSTALL_RPATH "${dependency_rpath}"
    BUILD_WITH_INSTALL_RPATH ON)
mrdocs_add_plugin_failure_test(mrdocs-plugin-with-dependency-fails
    "${dependent_addons}"
    "with-dependency\\.[a-z]+. failed: the dependency answered 42")

# The same plugin and library, loaded from an addon root whose path is not
# ASCII: on Windows, a path read in the ANSI code page names another
# directory, and the plugin then fails to load instead of failing on purpose.
add_test(NAME mrdocs-plugin-non-ascii-path-fails
    COMMAND ${CMAKE_COMMAND}
        "-DMRDOCS=$<TARGET_FILE:mrdocs>"
        "-DFROM=${dependent_addons}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/non-ascii-path"
        "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DBUILTIN_ARGS=${MRDOCS_PLUGIN_TEST_DIR_ARGS}"
        "-DEXPECTED=with-dependency\\.[a-z]+. failed: the dependency answered 42"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/NonAsciiPath.cmake")
