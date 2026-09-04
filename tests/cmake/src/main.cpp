//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Config.hpp>
#include <mrdocs/Engines/JavaScript.hpp>
#include <mrdocs/Engines/Lua.hpp>
#include <iostream>

int
main()
{
    // Exercises mrdocs and all its bundled private dependencies, forcing
    // the linker to resolve those symbols out of the archive.

    // LLVM / Clang: mrdocs's configuration loader parses YAML with LLVM.
    mrdocs::Config config;
    if (!mrdocs::Config::load(config, "", nullptr))
    {
        std::cerr << "Config::load failed\n";
        return 1;
    }

    // JerryScript: compile and run a trivial script.
    mrdocs::js::Context jsContext;
    mrdocs::js::Scope jsScope(jsContext);
    if (!jsScope.script("var x = 1 + 1;"))
    {
        std::cerr << "JavaScript engine failed\n";
        return 1;
    }

    // Lua: compile a trivial chunk.
    mrdocs::lua::Context luaContext;
    mrdocs::lua::Scope luaScope(luaContext);
    if (!luaScope.loadChunk("return 1 + 1"))
    {
        std::cerr << "Lua engine failed\n";
        return 1;
    }

    std::cout << "mrdocs bundled dependencies (LLVM, Clang, JerryScript, Lua) "
                 "linked and ran from a consumer\n";
    return 0;
}
