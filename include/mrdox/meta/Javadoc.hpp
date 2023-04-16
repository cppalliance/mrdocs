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

/** A single verbatim block.
*/
struct VerbatimBlock
{
    std::string text;
};

// A representation of a parsed comment.
struct CommentInfo
{
    CommentInfo() = default;
    CommentInfo(CommentInfo& Other) = delete;
    CommentInfo(CommentInfo&& Other) = default;
    CommentInfo& operator=(CommentInfo&& Other) = default;

    bool operator==(const CommentInfo& Other) const;

    // This operator is used to sort a vector of CommentInfos.
    // No specific order (attributes more important than others) is required. Any
    // sort is enough, the order is only needed to call std::unique after sorting
    // the vector.
    bool operator<(const CommentInfo& Other) const;

    llvm::SmallString<16> Kind;
        // Kind of comment (FullComment, ParagraphComment, TextComment,
        // InlineCommandComment, HTMLStartTagComment, HTMLEndTagComment,
        // BlockCommandComment, ParamCommandComment,
        // TParamCommandComment, VerbatimBlockComment,
        // VerbatimBlockLineComment, VerbatimLineComment).
    llvm::SmallString<64> Text;      // Text of the comment.
    llvm::SmallString<16> Name;      // Name of the comment (for Verbatim and HTML).
    llvm::SmallString<8> Direction;  // Parameter direction (for (T)ParamCommand).
    llvm::SmallString<16> ParamName; // Parameter name (for (T)ParamCommand).
    llvm::SmallString<16> CloseName; // Closing tag name (for VerbatimBlock).
    bool SelfClosing = false;  // Indicates if tag is self-closing (for HTML).
    bool Explicit = false; // Indicates if the direction of a param is explicit
    // (for (T)ParamCommand).
    llvm::SmallVector<llvm::SmallString<16>, 4> AttrKeys; // List of attribute keys (for HTML).
    llvm::SmallVector<llvm::SmallString<16>, 4> AttrValues; // List of attribute values for each key (for HTML).
    llvm::SmallVector<llvm::SmallString<16>, 4> Args; // List of arguments to commands (for InlineCommand).
    std::vector<std::unique_ptr<CommentInfo>> Children; // List of child comments for this CommentInfo.
};

//------------------------------------------------

/** A processed Javadoc-style comment attached to a declaration.
*/
struct Javadoc
{
    using String = std::string;

    enum class Kind
    {
        text,
        styledText,
        paragraph,
        admonition,
        code,
        returns,
        param,
        tparam
    };

    /** A text style.
    */
    enum class Style
    {
        none,
        mono,
        bold,
        italic
    };

    /** An admonishment style.
    */
    enum class Admonish
    {
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
            String text_)
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
            String text,
            Style style_)
            : Text(std::move(text), Kind::styledText)
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

        Paragraph()
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

    /** Documentation for an admonition
    */
    struct Admonition : Paragraph
    {
        Admonish style;

        auto operator<=>(Admonition const&) const noexcept = default;

        explicit
        Admonition(Admonish style_)
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

    /** Documentation for a function parameter
    */
    struct Param : Block
    {
        String name;
        Paragraph details;

        auto operator<=>(Param const&) const noexcept = default;

        Param(
            String name_,
            Paragraph details_)
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
            String name_,
            Paragraph details_)
            : Block(Kind::param)
            , name(std::move(name_))
            , details(std::move(details_))
        {
        }
    };

    // VFALCO LEGACY
    llvm::SmallString<32> brief;
    std::string desc;

    //---

    std::shared_ptr<Paragraph> const&
    getBrief() const noexcept
    {
        return brief_;
    }

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

    Javadoc() = default;

    /** Constructor
    */
    Javadoc(
        Paragraph brief,
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

private:
    std::shared_ptr<Paragraph> brief_;
    List<Block> blocks_;
    List<Param> params_;
    List<TParam> tparams_;
    Returns returns_;
};

} // mrdox
} // clang

#endif
