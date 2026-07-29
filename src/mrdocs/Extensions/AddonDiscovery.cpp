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
#include "../Support/AddonRoots.hpp"
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <algorithm>
#include <filesystem>
#include <utility>

namespace mrdocs {

namespace {

// Append the entry script for one immediate child of an extensions/
// directory: a .lua or .js file is an extension. Anything else is
// ignored.
void
collectEntry(
    std::filesystem::directory_entry const& entry,
    std::vector<std::string>& scripts)
{
    std::error_code ec;
    if (entry.is_regular_file(ec))
    {
        std::string path = entry.path().string();
        if (path.ends_with(".lua") || path.ends_with(".js"))
        {
            scripts.push_back(std::move(path));
        }
    }
}

// Append every entry script found in one extensions/ directory to
// `scripts`, returning any filesystem error hit while iterating.
Expected<void>
collectFromDirectory(
    std::string const& dir,
    std::vector<std::string>& scripts)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::directory_iterator const end{};
    for (fs::directory_iterator it(dir, ec);
         !ec && it != end;
         it.increment(ec))
    {
        collectEntry(*it, scripts);
    }
    Expected<void> result;
    if (ec)
    {
        result = Unexpected(formatError("{}: {}", dir, ec.message()));
    }
    return result;
}

} // (anon)

Expected<std::vector<std::string>>
collectExtensionScripts(Config const& config)
{
    std::vector<std::string> scripts;
    Expected<void> status;
    std::vector<std::string> const roots = addonRoots(config);
    for (std::string const& root : roots)
    {
        std::string const dir = files::appendPath(root, "extensions");
        if (status.has_value() && files::exists(dir))
        {
            status = collectFromDirectory(dir, scripts);
        }
    }

    Expected<std::vector<std::string>> result;
    if (status.has_value())
    {
        std::sort(scripts.begin(), scripts.end());
        result = std::move(scripts);
    }
    else
    {
        result = Unexpected(status.error());
    }
    return result;
}

} // mrdocs
