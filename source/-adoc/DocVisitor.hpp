//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_ADOC_DOCVISITOR_HPP
#define MRDOX_TOOL_ADOC_DOCVISITOR_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <mrdox/Support/RangeFor.hpp>
#include <fmt/format.h>

namespace clang {
namespace mrdox {
namespace adoc {

inline
std::string_view
ltrim(std::string_view s)
{
    auto it = s.begin();
    for (;; ++it)
    {
        if (it == s.end())
            return {};
        if (!isspace(*it))
            break;
    }
    return s.substr(it - s.begin());
}

inline
std::string_view
rtrim(std::string_view s)
{
    auto it = s.end() - 1;
    for (; it > s.begin() && isspace(*it); --it);
    return s.substr(0, it - s.begin());
}

inline
std::string_view
trim(std::string_view s)
{
    auto left = s.begin();
    for (;; ++left)
    {
        if (left == s.end())
            return {};
        if (!isspace(*left))
            break;
    }
    auto right = s.end() - 1;
    for (; right > left && isspace(*right); --right);
    return std::string_view(&*left, right - left + 1);
}

class DocVisitor
{
    std::string& dest_;
    std::back_insert_iterator<std::string> ins_;

public:
    explicit
    DocVisitor(
        std::string& dest) noexcept
        : dest_(dest)
        , ins_(std::back_inserter(dest_))
    {
    }

    void operator()(
        doc::List<doc::Block> const& list)
    {
        for(auto const& block : list)
            doc::visit(*block, *this);
    }

    void operator()(doc::Admonition const& I)
    {
        //dest_ += I.string;
    }

    void operator()(doc::Code const& I)
    {
        auto const leftMargin = measureLeftMargin(I.children);
        dest_ +=
            "[,cpp]\n"
            "----\n";
        for(auto const& it : RangeFor(I.children))
        {
            doc::visit(*it.value,
                [&]<class T>(T const& text)
                {
                    if constexpr(std::is_same_v<T, doc::Text>)
                    {
                        if(! text.string.empty())
                        {
                            std::string_view s = text.string;
                            s.remove_prefix(leftMargin);
                            dest_.append(s);
                        }
                        dest_.push_back('\n');
                    }
                    else
                    {
                        MRDOX_UNREACHABLE();
                    }
                });
        }
        dest_ += "----\n";
    }

    void operator()(doc::Heading const& I)
    {
        fmt::format_to(ins_, "=== {}\n", I.string);
    }

    //void operator()(doc::Brief const& I)
    void operator()(doc::Paragraph const& I)
    {
        for(auto const& it : RangeFor(I.children))
        {
            auto const n = dest_.size();
            doc::visit(*it.value, *this);
            // detect empty text blocks
            if(! it.last && dest_.size() > n)
            {
                // wrap past 80 cols
                if(dest_.size() < 80)
                    dest_.push_back(' ');
                else
                   dest_.append("\n");
            }
        }
        dest_.push_back('\n');
    }

    void operator()(doc::Link const& I)
    {
        //dest_ += I.string;
    }

    void operator()(doc::ListItem const& I)
    {
        dest_.append("* ");
        for(auto const& it : RangeFor(I.children))
        {
            auto const n = dest_.size();
            doc::visit(*it.value, *this);
            // detect empty text blocks
            if(! it.last && dest_.size() > n)
            {
                // wrap past 80 cols
                if(dest_.size() < 80)
                    dest_.push_back(' ');
                else
                   dest_.append("\n");
            }
        }
        dest_.push_back('\n');
    }

    void operator()(doc::Param const& I)
    {
        //dest_ += I.string;
    }

    void operator()(doc::Returns const& I)
    {
        //dest_ += I.string;
    }

    void operator()(doc::Text const& I)
    {
        // Asciidoc text must not have leading 
        // else they can be rendered up as code.
        std::string_view s = trim(I.string);
        dest_.append(s);
    }

    void operator()(doc::Styled const& I)
    {
        // VFALCO We need to apply Asciidoc escaping
        // depending on the contents of the string.
        std::string_view s = trim(I.string);
        switch(I.style)
        {
        case doc::Style::none:
            dest_.append(s);
            break;
        case doc::Style::bold:
            fmt::format_to(std::back_inserter(dest_), "*{}*", s);
            break;
        case doc::Style::mono:
            fmt::format_to(std::back_inserter(dest_), "`{}`", s);
            break;
        case doc::Style::italic:
            fmt::format_to(std::back_inserter(dest_), "_{}_", s);
            break;
        default:
            MRDOX_UNREACHABLE();
        }
    }

    void operator()(doc::TParam const& I)
    {
        //dest_ += I.string;
    }

    std::size_t
    measureLeftMargin(
        doc::List<doc::Text> const& list)
    {
        if(list.empty())
            return 0;
        std::size_t n = std::size_t(-1);
        for(auto& text : list)
        {
            if(trim(text->string).empty())
                continue;
            auto const space =
                text->string.size() - ltrim(text->string).size();
            if( n > space)
                n = space;
        }
        return n;
    }

};

} // adoc
} // mrdox
} // clang

#endif
