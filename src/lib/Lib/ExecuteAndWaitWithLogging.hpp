//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_EXECUTE_AND_WAIT_WITH_LOGGING_HPP
#define MRDOCS_LIB_EXECUTE_AND_WAIT_WITH_LOGGING_HPP

#include <llvm/Support/Program.h>

namespace clang::mrdocs {

/**
 * A wrapper around llvm::sys::ExecuteAndWait() that prints the command
 * being run (with its arguments).
 * 
 * This function has the same parameters, with the same meaning, as
 * llvm::sys::ExecuteAndWait().
 * 
 * \par Preconditions
 * `args.size() >= 1`.
 */
int ExecuteAndWaitWithLogging(
    llvm::StringRef program,
    llvm::ArrayRef<llvm::StringRef> args,
    std::optional<llvm::ArrayRef<llvm::StringRef>> env = std::nullopt,
    llvm::ArrayRef<std::optional<llvm::StringRef>> redirects = {},
    unsigned secondsToWait = 0,
    unsigned memoryLimit = 0,
    std::string* errMsg = nullptr,
    bool* executionFailed = nullptr,
    std::optional<llvm::sys::ProcessStatistics>* procStat = nullptr,
    llvm::BitVector* affinityMask = nullptr);

} // clang::mrdocs

#endif // MRDOCS_LIB_EXECUTE_AND_WAIT_WITH_LOGGING_HPP
