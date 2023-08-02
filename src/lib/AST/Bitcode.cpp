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

#include "Bitcode.hpp"

namespace clang {
namespace mrdox {

#if 0
void
insertBitcode(
    ExecutionContext& ex,
    Bitcode&& bitcode)
{
    // bitcode.data.
    ex.reportResult(
        StringRef(bitcode.id),
        std::move(bitcode.data));
}

Bitcodes
collectBitcodes(
    ToolExecutor& ex)
{
    Bitcodes results;
    ex.getToolResults()->forEachResult(
        [&](StringRef Key, StringRef Value)
        {
            auto result = results.try_emplace(
                Key, std::vector<StringRef>());
            result.first->second.emplace_back(Value);
        });
    return results;
}
#endif

} // mrdox
} // clang
