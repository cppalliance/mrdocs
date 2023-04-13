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

#ifndef MRDOX_ERROR_HPP
#define MRDOX_ERROR_HPP

#include <mrdox/detail/nice.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <source_location>
#include <string>

namespace clang {
namespace mrdox {

/** Return an Error with descriptive information.

    @param action A phrase describing the attempted action.

    @param loc The source location where the failure occurred.
*/
[[nodiscard]]
llvm::Error
makeError(
    std::string action,
    std::source_location loc =
        std::source_location::current());

/** Return an Error with descriptive information.

    @param action A phrase describing the attempted action.

    @param because A phrase describing the reason
        for the failure.

    @param loc The source location where the failure occurred.
*/
[[nodiscard]]
llvm::Error
makeError(
    std::string action,
    std::string because,
    std::source_location loc =
        std::source_location::current());

template<class Arg0, class... Args>
struct makeError_ : llvm::Error
{
    makeError_(
        Arg0&& arg0,
        Args&&... args,
        std::source_location loc =
            std::source_location::current())
        : llvm::Error(
            [&]
            {
                using detail::nice;
                std::string temp;
                llvm::raw_string_ostream os(temp);
                os << nice(std::forward<Arg0>(arg0));
                (os << ... << nice(std::forward<Args>(args)));
                os << ' ' << nice(loc);
                return makeError(temp, loc);
            }())
    {
    }
};

template<class Arg0, class... Args>
makeError_(Arg0&&, Args&&...) -> makeError_<Arg0, Args...>;

//------------------------------------------------

/** Return an Error for the given text and source location.

    @param actionFormat A phrase describing the
        attempted action. This may contain printf-style
        percent substitutions.

    @param because The phrase describing the reason
        for the failure.

    @param loc The source location where the failure occurred.
*/
template<class Arg0, class... Args>
[[nodiscard]]
llvm::Error
formatError(
    llvm::StringRef actionFormat,
    Arg0&& arg0, Args&&... args,
    std::source_location loc =
        std::source_location::current())
{
    std::string temp;
    llvm::raw_string_ostream os(temp);
    os << format(
        actionFormat,
        std::forward<Arg0>(arg0),
        std::forward<Args>(args)...);
    return makeError(std::move(temp), loc);
}

/** Return an Error for the given text and source location.

    @param actionFormat A phrase describing the
        attempted action. This may contain printf-style
        percent substitutions.

    @param because A phrase describing the reason
        for the failure.

    @param loc The source location where the failure occurred.
*/
template<class Arg0, class... Args>
[[nodiscard]]
llvm::Error
formatError(
    llvm::StringRef actionFormat,
    std::string because,
    Arg0&& arg0, Args&&... args,
    std::source_location loc =
        std::source_location::current())
{
    std::string temp;
    llvm::raw_string_ostream os(temp);
    os << format(
        actionFormat,
        std::forward<Arg0>(arg0),
        std::forward<Args>(args)...);
    return makeError(std::move(temp), std::move(because), loc);
}

} // mrdox
} // clang

#endif
