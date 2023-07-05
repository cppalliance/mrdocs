//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_XML_XMLTAGS_HPP
#define MRDOX_TOOL_XML_XMLTAGS_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <mrdox/Support/Dom.hpp>
#include <clang/Basic/Specifiers.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>
#include <optional>
#include <vector>

/*
    Object for assisting with generating
    XML tags and correctly escaped strings
*/

namespace clang {
namespace mrdox {
namespace xml {

class jit_indenter;

/** Manipulator to apply XML escaping to output.
*/
struct xmlEscape
{
    explicit
    xmlEscape(
        std::string_view const& s) noexcept
        : s_(s)
    {
    }

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        xmlEscape const& t)
    {
        t.write(os);
        return os;
    }

private:
    void write(llvm::raw_ostream& os) const;

    llvm::StringRef s_;
};

//------------------------------------------------

// Converters for attributes
std::string toString(SymbolID const& id);

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
        , value(toString(id))
        , pred(id != SymbolID::zero)
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

/** An vector of zero or more XML attributes.
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

/** A stream which indents just in time.
*/
class jit_indenter
{
    llvm::raw_ostream& os_;
    std::string const& indent_;
    bool indented_ = false;

public:
    jit_indenter(
        llvm::raw_ostream& os,
        std::string const& indent) noexcept
        : os_(os)
        , indent_(indent)
    {
    }

    template<class T>
    llvm::raw_ostream&
    operator<<(T const& t)
    {
        if(! indented_)
        {
            os_ << indent_;
            indented_ = true;
        }
        return os_ << t;
    }

    void
    finish()
    {
        if(indented_)
            os_ << '\n';
    }
};

//------------------------------------------------

/** State object for emission of XML tags and content.
*/
class XMLTags
{
public:
    std::string indent_;
    llvm::raw_ostream& os_;

    explicit
    XMLTags(
        llvm::raw_ostream& os) noexcept
        : os_(os)
    {
    }

    llvm::raw_ostream& indent();
    jit_indenter jit_indent() noexcept;

    void open(dom::String const&, Attributes = {});
    void write(dom::String const&,
        llvm::StringRef value = {}, Attributes = {});
    void close(dom::String const&);

    void nest(int levels);
};

} // xml
} // mrdox
} // clang

#endif
