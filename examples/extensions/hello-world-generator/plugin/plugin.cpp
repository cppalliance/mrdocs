//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The hello-world generator of the Defining Extensions page, the same
// extension as its JavaScript and Lua tabs: it writes hello.txt.

// tag::plugin[]
#include <mrdocs/Extensions/GeneratorExtensionContext.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Plugin.hpp>
#include <filesystem>
#include <fstream>

MRDOCS_PLUGIN_MAIN(plugin)
{
    return plugin.registerGenerator("hello-world-generator",
        [](mrdocs::GeneratorExtensionContext const& ctx) -> mrdocs::Expected<void>
        {
            std::filesystem::path const dir(ctx.outputPath);
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            std::ofstream os(dir / "hello.txt");
            if (ec || !os)
            {
                return mrdocs::Unexpected(mrdocs::Error(
                    "could not write hello.txt"));
            }
            os << "Hello from a generator\n";
            return {};
        });
}
// end::plugin[]
