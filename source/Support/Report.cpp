//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Support/Report.hpp>
#include <llvm/Support/Mutex.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Signals.h>
#include <cstdlib>
#include <mutex>

namespace clang {
namespace mrdox {

static llvm::sys::Mutex report_mutex_;

void
reportError(
    std::string_view text)
{
    std::lock_guard<llvm::sys::Mutex> lock(report_mutex_);
    llvm::errs() << text << '\n';
}

void
reportWarning(
    std::string_view text)
{
    std::lock_guard<llvm::sys::Mutex> lock(report_mutex_);
    llvm::errs() << text << '\n';
}

void
reportInfo(
    std::string_view text)
{
    std::lock_guard<llvm::sys::Mutex> lock(report_mutex_);
    llvm::errs() << text << '\n';
}

void
reportUnhandledException(
    std::exception const& ex)
{
    namespace sys = llvm::sys;

    std::lock_guard<llvm::sys::Mutex> lock(report_mutex_);
    llvm::errs() <<
        "Unhandled exception: " << ex.what() << '\n';
    sys::PrintStackTrace(llvm::errs());
    std::exit(EXIT_FAILURE);
}

} // mrdox
} // clang
