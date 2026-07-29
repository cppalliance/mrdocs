//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/Error.hpp>
#include <mrdocs/Handlebars/Error.hpp>
#include <mrdocs/Support/Error/Assert.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Exception.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


namespace mrdocs {

std::string
Error::
formatWhere(
    source_location const& loc)
{
  return std::format("{}:{}", files::makeProjectRelative(loc.file_name()),
                     loc.line());
#if 0
    return std::format(
        "{}@{}({})",
        loc.function_name(),
        files::makeProjectRelative(
            loc.file_name()),
        loc.line());
#endif
}

std::string
Error::
formatMessage(
    std::string_view const& reason,
    std::string_view const& where)
{
    std::string result;
    result = reason;
    result.append(" (");
    result.append(where);
    result.append(")");
    return result;
}

Error::
Error(
    std::string reason,
    source_location loc)
    : where_(formatWhere(loc))
    , reason_(std::move(reason))
    , message_(formatMessage(reason_, where_))
    , loc_(loc)
{
    MRDOCS_ASSERT(! message_.empty());
}

Error::
Error(
    std::error_code const& ec,
    source_location loc)
{
    if(! ec)
        return;
    where_ = formatWhere(loc);
    reason_ = ec.message();
    message_ = formatMessage(reason_, where_);
    loc_ = loc;
}

Error::
Error(
    std::exception const& ex)
{
    std::string_view s(ex.what());
    if(s.empty())
        s = "unknown exception";
    reason_ = s;
    message_ = s;
}

Error::
Error(
    std::vector<Error> const& errors,
    source_location loc)
{
    MRDOCS_ASSERT(errors.size() > 0);
    if(errors.size() == 1)
    {
        *this = errors.front();
        return;
    }

    where_ = formatWhere(loc);
    reason_ = std::format("{} errors occurred:\n", errors.size());
    for(auto const& err : errors)
    {
        reason_.append("    ");
        reason_.append(err.message());
        reason_.push_back('\n');
    }
    message_ = formatMessage(reason_, where_);
    loc_ = loc;
}

void Error::Throw() const&
{
    MRDOCS_ASSERT(failed());
    throw Exception(*this);
}

void Error::Throw() &&
{
    MRDOCS_ASSERT(failed());
    throw Exception(std::move(*this));
}

// Implicit upward conversion from a library-local (std-only) error.
Error::
Error(dom::Error const& e)
{
    if (auto const m = e.message(); !m.empty())
        *this = Error(std::string(m));
}

// Implicit upward conversion from the handlebars library error.
Error::
Error(handlebars::Error const& e)
{
    if (auto const m = e.message(); !m.empty())
        *this = Error(std::string(m));
}

} // namespace mrdocs
