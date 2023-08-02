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

#include "ExecutionContext.hpp"
#include "lib/AST/Bitcode.hpp"

namespace clang {
namespace mrdox {

void
ExecutionContext::
report(
    InfoSet&& results,
    Diagnostics&& diags)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for(auto& I : results)
    {
        Bitcode bitcode = writeBitcode(*I);
        auto [it, created] = bitcode_.emplace(
            I->id, std::vector<SmallString<0>>());
        auto& codes = it->second;
        if(std::find(codes.begin(), codes.end(), bitcode.data) == codes.end())
            codes.emplace_back(std::move(bitcode.data));
    }

    diags_.mergeAndReport(std::move(diags));
}

void
ExecutionContext::
reportEnd(report::Level level)
{
    diags_.reportTotals(level);
}

} // mrdox
} // clang
