//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_JAVADOC_HPP
#define MRDOX_API_METADATA_JAVADOC_HPP

#include <mrdox/Platform.hpp>
#include <Support/Error.hpp>
#include <llvm/ADT/SmallString.h>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

// This namespace contains all of the Javadoc
// related types, constants, and functions.
namespace doc {

struct Node;

using String = std::string;

template<typename T>
    requires std::derived_from<T, doc::Node>
using List = std::vector<std::unique_ptr<T>>;

enum class Kind
{
    text = 1, // needed by bitstream
    styled,
    block, // used by bitcodes
    paragraph,
    brief,
    admonition,
    code,
    param,
    tparam,
    returns
};

/** A text style.
*/
enum class Style
{
    none = 1, // needed by bitstream
    mono,
    bold,
    italic
};

/** An admonishment style.
*/
enum class Admonish
{
    none = 1, // needed by bitstream
    note,
    tip,
    important,
    caution,
    warning
};

/** Parameter pass direction.
*/
enum class ParamDirection : int
{
    none,
    in,
    out,
    inout
};

//--------------------------------------------

/** This is a variant-like list element.
*/
struct Node
{
    Kind kind;

    explicit Node(Kind kind_) noexcept
        : kind(kind_)
    {
    }

    virtual ~Node() = default;

    bool operator==(const Node&)
        const noexcept = default;

    virtual bool equals(
        const Node& other) const noexcept
    {
        return kind == other.kind;
    }
};

/** A string of plain text.
*/
struct Text : Node
{
    String string;

    static constexpr Kind static_kind = Kind::text;

    explicit
    Text(
        String string_ = String())
        : Node(Kind::text)
        , string(std::move(string_))
    {
    }

    bool operator==(const Text&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Text&>(other);
    }

protected:
    Text(
        String string_,
        Kind kind_)
        : Node(kind_)
        , string(std::move(string_))
    {
    }
};

/** A piece of style text.
*/
struct StyledText : Text
{
    Style style;

    static constexpr Kind static_kind = Kind::styled;

    StyledText(
        String string_ = String(),
        Style style_ = Style::none)
        : Text(std::move(string_), Kind::styled)
        , style(style_)
    {
    }

    bool operator==(const StyledText&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const StyledText&>(other);
    }

};

/** A piece of block content

    The top level is a list of blocks.
*/
struct Block : Node
{
    static constexpr Kind static_kind = Kind::block;

    List<Text> children;

    bool empty() const noexcept
    {
        return children.empty();
    }

    bool operator==(const Block& other) const noexcept
    {
        if(kind != other.kind)
            return false;
        return std::equal(children.begin(), children.end(),
            other.children.begin(), other.children.end(),
            [](const auto& a, const auto& b)
            {
                return a->equals(*b);
            });
    }

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Block&>(other);
    }

protected:
    explicit
    Block(
        Kind kind_,
        List<Text> children_ = {}) noexcept
        : Node(kind_)
        , children(std::move(children_))
    {
    }
};

/** A sequence of text nodes.
*/
struct Paragraph : Block
{
    static constexpr Kind static_kind = Kind::paragraph;

    Paragraph() noexcept
        : Block(Kind::paragraph)
    {
    }

    bool operator==(const Paragraph&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Paragraph&>(other);
    }

protected:
    explicit
    Paragraph(
        Kind kind,
        List<Text> children_ = {})
        : Block(kind, std::move(children_))
    {
    }
};

/** The brief description
*/
struct Brief : Paragraph
{
    static constexpr Kind static_kind = Kind::brief;

    Brief() noexcept
        : Paragraph(Kind::brief)
    {
    }

    bool operator==(const Brief&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Brief&>(other);
    }
};

/** Documentation for an admonition
*/
struct Admonition : Paragraph
{
    Admonish style;

    explicit
    Admonition(
        Admonish style_ = Admonish::none)
        : Paragraph(Kind::admonition)
        , style(style_)
    {
    }

    bool operator==(const Admonition&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Admonition&>(other);
    }
};

/** Preformatted source code.
*/
struct Code : Paragraph
{
    // VFALCO we can add a language (e.g. C++),
    //        then emit attributes in the generator.

    static constexpr Kind static_kind = Kind::code;

    Code()
        : Paragraph(Kind::code)
    {
    }

    bool operator==(const Code&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Code&>(other);
    }
};

/** Documentation for a function parameter
*/
struct Param : Paragraph
{
    String name;
    ParamDirection direction;

    static constexpr Kind static_kind = Kind::param;

    Param(
        String name_ = String(),
        Paragraph details_ = Paragraph(),
        ParamDirection direction_ = ParamDirection::none)
        : Paragraph(
            Kind::param,
            std::move(details_.children))
        , name(std::move(name_))
        , direction(direction_)
    {
    }

    bool operator==(const Param&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Param&>(other);
    }
};

/** Documentation for a template parameter
*/
struct TParam : Paragraph
{
    String name;

    static constexpr Kind static_kind = Kind::tparam;

    TParam()
        : Paragraph(Kind::tparam)
    {
    }

    bool operator==(const TParam&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const TParam&>(other);
    }
};

/** Documentation for a function return type
*/
struct Returns : Paragraph
{
    static constexpr Kind static_kind = Kind::returns;

    Returns()
        : Paragraph(Kind::returns)
    {
    }

    bool operator==(const Returns&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Returns&>(other);
    }
};

} // doc

/** A processed Doxygen-style comment attached to a declaration.
*/
struct MRDOX_VISIBLE
    Javadoc
{

    //--------------------------------------------

    MRDOX_DECL
    Javadoc() noexcept;

    /** Constructor
    */
    MRDOX_DECL
    explicit
    Javadoc(
        doc::List<doc::Block> blocks);

    /** Return true if this is empty
    */
    MRDOX_DECL
    bool
    empty() const noexcept;

    /** Return the brief, or nullptr if there is none.

        This function should only be called
        after postProcess() has been invoked.
    */
    doc::Paragraph const*
    getBrief() const noexcept
    {
        return brief_.get();
    }

    /** Return the list of top level blocks.
    */
    doc::List<doc::Block> const&
    getBlocks() const noexcept
    {
        return blocks_;
    }

    /** Return the element describing the return type.
    */
    doc::Returns const*
    getReturns() const noexcept
    {
        return returns_.get();
    }

    /** Return the list of top level blocks.
    */
    doc::List<doc::Param> const&
    getParams() const noexcept
    {
        return params_;
    }

    /** Return the list of top level blocks.
    */
    doc::List<doc::TParam> const&
    getTParams() const noexcept
    {
        return tparams_;
    }

    // VFALCO This is unfortunately necessary for
    //        the deserialization from bitcode...
    doc::List<doc::Block>&
    getBlocks() noexcept
    {
        return blocks_;
    }

    //--------------------------------------------

    /** Comparison

        These are used internally to impose a
        total ordering, and not visible in the
        output format.
    */
    /** @{ */
    MRDOX_DECL bool operator==(Javadoc const&) const noexcept;
    MRDOX_DECL bool operator!=(Javadoc const&) const noexcept;
    /* @} */

    /** Apply post-processing to the final object.

        The implementation calls this function once,
        after all doc comments have been merged
        and attached, to calculate the brief as
        follows:

        @li Sets the brief to the first paragraph
            in which a "brief" command exists, or

        @li Sets the first paragraph as the brief if
            no "brief" is found.

        @li Otherwise, the brief is set to a
            null pointer to indicate absence.

        Furthermore, the Params and TParams are
        spliced out of the top level list of
        blocks into their own lists.
    */
    MRDOX_DECL
    void
    postProcess();

    //--------------------------------------------

    /** These are used to bottleneck all insertions.
    */
    /** @{ */
    template<class T, class U>
    static
    void
    append(doc::List<T>& list, doc::List<U>&& other) noexcept
    {
        list.reserve(list.size() + other.size());
        for(auto& p : other)
            list.emplace_back(static_cast<T*>(p.release()));
        other.clear();
    }

    template<class T, class Child>
    static
    void
    append(
        doc::List<T>& list,
        std::unique_ptr<Child>&& child)
    {
        list.emplace_back(
            std::move(child));
    }

    template<class Parent, class Child>
    static
    void
    append(
        Parent& parent,
        std::unique_ptr<Child>&& child)
    {
        append(parent.children,
            std::move(child));
    }

    //--------------------------------------------

private:
    std::unique_ptr<doc::Paragraph> brief_;
    std::unique_ptr<doc::Returns> returns_;
    doc::List<doc::Block> blocks_;
    doc::List<doc::Param> params_;
    doc::List<doc::TParam> tparams_;
};

} // mrdox
} // clang

#endif
