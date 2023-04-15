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

//===--- SymbolDocumentation.cpp ==-------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SymbolDocumentation.hpp"
#include "clang/AST/CommentVisitor.h"
#include "llvm/Support/JSON.h"

namespace clang {
namespace mrdox {

void ensureUTF8(std::string& Str)
{
    if (!llvm::json::isUTF8(Str))
        Str = llvm::json::fixUTF8(Str);
}

void ensureUTF8(llvm::MutableArrayRef<std::string> Strings)
{
    for (auto& Str : Strings) {
        ensureUTF8(Str);
    }
}

//------------------------------------------------

class BlockCommentToString
    : public comments::ConstCommentVisitor<BlockCommentToString>
{
public:
    BlockCommentToString(std::string& Out, const ASTContext& Ctx)
        : Out(Out), Ctx(Ctx)
    {
    }

    void visitParagraphComment(const comments::ParagraphComment* C)
    {
        for (const auto* Child = C->child_begin(); Child != C->child_end();
            ++Child) {
            visit(*Child);
        }
    }

    void visitBlockCommandComment(const comments::BlockCommandComment* B)
    {
        Out << (B->getCommandMarker() == (comments::CommandMarkerKind::CMK_At)
            ? '@'
            : '\\')
            << B->getCommandName(Ctx.getCommentCommandTraits());

        visit(B->getParagraph());
    }

    void visitTextComment(const comments::TextComment* C)
    {
        // If this is the very first node, the paragraph has no doxygen command,
        // so there will be a leading space -> Trim it
        // Otherwise just trim trailing space
        if (Out.str().empty())
            Out << C->getText().trim();
        else
            Out << C->getText().rtrim();
    }

    void visitInlineCommandComment(const comments::InlineCommandComment* C)
    {
        const std::string SurroundWith = [C]
        {
            switch (C->getRenderKind()) {
            case comments::InlineCommandComment::RenderKind::RenderMonospaced:
                return "`";
            case comments::InlineCommandComment::RenderKind::RenderBold:
                return "**";
            case comments::InlineCommandComment::RenderKind::RenderEmphasized:
                return "*";
            default:
                return "";
            }
        }();

        Out << " " << SurroundWith;
        for (unsigned I = 0; I < C->getNumArgs(); ++I) {
            Out << C->getArgText(I);
        }
        Out << SurroundWith;
    }

private:
    llvm::raw_string_ostream Out;
    const ASTContext& Ctx;
};

//------------------------------------------------

class CommentToSymbolDocumentation
    : public comments::ConstCommentVisitor<CommentToSymbolDocumentation>
{
public:
    CommentToSymbolDocumentation(const RawComment& RC, const ASTContext& Ctx,
        const Decl* D, SymbolDocumentation& Doc)
        : FullComment(RC.parse(Ctx, nullptr, D)), Output(Doc), Ctx(Ctx)
    {
        Doc.CommentText =
            RC.getFormattedText(Ctx.getSourceManager(), Ctx.getDiagnostics());

        for (auto* Block : FullComment->getBlocks())
        {
            visit(Block);
        }
    }

    void visitBlockCommandComment(const comments::BlockCommandComment* B)
    {
        const llvm::StringRef CommandName =
            B->getCommandName(Ctx.getCommentCommandTraits());

        // Visit B->getParagraph() for commands that we have special fields for,
        // so that the command name won't be included in the string.
        // Otherwise, we want to keep the command name, so visit B itself.
        if (CommandName == "brief")
        {
            BlockCommentToString(Output.Brief, Ctx).visit(B->getParagraph());
        }
        else if (CommandName == "return")
        {
            BlockCommentToString(Output.Returns, Ctx).visit(B->getParagraph());
        }
        else if (CommandName == "warning")
        {
            BlockCommentToString(Output.Warnings.emplace_back(), Ctx)
                .visit(B->getParagraph());
        }
        else if (CommandName == "note")
        {
            BlockCommentToString(Output.Notes.emplace_back(), Ctx)
                .visit(B->getParagraph());
        }
        else {
            if (!Output.Description.empty())
                Output.Description += "\n\n";

            BlockCommentToString(Output.Description, Ctx).visit(B);
        }
    }

    void visitParagraphComment(const comments::ParagraphComment* P)
    {
        BlockCommentToString(Output.Description, Ctx).visit(P);
    }

    void visitParamCommandComment(const comments::ParamCommandComment* P)
    {
        if (P->hasParamName() && P->hasNonWhitespaceParagraph()) {
            ParameterDocumentation Doc;
            Doc.Name = P->getParamNameAsWritten().str();
            BlockCommentToString(Doc.Description, Ctx).visit(P->getParagraph());
            Output.Parameters.push_back(std::move(Doc));
        }
    }

private:
    comments::FullComment* FullComment;
    SymbolDocumentation& Output;
    const ASTContext& Ctx;
};

//------------------------------------------------

SymbolDocumentation
parseDoxygenComment(
    const RawComment& RC,
    const ASTContext& Ctx,
    const Decl* D)
{
    SymbolDocumentation Doc;
    CommentToSymbolDocumentation(RC, Ctx, D, Doc);

    // Clang requires source to be UTF-8, but doesn't enforce this in comments.
    ensureUTF8(Doc.Brief);
    ensureUTF8(Doc.Returns);

    ensureUTF8(Doc.Notes);
    ensureUTF8(Doc.Warnings);

    for (auto& Param : Doc.Parameters) {
        ensureUTF8(Param.Name);
        ensureUTF8(Param.Description);
    }

    ensureUTF8(Doc.Description);
    ensureUTF8(Doc.CommentText);

    return Doc;
}

} // mrdox
} // clang
