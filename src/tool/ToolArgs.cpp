//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ToolArgs.hpp"
#include <lib/Support/Path.hpp>
#include <cstddef>
#include <ranges>
#include <string>
#include <vector>


namespace mrdocs {

ToolArgs ToolArgs::instance_;

//------------------------------------------------

ToolArgs::
ToolArgs()
    : PublicToolArgs()
    , usageText("Generate C++ reference documentation")
    , extraHelp(
R"(
EXAMPLES:
    mrdocs
    mrdocs docs/mrdocs.yml
    mrdocs docs/mrdocs.yml ../build/compile_commands.json
)")
{}

void
ToolArgs::
hideForeignOptions()
{
    std::vector<llvm::cl::Option const*> oursOptions;
    this->visit([&](
        std::string_view, auto const& opt)
    {
        oursOptions.push_back(std::addressof(opt));
    });
    auto optionMap = llvm::cl::getRegisteredOptions();
    for(auto& opt : optionMap)
    {
        opt.getValue()->setHiddenFlag(
            std::ranges::find(oursOptions, opt.getValue()) != oursOptions.end() ?
            llvm::cl::NotHidden :
            llvm::cl::ReallyHidden);
    }
}

} // mrdocs

