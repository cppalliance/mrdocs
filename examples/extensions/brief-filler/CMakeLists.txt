#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# The name of the plugin directory, `brief-filler`, is the name the extension
# loads under. The transform changes the corpus every generator sees, so the XML
# generator, run next to the plugin, has to show the brief it added.
mrdocs_add_extension_example(brief-filler MODE xml-check
    PLUGIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/plugin.cpp"
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/sample-project/mrdocs.yml"
    CHECK "${CMAKE_CURRENT_SOURCE_DIR}/check-brief.cmake")
