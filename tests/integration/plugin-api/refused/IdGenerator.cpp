//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin that installs a generator whose id the compile definition
// MRDOCS_TEST_GENERATOR_ID sets. Built more than once with one id, or with
// the id of a generator that ships with MrDocs, the second installation
// fails because the id is taken. The run has to stop and name the library
// it refused.

#include <mrdocs/Config.hpp>
#include <mrdocs/Extensions/GeneratorExtensionContext.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Plugin.hpp>

#ifndef MRDOCS_TEST_GENERATOR_ID
#error "MRDOCS_TEST_GENERATOR_ID names the generator"
#endif

MRDOCS_PLUGIN_MAIN(plugin)
{
    return plugin.registerGenerator(
        MRDOCS_TEST_GENERATOR_ID,
        [](mrdocs::GeneratorExtensionContext const&) -> mrdocs::Expected<void>
        {
            return {};
        },
        "Test generator",
        "txt");
}
