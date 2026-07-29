//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Handlebars/OutputRef.hpp>
#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>

namespace mrdocs {
namespace handlebars {

OutputRef&
OutputRef::
write_impl( std::string_view sv )
{
    // ==========================================
    // No indent
    // ==========================================
    if (indent_ == 0)
    {
        fptr_( out_, sv );
        return *this;
    }

    std::size_t pos = sv.find('\n');
    if (pos == std::string_view::npos)
    {
        fptr_( out_, sv );
        return *this;
    }

    // ==========================================
    // Indented
    // ==========================================
    fptr_( out_, sv.substr(0, pos + 1) );
    ++pos;
    while (pos < sv.size())
    {
        for (std::size_t i = 0; i < indent_; ++i)
        {
            fptr_( out_, std::string_view(" ") );
        }
        std::size_t next = sv.find('\n', pos);
        if (next == std::string_view::npos)
        {
            fptr_( out_, sv.substr(pos) );
            return *this;
        }
        fptr_( out_, sv.substr(pos, (next - pos) + 1) );
        pos = next + 1;
    }
    return *this;
}

void
HTMLEscape(
    OutputRef& out,
    std::string_view str)
{
    // Entity table lives in the public header so the HTML generator's
    // `EscapeMap` can share it. Source convention follows handlebars.js:
    // https://github.com/handlebars-lang/handlebars.js/blob/master/lib/handlebars/utils.js
    static constexpr auto badChars = std::views::keys(htmlEscapeEntities);
    for (auto c : str)
    {
        if (auto it = std::ranges::find(badChars, c); it != badChars.end())
        {
            out << it.base()->second;
        }
        else
        {
            out << c;
        }
    }
}

std::string
HTMLEscape(std::string_view str)
{
    std::string result;
    OutputRef out(result);
    HTMLEscape(out, str);
    return result;
}

} // namespace handlebars
} // namespace mrdocs
