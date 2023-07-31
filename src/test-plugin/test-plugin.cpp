// Copyright (c) 2023 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <mrdox/Plugin.hpp>
#include <mrdox/Version.hpp>


bool
MrDoxMain(
    int versionMajor,
    int versionMinor,
    clang::mrdox::PluginEnvironment & env)
{
    if (versionMajor != clang::mrdox::projectVersionMajor)
        return false;
    return true;
}