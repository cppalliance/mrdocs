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

#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/DynamicLibrary.h>

namespace clang {
namespace mrdox {

bool
PluginInfo
::requireVersion(
    int abiVersion_) const
{
    if (abiVersion_ != projectVersionMajor)
    {
        report::warn(
            "Plugin not loaded because it required version {} but tool is version {}",
            abiVersion_,
            projectVersionMajor);
        return false;
    }
    else
        return true;
}


static const PluginInfo pluginInfo{
    .size=sizeof(PluginInfo),
    .abiVersion=projectVersionMajor};

Error loadOnePlugin(
    std::string_view pluginPath_)
{
    std::string pluginPath;
    pluginPath.resize(pluginPath_.size());
    std::transform(pluginPath_.begin(), pluginPath_.end(), pluginPath.begin(), llvm::toLower);

    if (pluginPath.ends_with(".so") || pluginPath.ends_with(".dll"))
    {
        std::string errMsg;
        auto res = llvm::sys::DynamicLibrary::getPermanentLibrary(pluginPath.c_str(), &errMsg);
        if (!res.isValid())
            return formatError("Couldn't load {}, because '{}'", pluginPath, errMsg);

        auto f = res.getAddressOfSymbol("MrDoxMain");
        if (f == nullptr)
            return formatError("{}, doesn't export MrDoxMain symbol", pluginPath);

        // so the user can see which plugin caused a version mismatch
        report::info("Loading plugin {}", pluginPath);
        reinterpret_cast<decltype(&MrDoxMain)>(f)(pluginInfo);
    }
    return Error::success();
}

Error
loadPlugins(
    const std::string & addonsDir)
{
    auto pluginDir = files::appendPath(addonsDir, "plugins");
    return forEachFile(pluginDir, &loadOnePlugin);
}

}
}

