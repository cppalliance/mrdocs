#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# In the MrDocs build the plugin links the tool that was just built. Out of
# tree it is the same library built the way a plugin author builds one,
# against an installed MrDocs; tests/cmake covers that.
if (MRDOCS_BUILD_PLUGIN_MODULES)
    add_library(mrdocs-stats-plugin-example MODULE plugin.cpp)
    target_link_libraries(mrdocs-stats-plugin-example PRIVATE mrdocs)
    target_compile_features(mrdocs-stats-plugin-example PRIVATE cxx_std_23)

    # MrDocs looks for plugins in the `plugins` subdirectory of an addon root,
    # so the library is built into one. The generator expression keeps CMake
    # from appending a per-configuration subdirectory, which would put the
    # library somewhere the test cannot name.
    set(MRDOCS_STATS_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/addons")
    set_target_properties(mrdocs-stats-plugin-example PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "$<1:${MRDOCS_STATS_ADDONS}/plugins>")

    # Documenting the sample project is the test: its configuration asks for
    # the `stats` generator, which exists only if the plugin was found,
    # loaded, and installed it.
    add_test(NAME mrdocs-plugin-example-stats
        COMMAND
            mrdocs
            "--config=${CMAKE_CURRENT_SOURCE_DIR}/sample-project/mrdocs.yml"
            "--output=${CMAKE_CURRENT_BINARY_DIR}/reference-output"
            ${MRDOCS_BUILTIN_DIR_ARGS}
            "--addons-supplemental=${MRDOCS_STATS_ADDONS}"
            --log-level=warn
    )
endif ()
