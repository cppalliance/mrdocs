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

#ifndef MRDOX_API_ERROR_HPP
#define MRDOX_API_ERROR_HPP

#include <mrdox/Platform.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace clang {
namespace mrdox {

//------------------------------------------------

class Err
{
    std::string text_;

public:
    Err() = default;

    explicit
    Err(std::string text)
        : text_(std::move(text))
    {
    }

    explicit
    operator bool() const noexcept
    {
        return ! text_.empty();
    }

    std::string_view
    message() const noexcept
    {
        return text_;
    }
};

//------------------------------------------------
/*
    nice output for variadic error functions

    These are used to convert arguments to
    strings in makeError and Reporter members.
*/

template<class T>
T& nice(T& t)
{
    return t;
}

template<class T>
T&& nice(T&& t)
{
    return std::forward<T>(t);
}

template<class T>
auto nice(llvm::Expected<T>&& e)
{
    return nice(e.takeError());
}

inline auto nice(std::error_code ec)
{
    return ec.message();
}

inline auto nice(Err e)
{
    return e.message();
}

template<class T>
auto nice(llvm::ErrorOr<T>&& e)
{
    return nice(e.getError());
}

MRDOX_DECL
llvm::StringRef
nice(std::source_location loc);

//------------------------------------------------

/** Return an Error with descriptive information.

    @param reason A phrase describing the cause of the failure.

    @param loc The source location where the failure occurred.
*/
MRDOX_DECL
[[nodiscard]]
llvm::Error
makeErrorString(
    std::string reason,
    std::source_location loc =
        std::source_location::current());

template<class Arg0, class... Args>
struct makeError : llvm::Error
{
    makeError(
        Arg0&& arg0,
        Args&&... args,
        std::source_location loc =
            std::source_location::current())
        : llvm::Error(
            [&]
            {
                std::string temp;
                llvm::raw_string_ostream os(temp);
                os << nice(std::forward<Arg0>(arg0));
                if constexpr(sizeof...(args) > 0)
                    (os << ... << nice(std::forward<Args>(args)));
                os << ' ' << nice(loc);
                return makeErrorString(std::move(temp), loc);
            }())
    {
    }
};

template<class Arg0, class... Args>
makeError(Arg0&&, Args&&...) -> makeError<Arg0, Args...>;

template<class... Args>
Err makeErr(Args&&... args)
{
    auto err = makeError(std::forward<Args>(args)...);
    if(! err)
        return Err();
    std::string s;
    llvm::raw_string_ostream os(s);
    os << err;
    return Err(s.c_str());
}


} // mrdox
} // clang

#endif
