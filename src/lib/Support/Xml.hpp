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

#ifndef MRDOCS_LIB_SUPPORT_XML_HPP
#define MRDOCS_LIB_SUPPORT_XML_HPP

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>
#include <span>
#include <string>
#include <string_view>

/*
    Lightweight XML emission helpers shared by the XML symbol
    writer, the Doxygen-tagfile writer, and the RELAX NG schema
    writer. No metadata-specific knowledge lives here.
*/

namespace mrdocs {
namespace xml {

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

/** A single XML attribute.

    `pred=false` causes the emitter to omit the attribute, which
    lets callers express conditionally-emitted attributes inline
    without an `if`.
*/
struct XmlAttribute
{
    std::string name;
    std::string value;
    bool pred = true;
};

//------------------------------------------------

/** State object for emission of XML tags and content.

    Manages indentation and produces well-formed XML elements via
    `open` / `close` / `write`. Indentation is two spaces per level
    and is advanced/retreated on `open`/`close`.
*/
class XmlEmitter
{
    llvm::raw_ostream& os_;
    std::string indent_;
    bool nesting_ = true;

public:
    explicit
    XmlEmitter(
        llvm::raw_ostream& os) noexcept;

    llvm::raw_ostream& indent();
    jit_indenter jit_indent() noexcept;

    void open(std::string_view tag,
        std::span<XmlAttribute const> attrs = {});
    void write(std::string_view tag,
        std::string_view value = {},
        std::span<XmlAttribute const> attrs = {});
    void close(std::string_view tag);
    void nesting(bool enable) noexcept { nesting_ = enable; }

    void nest(int levels);
};

} // xml
} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_XML_HPP
