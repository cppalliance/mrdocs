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

#ifndef MRDOX_JAVADOC_HPP
#define MRDOX_JAVADOC_HPP

#include <llvm/ADT/SmallString.h>
#include <string>

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

/** A complete javadoc attached to a declaration
*/
struct Javadoc
{
    /** The brief description.
    */
    llvm::SmallString<32> brief;

    /** The detailed description.
    */
    //llvm::SmallString<32> desc; // asciidoc
    std::string desc; // asciidoc
};

} // mrdox
} // clang

#endif
