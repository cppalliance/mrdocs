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
    std::source_location const& loc)
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
    std::vector<Error> const& errors)
{
    MRDOX_ASSERT(errors.size() > 0);
    if(errors.size() == 1)
    {
        text_ = errors.front().message();
        return;
    }

    text_ = fmt::format("{} errors occurred:\n", errors.size());
    for(auto const& err : errors)
    {
        text_.append("    ");
        text_.append(err.message());
        text_.push_back('\n');
    }
}

} // mrdox
} // clang
