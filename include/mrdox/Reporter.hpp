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

#ifndef MRDOX_REPORTER_HPP
#define MRDOX_REPORTER_HPP

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <system_error>

namespace clang {
namespace mrdox {

//------------------------------------------------

/** Used to check and report errors uniformly.
*/
struct Reporter
{
    bool failed()
    {
        return failed_;
    }

    bool success(llvm::Error&& err);

    bool
    success(
        llvm::StringRef what,
        std::error_code const& ec);

    bool
    success(
        llvm::StringRef what,
        llvm::Error& err);

private:
    bool failed_ = false;
};

} // mrdox
} // clang

#endif
