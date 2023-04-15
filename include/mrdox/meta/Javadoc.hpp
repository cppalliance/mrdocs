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
        code,
        styledText,
        paragraph
    };

    /** This is a variant-like list element.
    */
    struct Node
    {
        Kind const kind;

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

        explicit
        Text(
            String text_,
            Kind kind_ = Kind::text)
            : Node(kind_)
            , text(std::move(text))
        {
        }
    };

    /** A string of plain text representing source code.
    */
    struct Code : Text
    {
        // VFALCO this can have a language (e.g. C++),
        //        and we can emit attributes in the generator.

        explicit
        Code(
            String text_)
            : Text(std::move(text_), Kind::code)
        {
        }
    };

    /** A text style.
    */
    enum class Style
    {
        mono,
        bold,
        italic
    };

    /** A piece of style text.
    */
    struct StyledText : Text
    {
        Style style;

        StyledText(
            String text,
            Style style_)
            : Text(std::move(text), Kind::styledText)
            , style(style_)
        {
        }
    };

    /** A sequence of text nodes.
    */
    struct Paragraph : Node
    {
        Paragraph()
            : Node(Kind::paragraph)
        {
        }

        List<Text> list;
    };

    // VFALCO LEGACY
    llvm::SmallString<32> brief;
    std::string desc;

    //---

    ~Javadoc()
    {
        if(brief_)
            delete brief_;
    }

    Javadoc() = default;

    Paragraph const*
    getBrief() const noexcept
    {
        return brief_;
    }

    List<Node> const&
    getNodes() const noexcept
    {
        return list_;
    }

    /** Append a node to the documentation comment.,
    */
    template<class Child>
    void
    append(Child&& node)
    {
        static_assert(std::is_base_of<Node, Child>);

        list_.emplace_back<Child>(std::forward<Child>(node));
    }

    /** Append a brief to the documentation comment.,
    */
    void
    append_brief(Paragraph&& paragraph)
    {
        assert(brief_ == nullptr);
        brief_ = new Paragraph(std::move(paragraph));
    }

private:
    List<Node> list_;

    Paragraph const* brief_ = nullptr;
};

} // mrdox
} // clang

#endif
