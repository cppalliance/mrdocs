// Copyright (c) 2023 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <mrdox/Plugin.hpp>
#include <mrdox/Version.hpp>


void
MrDoxMain(
    const clang::mrdox::PluginInfo & info)
{
    if (!info.requireVersion(clang::mrdox::projectVersionMajor))
        return;

    clang::mrdox::report::info("It works!");
}
