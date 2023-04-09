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
#include <utility>

namespace clang {
namespace mrdox {

namespace {

class ErrorInfoPlus
    : public llvm::ErrorInfo<ErrorInfoPlus>
{
    std::string action_;
    std::source_location loc_;

public:
    static char ID;

    ErrorInfoPlus(
        std::string action,
        std::source_location loc)
        : action_(std::move(action))
        , loc_(loc)
    {
    }

    void
    log(
        llvm::raw_ostream &os) const override
    {
        os << action_ << " at " << Reporter::makeString(loc_);
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
    std::string action,
    std::source_location loc)
{
    return llvm::make_error<ErrorInfoPlus>(std::move(action), loc);
}

llvm::Error
makeError(
    std::string action,
    std::string because,
    std::source_location loc)
{
    return llvm::make_error<ErrorInfoPlus>(std::move(action), loc);
}

} // mrdox
} // clang
