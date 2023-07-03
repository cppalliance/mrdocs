//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/Path.hpp>

namespace clang {
namespace mrdox {

std::string
Error::
appendSourceLocation(
    std::string&& text,
    source_location const& loc)
{
#if 0
    fmt::format_to(
        std::back_inserter(text),
        "\n"
        "    function `{}` at {}({})",
        loc.function_name(),
        files::getSourceFilename(loc.file_name()),
        loc.line());
#else
    fmt::format_to(
        std::back_inserter(text),
        " at {}({})",
        files::getSourceFilename(loc.file_name()),
        loc.line());
#endif
    return std::move(text);
}

Error::
Error(
    std::string reason,
    source_location loc)
    : message_(appendSourceLocation(std::string(reason), loc))
    , reason_(std::move(reason))
    , loc_(loc)
{
    MRDOX_ASSERT(! message_.empty());
}

Error::
Error(
    std::error_code const& ec,
    source_location loc)
{
    if(! ec)
        return;
    message_ = appendSourceLocation(ec.message(), loc);
    reason_ = ec.message();
    loc_ = loc;
}

Error::
Error(
    std::vector<Error> const& errors,
    source_location loc)
{
    MRDOX_ASSERT(errors.size() > 0);
    if(errors.size() == 1)
    {
        message_ = errors.front().message();
        return;
    }

    message_ = fmt::format("{} errors occurred:\n", errors.size());
    for(auto const& err : errors)
    {
        message_.append("    ");
        message_.append(err.message());
        message_.push_back('\n');
    }
    reason_ = message_;
    loc_ = loc;
}

void Error::Throw() const&
{
    MRDOX_ASSERT(failed());
    throw Exception(*this);
}

void Error::Throw() &&
{
    MRDOX_ASSERT(failed());
    throw Exception(std::move(*this));
}

SourceLocation::
SourceLocation(
    source_location const& loc) noexcept
    : file_(files::getSourceFilename(loc.file_name()))
    , line_(loc.line())
    , col_(loc.column())
    , func_(loc.function_name())
{
}

} // mrdox
} // clang
