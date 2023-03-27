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

#ifndef MRDOX_COMMENT_VISITOR_HPP
#define MRDOX_COMMENT_VISITOR_HPP

#include "Representation.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 5054) // C5054: operator '+': deprecated between enumerations of different types
#include <clang/AST/Comment.h>
#include <clang/AST/CommentVisitor.h>
#pragma warning(pop)
#endif

namespace clang {
namespace doc {
namespace serialize {

// VFALCO refactor this
using namespace comments;

class CommentVisitor
    : public ConstCommentVisitor<CommentVisitor>
{
public:
  CommentVisitor(CommentInfo &CI) : CurrentCI(CI) {}

  void parseComment(const comments::Comment *C);

  void visitTextComment(const TextComment *C);
  void visitInlineCommandComment(const InlineCommandComment *C);
  void visitHTMLStartTagComment(const HTMLStartTagComment *C);
  void visitHTMLEndTagComment(const HTMLEndTagComment *C);
  void visitBlockCommandComment(const BlockCommandComment *C);
  void visitParamCommandComment(const ParamCommandComment *C);
  void visitTParamCommandComment(const TParamCommandComment *C);
  void visitVerbatimBlockComment(const VerbatimBlockComment *C);
  void visitVerbatimBlockLineComment(const VerbatimBlockLineComment *C);
  void visitVerbatimLineComment(const VerbatimLineComment *C);

private:
  std::string getCommandName(unsigned CommandID) const;
  bool isWhitespaceOnly(StringRef S) const;

  CommentInfo &CurrentCI;
};

} // namespace serialize
} // namespace doc
} // namespace clang

#endif
