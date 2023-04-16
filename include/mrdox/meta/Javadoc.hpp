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

#ifndef MRDOX_META_JAVADOC_HPP
#define MRDOX_META_JAVADOC_HPP

#include <mrdox/meta/List.hpp>
#include <llvm/ADT/SmallString.h>
#include <memory>
#include <string>
#include <utility>

namespace clang {
namespace mrdox {

/** A processed Doxygen-style comment attached to a declaration.
*/
struct Javadoc
{
    using String = std::string;

    enum class Kind : int
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
    enum class Style : int
    {
        none = 1, // needed by bitstream
        mono,
        bold,
        italic
    };

    /** An admonishment style.
    */
    enum class Admonish : int
    {
        none = 1, // needed by bitstream
        note,
        tip,
        important,
        caution,
        warning
    };

    /** This is a variant-like list element.
    */
    struct Node
    {
        Kind kind;

        auto operator<=>(Node const&) const noexcept = default;

        explicit Node(Kind kind_) noexcept
            : kind(kind_)
        {
        }
    };

    /** A string of plain text.
    */
    struct Text : Node
    {
        String text;

        auto operator<=>(Text const&) const noexcept = default;

        explicit
        Text(
            String text_ = String())
            : Node(Kind::text)
            , text(std::move(text_))
        {
        }

    protected:
        Text(
            String text_,
            Kind kind_)
            : Node(kind_)
            , text(std::move(text_))
        {
        }
    };

    /** A piece of style text.
    */
    struct StyledText : Text
    {
        Style style;

        auto operator<=>(StyledText const&) const noexcept = default;

        StyledText(
            String text = String(),
            Style style_ = Style::none)
            : Text(std::move(text), Kind::styled)
            , style(style_)
        {
        }
    };

    /** A piece of block content

        The top level is a list of blocks.
    */
    struct Block : Node
    {
        auto operator<=>(Block const&) const noexcept = default;

    protected:
        explicit
        Block(Kind kind_) noexcept
            : Node(kind_)
        {
        }
    };

    /** A sequence of text nodes.
    */
    struct Paragraph : Block
    {
        List<Text> list;

        bool empty() const noexcept
        {
            return list.empty();
        }

        auto operator<=>(Paragraph const&) const noexcept = default;

        Paragraph() noexcept
            : Block(Kind::paragraph)
        {
        }

    protected:
        explicit
        Paragraph(Kind kind) noexcept
            : Block(kind)
        {
        }
    };

    /** The brief description
    */
    struct Brief : Paragraph
    {
        auto operator<=>(Brief const&) const noexcept = default;

        Brief() noexcept
            : Paragraph(Kind::brief)
        {
        }
    };

    /** Documentation for an admonition
    */
    struct Admonition : Paragraph
    {
        Admonish style;

        auto operator<=>(Admonition const&) const noexcept = default;

        explicit
        Admonition(
            Admonish style_ = Admonish::none)
            : Paragraph(Kind::admonition)
            , style(style_)
        {
        }
    };

    /** Preformatted source code.
    */
    struct Code : Paragraph
    {
        // VFALCO we can add a language (e.g. C++),
        //        then emit attributes in the generator.

        auto operator<=>(Code const&) const noexcept = default;

        Code()
            : Paragraph(Kind::code)
        {
        }
    };

    /** Documentation for a function parameter
    */
    struct Param : Block
    {
        String name;
        Paragraph details;

        auto operator<=>(Param const&) const noexcept = default;

        Param(
            String name_ = String(),
            Paragraph details_ = Paragraph())
            : Block(Kind::param)
            , name(std::move(name_))
            , details(std::move(details_))
        {
        }
    };

    /** Documentation for a template parameter
    */
    struct TParam : Block
    {
        String name;
        Paragraph details;

        auto operator<=>(TParam const&) const noexcept = default;

        TParam(
            String name_ = String(),
            Paragraph details_ = Paragraph())
            : Block(Kind::param)
            , name(std::move(name_))
            , details(std::move(details_))
        {
        }
    };

    /** Documentation for a function return type
    */
    struct Returns : Paragraph
    {
        auto operator<=>(Returns const&) const noexcept = default;

        Returns()
            : Paragraph(Kind::returns)
        {
        }
    };

    // VFALCO LEGACY
    llvm::SmallString<32> brief;
    std::string desc;

    //---

    Paragraph const*
    getBrief() const noexcept;

    List<Block> const&
    getBlocks() const noexcept
    {
        return blocks_;
    }

    Returns const&
    getReturns() const noexcept
    {
        return returns_;
    }

    List<Param> const&
    getParams() const noexcept
    {
        return params_;
    }

    List<TParam> const&
    getTParams() const noexcept
    {
        return tparams_;
    }

    //---

    Javadoc() noexcept;

    /** Constructor
    */
    Javadoc(
        List<Block> blocks,
        List<Param> params,
        List<TParam> tparams,
        Returns returns);

    bool operator<(Javadoc const&) const noexcept;
    bool operator==(Javadoc const&) const noexcept;

    /** Append a node to the documentation comment.,
    */
    template<class Child>
    void
    emplace_back(Child&& node)
    {
        static_assert(std::is_base_of_v<Node, Child>);

        blocks_.emplace_back<Child>(std::forward<Child>(node));
    }

    void
    emplace_back(Param param)
    {
        params_.emplace_back(std::move(param));
    }

    void
    emplace_back(TParam tparam)
    {
        tparams_.emplace_back(std::move(tparam));
    }

    void merge(Javadoc& other);

bool dummy = false;

private:
    static Paragraph const s_empty_;

    Paragraph const* brief_;
    List<Block> blocks_;
    List<Param> params_;
    List<TParam> tparams_;
    Returns returns_;
};

} // mrdox
} // clang

#endif
