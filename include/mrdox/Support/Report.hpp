//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_REPORT_HPP
#define MRDOX_API_SUPPORT_REPORT_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>
#include <fmt/format.h>
#include <string_view>
#include <utility>

namespace clang {
namespace mrdox {

/** Report an error to the console.

    @param text The message contents. A newline
    will be added automatically to the output.
*/
MRDOX_DECL
void
reportError(
    std::string_view text);

/** Format an error to the console.

    @param fs The operation format string.

    @param arg0,args The arguments to use
    with the format string.
*/
template<class Arg0, class... Args>
void
reportError(
    fmt::format_string<Arg0, Args...> fs,
    Arg0&& arg0, Args&&... args)
{
    reportError(fmt::format(fs,
        std::forward<Arg0>(arg0),
        std::forward<Args>(args)...));
}

/** Format an error to the console.

    This function formats an error message
    to the console, of the form:
    @code
    "Could not {1} because {2}."
    @endcode
    Where 1 is the operation which failed,
    specified by the format arguments, and
    2 is the reason for the failure.

    @param err The error which occurred.

    @param fs The operation format string.

    @param arg0,args The arguments to use
    with the format string.
*/
template<class... Args>
void
reportError(
    Error const& err,
    fmt::format_string<Args...> fs,
    Args&&... args)
{
    MRDOX_ASSERT(err.failed());
    reportError(fmt::format(
        "Could not {} because {}",
        fmt::format(fs, std::forward<Args>(args)...),
        err.message()));
}

template<class Range>
Error
reportErrors(Range const& errors)
{
    MRDOX_ASSERT(std::begin(errors) != std::end(errors));
    std::size_t n = 0;
    for(auto const& err : errors)
    {
        ++n;
        reportError(err.message());
    }
    if(n > 1)
        return Error("{} errors occurred", n);
    return Error("an error occurred");
}

/** Report a warning to the console.

    @param text The message contents. A newline
    will be added automatically to the output.
*/
MRDOX_DECL
void
reportWarning(
    std::string_view text);

/** Format a warning to the console.

    @param fs The message format string.
    A newline will be added automatically
    to the output.

    @param arg0,args The arguments to use
    with the format string.
*/
template<class Arg0, class... Args>
void
reportWarning(
    fmt::format_string<Arg0, Args...> fs,
    Arg0&& arg0, Args&&... args)
{
    reportWarning(fmt::format(fs,
        std::forward<Arg0>(arg0),
        std::forward<Args>(args)...));
}

/** Report information to the console.

    @param text The message contents. A newline
    will be added automatically to the output.
*/
MRDOX_DECL
void
reportInfo(
    std::string_view text);

/** Format information to the console.

    @param fs The message format string.
    A newline will be added automatically
    to the output.

    @param arg0,args The arguments to use
    with the format string.
*/
template<class Arg0, class... Args>
void
reportInfo(
    fmt::format_string<Arg0, Args...> fs,
    Arg0&& arg0, Args&&... args)
{
    reportInfo(fmt::format(fs,
        std::forward<Arg0>(arg0),
        std::forward<Args>(args)...));
}

} // mrdox
} // clang

#endif
