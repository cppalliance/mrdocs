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

#include <mrdox/Error.hpp>
#include <mrdox/Reporter.hpp>

namespace clang {
namespace mrdox {

namespace {

class ErrorInfoPlus
    : public llvm::ErrorInfo<ErrorInfoPlus>
{
    std::string what_;
    std::source_location loc_;

public:
    static char ID;

    ErrorInfoPlus(
        llvm::StringRef what,
        std::source_location loc)
        : what_(what)
        , loc_(loc)
    {
    }

    void
    log(
        llvm::raw_ostream &os) const override
    {
        os << what_ << " at " << Reporter::makeString(loc_);
    }

    std::error_code
    convertToErrorCode() const
    {
        return llvm::inconvertibleErrorCode();
    }

    void const*
    dynamicClassID() const
    {
        return this;
    }
};

char ErrorInfoPlus::ID{};

} // (anon)

llvm::Error
makeError(
    llvm::StringRef what,
    std::source_location loc)
{
    return llvm::make_error<ErrorInfoPlus>(what, loc);
}

} // mrdox
} // clang
