//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Plugin.hpp>
#include <mrdox/Version.hpp>
#include <mrdox/Support/Path.hpp>

#include "lib/Support/GeneratorsImpl.hpp"

#include <llvm/Support/DynamicLibrary.h>

namespace clang {
namespace mrdox {

class PluginEnvironmentImpl final : public PluginEnvironment
{
public:
    std::vector<Error> errors;

    void
    reportError(
        Error error) override
    {
        errors.push_back(error);
    }
    void
    addGenerator(
        std::unique_ptr<Generator> generator) override
    {
        getGeneratorsImpl().insert(std::move(generator));
    }
};

Error loadOnePlugin(
    std::string pth,
    PluginEnvironment & env)
{
    if (pth.ends_with(".so") || pth.ends_with(".dll"))
    {
        std::string errMsg;
        auto res = llvm::sys::DynamicLibrary::getPermanentLibrary(pth.c_str(), &errMsg);
        if (!res.isValid())
            return formatError("Couldn't load {}, because '{}'", pth, errMsg);

        auto f = res.getAddressOfSymbol("MrDoxMain");
        if (f == nullptr)
            return formatError("{}, doesn't export MrDoxMain symbol", pth);

        auto func = reinterpret_cast<decltype(&MrDoxMain)>(f);
        if (!func(projectVersionMajor, projectVersionMinor, env))
            return formatError("Couldn't load {} - version mismatch.", pth);
    }
    return Error::success();
}

Error
loadPlugins(
    const std::string & addonsDir)
{
    auto pluginDir = files::appendPath(addonsDir, "plugins");
    PluginEnvironmentImpl env;
    auto err = forEachFile(
        pluginDir,
        [&](std::string_view path) -> Error
        {
            return loadOnePlugin(std::string(path), env);
        });

    if (err.failed())
        env.errors.push_back(std::move(err));

    if (env.errors.empty())
        return Error::success();
    return env.errors;
}

}
}

