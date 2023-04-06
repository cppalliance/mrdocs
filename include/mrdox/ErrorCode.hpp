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

#ifndef MRDOX_ERRORCODE_HPP
#define MRDOX_ERRORCODE_HPP

#include <mrdox/detail/access.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>
#include <source_location>
#include <system_error>

namespace clang {
namespace mrdox {

//------------------------------------------------

namespace detail {
CONST_FUNCTION_ACCESS(callGetPtr, llvm::Error, getPtr, llvm::ErrorInfoBase*);
} // detail

/** Holds a portable error code
*/
class ErrorCode
{
    std::error_code ec_;
    std::source_location loc_;

    static
    llvm::ErrorInfoBase const*
    getPtr(llvm::Error const& e) noexcept
    {
        return access::callFunction<detail::callGetPtr>(e);
    }

    static
    std::error_code
    getErrorCode(
        llvm::Error const& e) noexcept
    {
        auto const p = getPtr(e);
        if(! p)
            return std::error_code();
        return p->convertToErrorCode();
    }

public:
    ErrorCode() = default;
    ErrorCode(ErrorCode const&) = default;
    ErrorCode& operator=(ErrorCode const&) = default;

    ErrorCode(
        llvm::Error& e,
        std::source_location const& loc = 
            std::source_location::current()) noexcept
        : ec_(getErrorCode(e))
        , loc_(loc)
    {
        e.operator bool();
    }

    ErrorCode(
        llvm::Error&& e,
        std::source_location const& loc = 
            std::source_location::current()) noexcept
        : ec_(getErrorCode(e))
        , loc_(loc)
    {
        llvm::Error e_(std::move(e));
        e_.operator bool();
    }

    std::string
    message() const
    {
        return ec_.message();
    }

    std::source_location const&
    where() const noexcept
    {
        return loc_;
    }

    bool failed() const noexcept
    {
        return ec_.operator bool();
    }

    operator bool() const noexcept
    {
        return failed();
    }

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        ErrorCode const& ec)
    {
        ec.write(os);
        return os;
    }

private:
    void write(llvm::raw_ostream& os) const;
};

} // mrdox
} // clang

#endif
