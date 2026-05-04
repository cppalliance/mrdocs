//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "XMLTags.hpp"
#include <mrdocs/Platform.hpp>
#include <lib/Support/Radix.hpp>


namespace mrdocs {
namespace xml {

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

namespace {

// Convert metadata-flavored Attributes into a span-friendly
// vector of XmlAttribute, dropping suppressed entries.
std::vector<XmlAttribute>
toXmlAttributes(Attributes const& attrs)
{
    std::vector<XmlAttribute> result;
    result.reserve(attrs.attrs_.size());
    for(Attribute const& a : attrs.attrs_)
    {
        if(a.pred)
        {
            result.push_back({
                std::string(std::string_view(a.name)),
                std::string(std::string_view(a.value)),
                true});
        }
    }
    return result;
}

} // (anon)

void
XMLTags::
open(
    dom::String const& tag,
    Attributes attrs)
{
    std::vector<XmlAttribute> const xml_attrs = toXmlAttributes(attrs);
    emitter_.open(std::string_view(tag), xml_attrs);
}

void
XMLTags::
close(
    dom::String const& tag)
{
    emitter_.close(std::string_view(tag));
}

void
XMLTags::
write(
    dom::String const& tag,
    llvm::StringRef value,
    Attributes attrs)
{
    std::vector<XmlAttribute> const xml_attrs = toXmlAttributes(attrs);
    emitter_.write(
        std::string_view(tag),
        std::string_view(value.data(), value.size()),
        xml_attrs);
}

} // xml
} // mrdocs
