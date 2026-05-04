//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/Xml.hpp>
#include <mrdocs/Platform.hpp>
#include <cstddef>

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
//
// XmlEmitter
//
//------------------------------------------------

XmlEmitter::
XmlEmitter(llvm::raw_ostream& os) noexcept
    : os_(os)
{
}

llvm::raw_ostream&
XmlEmitter::
indent()
{
    return os_ << indent_;
}

jit_indenter
XmlEmitter::
jit_indent() noexcept
{
    return jit_indenter(os_, indent_);
}

namespace {

// Emit each non-suppressed attribute as ` name="value"`.
void
writeAttributes(
    llvm::raw_ostream& os,
    std::span<XmlAttribute const> attrs)
{
    for(XmlAttribute const& attr : attrs)
        if(attr.pred)
            os <<
                ' ' << attr.name << '=' <<
                "\"" << xmlEscape(attr.value) << "\"";
}

} // (anon)

void
XmlEmitter::
open(
    std::string_view tag,
    std::span<XmlAttribute const> attrs)
{
    indent() << '<' << tag;
    writeAttributes(os_, attrs);
    os_ << ">\n";
    nest(1);
}

void
XmlEmitter::
close(std::string_view tag)
{
    nest(-1);
    indent() << "</" << tag << ">\n";
}

void
XmlEmitter::
write(
    std::string_view tag,
    std::string_view value,
    std::span<XmlAttribute const> attrs)
{
    if(value.empty())
    {
        indent() << '<' << tag;
        writeAttributes(os_, attrs);
        os_ << "/>\n";
        return;
    }

    indent() << '<' << tag;
    writeAttributes(os_, attrs);
    os_ << '>' << xmlEscape(value) << "</" << tag << ">\n";
}

void
XmlEmitter::
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
