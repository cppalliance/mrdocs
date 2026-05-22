//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "AddonDiscovery.hpp"

#include <lib/Support/AddonRoots.hpp>

#include <mrdocs/Support/Path.hpp>

#include <algorithm>

namespace mrdocs {

Expected<std::vector<std::string>>
collectExtensionScripts(Config const& config)
{
    std::vector<std::string> scripts;
    std::vector<std::string> const roots = addonRoots(config);
    for (std::string const& root : roots)
    {
        std::string const dir = files::appendPath(root, "extensions");
        if (files::exists(dir))
        {
            Expected<void> exp = forEachFile(dir, true,
                [&](std::string_view pathName) -> Expected<void>
                {
                    if (pathName.ends_with(".lua") ||
                        pathName.ends_with(".js"))
                    {
                        scripts.emplace_back(pathName);
                    }
                    return {};
                });
            if (!exp)
            {
                return Unexpected(exp.error());
            }
        }
    }
    std::sort(scripts.begin(), scripts.end());
    return scripts;
}

} // mrdocs
