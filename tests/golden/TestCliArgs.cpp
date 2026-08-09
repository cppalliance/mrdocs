//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "TestCliArgs.hpp"
#include <mrdocs/ConfigSchema.hpp>
#include <string_view>

namespace mrdocs {

TestCliArgs testCliArgs;

namespace {

// Whether `key` names a configuration option, and whether it takes a value.
// The harness leaves configuration options in argv for Config::load, but it
// still needs to know a value-bearing one so `--opt value` is not read as a
// positional test path.
bool
isConfigOptionKey(std::string_view const key)
{
    for (auto const& info : ConfigSchema::commandLineOptionInfos())
    {
        if (info.name == key)
        {
            return true;
        }
    }
    return false;
}

bool
configOptionTakesValue(std::string_view const key)
{
    for (auto const& info : ConfigSchema::commandLineOptionInfos())
    {
        if (info.name == key)
        {
            return info.takesValue;
        }
    }
    return false;
}

} // (anon)

TestCliArgs
parseTestCommandLine(int argc, char const** argv)
{
    TestCliArgs args;
    for (int i = 1; i < argc; ++i)
    {
        std::string_view const arg(argv[i]);
        if (arg == "--help" || arg == "-h")
        {
            args.showHelp = true;
            continue;
        }
        if (!arg.starts_with("--"))
        {
            args.inputs.emplace_back(arg);
            continue;
        }
        std::string_view const body = arg.substr(2);
        auto const eq = body.find('=');
        bool const hasValue = eq != std::string_view::npos;
        std::string_view const key = hasValue ? body.substr(0, eq) : body;
        auto const valueOrNext = [&]() -> std::string
        {
            if (hasValue)
            {
                return std::string(body.substr(eq + 1));
            }
            if (i + 1 < argc)
            {
                return std::string(argv[++i]);
            }
            return {};
        };

        // Harness-specific options.
        if (key == "action")
        {
            std::string const value = valueOrNext();
            if (value == "create")
            {
                args.action = Action::create;
            }
            else if (value == "update")
            {
                args.action = Action::update;
            }
            else
            {
                args.action = Action::test;
            }
            continue;
        }
        if (key == "bad")
        {
            args.bad = true;
            continue;
        }
        if (key == "force")
        {
            args.force = true;
            continue;
        }
        if (key == "log-level")
        {
            // Also a configuration option, but the harness reads it here to
            // set its own reporting level.
            args.logLevel = valueOrNext();
            continue;
        }

        // A dotted object override (`--generator-options.x=y`) is a config
        // override applied later by Config::load; skip it here.
        if (key.find('.') != std::string_view::npos)
        {
            continue;
        }
        // Other configuration options are applied later by Config::load; skip
        // them here, consuming a `--opt value` token so it is not read as a
        // positional test path.
        if (isConfigOptionKey(key))
        {
            if (configOptionTakesValue(key) && !hasValue && i + 1 < argc)
            {
                ++i;
            }
            continue;
        }
        // Unknown options are ignored by the harness.
    }
    return args;
}

} // mrdocs
