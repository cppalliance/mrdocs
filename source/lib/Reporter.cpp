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

#include <mrdox/Reporter.hpp>
#include <llvm/Support/raw_ostream.h>

namespace clang {
namespace mrdox {

//------------------------------------------------

bool
Reporter::
success(llvm::Error&& err)
{
    if(! err)
        return true;
    // VFALCO TODO Source file, line number, and what
    llvm::errs() << toString(std::move(err)) << "\n";
    failed_ = true;
    return false;
}

bool
Reporter::
success(std::error_code const& ec)
{
    if(! ec)
        return true;
    // VFALCO TODO Source file, line number, and what
    llvm::errs() << ec.message() << "\n";
    failed_ = true;
    return false;
}

bool
Reporter::
success(
    llvm::StringRef what,
    std::error_code const& ec)
{
    if(! ec)
        return true;
    llvm::errs() <<
        what << ": " << ec.message() << "\n";
    failed_ = true;
    return false;
}

bool
Reporter::
success(
    llvm::StringRef what,
    llvm::Error& err)
{
    if(! err)
        return true;
    llvm::errs() <<
        what << ": " << toString(std::move(err)) << "\n";
    failed_ = true;
    return false;
}

} // mrdox
} // clang
