//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ExecuteAndWaitWithLogging.hpp"
#include <lib/Support/Report.hpp>
#include <mrdocs/Support/Assert.hpp>

namespace clang::mrdocs {

int ExecuteAndWaitWithLogging(
    llvm::StringRef program,
    llvm::ArrayRef<llvm::StringRef> args,
    std::optional<llvm::ArrayRef<llvm::StringRef>> env,
    llvm::ArrayRef<std::optional<llvm::StringRef>> redirects,
    unsigned secondsToWait,
    unsigned memoryLimit,
    std::string* errMsg,
    bool* executionFailed,
    std::optional<llvm::sys::ProcessStatistics>* procStat,
    llvm::BitVector* affinityMask)
{
    MRDOCS_ASSERT(args.size() >= 1);
    llvm::SmallString<128> command = args[0];
    for (llvm::ArrayRef<llvm::StringRef>::size_type i = 1; i < args.size(); ++i) {
        command += ' ';
        command += args[i];
    }
    report::info("{}", command);
    return llvm::sys::ExecuteAndWait(program, args, env, redirects, secondsToWait, memoryLimit, errMsg, executionFailed, procStat, affinityMask);
}

} // clang::mrdocs
