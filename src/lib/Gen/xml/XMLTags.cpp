//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "XMLTags.hpp"
#include "lib/Support/Radix.hpp"
#include <mrdocs/Platform.hpp>

namespace clang {
namespace mrdocs {
namespace xml {

//------------------------------------------------
//
// xmlEscape
//
//------------------------------------------------

void
xmlEscape::
write(
    llvm::raw_ostream& os) const
{
    std::size_t pos = 0;
    auto const size = s_.size();
    while(pos < size)
    {
    unescaped:
        auto const found = s_.find_first_of("<>&'\"", pos);
        if(found == llvm::StringRef::npos)
        {
            os.write(s_.data() + pos, s_.size() - pos);
            break;
        }
        os.write(s_.data() + pos, found - pos);
        pos = found;
        while(pos < size)
        {
            auto const c = s_[pos];
            switch(c)
            {
            case '<':
                os.write("&lt;", 4);
                break;
            case '>':
                os.write("&gt;", 4);
                break;
            case '&':
                os.write("&amp;", 5);
                break;
            case '\'':
                os.write("&apos;", 6);
                break;
            case '\"':
                os.write("&quot;", 6);
                break;
            default:
                goto unescaped;
            }
            ++pos;
        }
    }
}

//------------------------------------------------

std::string
toBase64Str(
    SymbolID const& id)
{
    return toBase64(id);
}

//------------------------------------------------

Attributes::
Attributes(
    std::initializer_list<Attribute> attrs)
    : attrs_(attrs)
{
}

Attributes::
Attributes(
    const std::vector<Attribute>& attrs)
    : attrs_(attrs)
{
}

Attributes::
Attributes(
    std::vector<Attribute>&& attrs)
    : attrs_(std::move(attrs))
{
}

void
Attributes::
push(Attribute const& attr)
{
    attrs_.push_back(attr);
}

void
Attributes::
push(Attribute&& attr)
{
    attrs_.push_back(std::move(attr));
}

llvm::raw_ostream&
operator<<(
    llvm::raw_ostream& os,
    Attributes const& attrs)
{
    for(auto const& attr : attrs.attrs_)
        if(attr.pred)
            os <<
                ' ' << attr.name << '=' <<
                "\"" << xmlEscape(attr.value) << "\"";
    return os;
}

//------------------------------------------------
//
// XMLTags
//
//------------------------------------------------

llvm::raw_ostream&
XMLTags::
indent()
{
    return os_ << indent_;
}

auto
XMLTags::
jit_indent() noexcept ->
    jit_indenter
{
    return jit_indenter(os_, indent_);
}

//------------------------------------------------

void
XMLTags::
open(
    dom::String const& tag,
    Attributes attrs)
{
    indent() << '<' << tag << attrs << ">\n";
    nest(1);
}

void
XMLTags::
close(
    dom::String const& tag)
{
    nest(-1);
    indent() << "</" << tag << ">\n";
}

void
XMLTags::
write(
    dom::String const& tag,
    llvm::StringRef value,
    Attributes attrs)
{
    if(value.empty())
    {
        indent() << "<" << tag << attrs << "/>\n";
        return;
    }

    indent() <<
        "<" << tag << attrs << ">" <<
        xmlEscape(value) <<
        "</" << tag << ">\n";
}

void
XMLTags::
nest(int levels)
{
    if (!nesting_)
        return;
    
    if(levels > 0)
    {
        indent_.append(levels * 2, ' ');
    }
    else
    {
        auto const n = static_cast<std::size_t>(levels * -2);
        MRDOCS_ASSERT(n <= indent_.size());
        indent_.resize(indent_.size() - n);
    }
}

} // xml
} // mrdocs
} // clang
