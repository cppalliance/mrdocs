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

#ifndef MRDOX_METADATA_JAVADOC_HPP
#define MRDOX_METADATA_JAVADOC_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/List.hpp>
#include <llvm/ADT/SmallString.h>
#include <memory>
#include <string>
#include <utility>

namespace clang {
namespace mrdox {

/** A processed Doxygen-style comment attached to a declaration.
*/
struct MRDOX_VISIBLE
    Javadoc
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

    //--------------------------------------------

    /** This is a variant-like list element.
    */
    struct Node
    {
        Kind kind;

        auto operator<=>(Node const&
            ) const noexcept = default;

        explicit Node(Kind kind_) noexcept
            : kind(kind_)
        {
        }
    };

    /** A string of plain text.
    */
    struct Text : Node
    {
        String string;

        static constexpr Kind static_kind = Kind::text;

        auto operator<=>(Text const&
            ) const noexcept = default;

        explicit
        Text(
            String string_ = String())
            : Node(Kind::text)
            , string(std::move(string_))
        {
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

        auto operator<=>(StyledText const&
            ) const noexcept = default;

        StyledText(
            String string_ = String(),
            Style style_ = Style::none)
            : Text(std::move(string_), Kind::styled)
            , style(style_)
        {
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

        auto operator<=>(Block const&
            ) const noexcept = default;

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

        auto operator<=>(Paragraph const&
            ) const noexcept = default;

        Paragraph() noexcept
            : Block(Kind::paragraph)
        {
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

        auto operator<=>(Brief const&
            ) const noexcept = default;

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

        auto operator<=>(Admonition const&
            ) const noexcept = default;

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

        static constexpr Kind static_kind = Kind::code;

        auto operator<=>(Code const&
            ) const noexcept = default;

        Code()
            : Paragraph(Kind::code)
        {
        }
    };

    /** Documentation for a function parameter
    */
    struct Param : Paragraph
    {
        String name;

        static constexpr Kind static_kind = Kind::param;

        auto operator<=>(Param const&
            ) const noexcept = default;

        Param(
            String name_ = String(),
            Paragraph details_ = Paragraph())
            : Paragraph(
                Kind::param,
                std::move(details_.children))
            , name(std::move(name_))
        {
        }
    };

    /** Documentation for a template parameter
    */
    struct TParam : Paragraph
    {
        String name;

        static constexpr Kind static_kind = Kind::tparam;

        auto operator<=>(TParam const&
            ) const noexcept = default;

        TParam()
            : Paragraph(Kind::tparam)
        {
        }
    };

    /** Documentation for a function return type
    */
    struct Returns : Paragraph
    {
        static constexpr Kind static_kind = Kind::returns;

        auto operator<=>(Returns const&
            ) const noexcept = default;

        Returns()
            : Paragraph(Kind::returns)
        {
        }
    };

    //--------------------------------------------

    MRDOX_DECL
    Javadoc() noexcept;

    /** Constructor
    */
    MRDOX_DECL
    explicit
    Javadoc(
        List<Block> blocks);

    /** Return true if this is empty
    */
    MRDOX_DECL
    bool
    empty() const noexcept;

    /** Return the brief, or nullptr if there is none.

        This function should only be called
        after calculateBrief() has been invoked.
    */
    Paragraph const*
    getBrief() const noexcept
    {
        return brief_.get();
    }

    /** Return the list of top level blocks.
    */
    List<Block> const&
    getBlocks() const noexcept
    {
        return blocks_;
    }

    // VFALCO This is unfortunately necessary for
    //        the deserialization from bitcode...
    List<Block>&
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

    MRDOX_DECL
    List<Block>::const_iterator
    findBrief() const noexcept;

    /** Calculate the brief.

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
    */
    MRDOX_DECL 
    void
    calculateBrief();

    //--------------------------------------------

    /** These are used to bottleneck all insertions.
    */
    /** @{ */
    template<class T, class U>
    static
    void
    append(List<T>& list, List<U>&& other) noexcept
    {
        list.splice_back(std::move(other));
    }

    template<class T, class Child>
    static
    void
    append(List<T>& list, Child&& child)
    {
        list.emplace_back(
            std::forward<Child>(child));
    }

    template<class Parent, class Child>
    static
    void
    append(Parent& parent, Child&& child)
    {
        append(parent.children,
            std::forward<Child>(child));
    }

    template<class Child>
    static
    void
    append(Paragraph& parent, Child&& child)
    {
        append(parent.children,
            std::forward<Child>(child));
    }
    /** @} */

    /** Add a top level element to the doc comment.
    */
    void append(Block node)
    {
        append(blocks_, std::move(node));
    }

    //--------------------------------------------

private:
    std::shared_ptr<Paragraph const> brief_;
    List<Block> blocks_;
};

} // mrdox
} // clang

#endif
