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

namespace clang {
namespace mrdox {

void
ExecutionContext::
report(
    Diagnostics&& diags)
{
    std::lock_guard<llvm::sys::Mutex> lock(mutex_);
    diags_.merge(std::move(diags), &llvm::outs());
}

void
ExecutionContext::
reportEnd(report::Level level)
{
    diags_.reportTotals(level);
}

} // mrdox
} // clang
