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

#ifndef MRDOCS_LIB_GEN_XML_XMLTAGS_HPP
#define MRDOCS_LIB_GEN_XML_XMLTAGS_HPP

#include <lib/Support/Xml.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/Specifiers/AccessKind.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Support/EnumToString.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>
#include <type_traits>
#include <vector>

/*
    Object for assisting with generating
    XML tags and correctly escaped strings
*/

namespace mrdocs::xml {

//------------------------------------------------

// Converters for attributes
std::string toBase64Str(SymbolID const& id);

//------------------------------------------------

/** A single XML attribute.
*/
struct Attribute
{
    dom::String name;
    dom::String value;
    bool pred;

    Attribute(
        dom::String name_,
        dom::String value_,
        bool pred_ = true) noexcept
        : name(std::move(name_))
        , value(std::move(value_))
        , pred(pred_)
    {
    }

    Attribute(
        SymbolID id)
        : name("id")
        , value(toBase64Str(id))
        , pred(id != SymbolID::invalid)
    {
    }

    Attribute(
        AccessKind access)
        : name("access")
        , value(toString(access))
        , pred(access == AccessKind::Private ||
            access == AccessKind::Protected)
    {
    }

    template<class Enum>
    requires std::is_enum_v<Enum>
    Attribute(Enum v)
        : name("value")
        , value(std::to_string(static_cast<
            std::underlying_type_t<Enum>>(v)))
        , pred(true)
    {
    }
};

//------------------------------------------------

/** A vector of zero or more XML attributes.
*/
struct Attributes
{
    std::vector<Attribute> attrs_;

    Attributes() = default;
    Attributes(std::initializer_list<Attribute> attrs);
    Attributes(const std::vector<Attribute>& attrs);
    Attributes(std::vector<Attribute>&& attrs);

    void push(Attribute const& attr);
    void push(Attribute&& attr);

    friend
    llvm::raw_ostream& operator<<(
        llvm::raw_ostream& os,
        Attributes const& attrs);
};


//------------------------------------------------

/** State object for emission of XML tags and content.
*/
class XMLTags
{
    XmlEmitter emitter_;

public:
    explicit
    XMLTags(
        llvm::raw_ostream& os) noexcept
        : emitter_(os)
    {
    }

    llvm::raw_ostream& indent() { return emitter_.indent(); }
    jit_indenter jit_indent() noexcept { return emitter_.jit_indent(); }

    void open(dom::String const&, Attributes = {});
    void write(dom::String const&,
        llvm::StringRef value = {}, Attributes = {});
    void close(dom::String const&);
    void nesting(bool enable) noexcept { emitter_.nesting(enable); }

    void nest(int levels) { emitter_.nest(levels); }
};

} // mrdocs::xml

#endif
