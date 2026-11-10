#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Included from the root early, before add_subdirectory(utils), because
# set_ternary is used there.
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/helpers/set_ternary.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/mrdocs_flags_match.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/mrdocs_detect_stdlib_tag.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/mrdocs_static_link_flags.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/mrdocs_collect_static_libs.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/mrdocs_hide_symbols.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/mrdocs_add_hidden_archives.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/mrdocs_target_link_whole_libraries.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/mrdocs_target_plugin_host_compile_definitions.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/mrdocs_add_plugin_module.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/mrdocs_add_checked_test.cmake")
