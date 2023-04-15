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
#include <clang/AST/CommentVisitor.h>
#include <clang/AST/CommentCommandTraits.h>
#include <llvm/Support/JSON.h>

/*
    Comment Types

    Comment
        abstract base for all comments

        FullComment : Comment
            The entire extracted comment(s) attached to a declaration.

        InlineContentComment : Comment
            contained within a block, abstract

            TextComment : InlineContentComment
                plain text

            InlineCommandComment : InlineContentComment
                command with args as inline content

            HTMLTagComment : InlineContentComment
                Abstract class for opening and closing HTML tags, inline content

                HTMLStartTagComment : HTMLTagComment
                    opening HTML tag with attributes.

                HTMLEndTagComment : HTMLTagComment
                     closing HTML tag.

        BlockContentComment : Comment
            Block content (contains inline content). abstract

            ParagraphComment : BlockContentComment
                A single paragraph that contains inline content.

            BlockCommandComment : BlockContentComment
                zero or more word-like arguments, then a paragraph

                ParamCommandComment : BlockCommandComment
                    describes a parameter

                TParamCommandComment : BlockCommandComment
                    describes a template parameter.

                VerbatimBlockComment : BlockCommandComment
                    A verbatim block command (e. g., preformatted code). Verbatim
                    block has an opening and a closing command and contains multiple
                    lines of text (VerbatimBlockLineComment nodes).

                VerbatimLineComment : BlockCommandComment
                    A verbatim line command.  Verbatim line has an opening command,
                    a single line of text (up to the newline after the opening command)
                    and has no closing command.

        VerbatimBlockLineComment : Comment
            A line of text contained in a verbatim block.
*/
/*
    BlockCommandComment
        Always has one child of type ParagraphComment child?
*/

namespace clang {

#if 0
namespace comments {
#include <clang/AST/CommentCommandInfo.inc>
} // comments
#endif

namespace mrdox {

namespace {

using namespace comments;

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

template<class T, class R, class... Args>
void
visit_children(
    ConstCommentVisitor<T, R, Args...>& visitor,
    Comment const& C)
{
    for(auto const* it = C.child_begin();
        it != C.child_end();
        ++it)
    {
        visitor.visit(*it);
    }
}

//------------------------------------------------

/** Visitor which applies inline text styling commands.
*/
struct InlineStyler
{
    explicit
    InlineStyler(
        llvm::raw_string_ostream& os)
        : os_(os)
    {
    }

    bool
    handleInlineStyle(
        InlineCommandComment const* C)
    {
        char const SurroundWith = [C]
        {
            switch (C->getRenderKind())
            {
            case InlineCommandComment::RenderKind::RenderMonospaced:
                return '`';
            case InlineCommandComment::RenderKind::RenderBold:
                return '*';
            case InlineCommandComment::RenderKind::RenderEmphasized:
                return '_';
            default:
                return char(0);
            }
        }();

        // VFALCO note, mixing styles must be done
        // in the correct order. This is canonical:
        //
        //      `*_monospace bold italic phrase_*`
        //      ``**__char__**``acter``**__s__**``
        //
        // See: https://docs.asciidoctor.org/asciidoc/latest/text/italic/#mixing-italic-with-other-formatting
        //
        // It looks like the clang parser does not
        // emit nested styles, so only one inline
        // style command can be applied per args.
        //

        if(SurroundWith != 0)
            os_ << " " << SurroundWith;
        for (unsigned I = 0; I < C->getNumArgs(); ++I)
        {
            // VFALCO args whose characters match
            // in SurroundWith need to be escaepd
            os_ << C->getArgText(I);
        }
        if(SurroundWith != 0)
            os_ << SurroundWith;
        return true;
    }

private:
    llvm::raw_string_ostream& os_;
};

//------------------------------------------------

/** Visitor for the first paragraph in a FullComment.

    This will represent:

    @li The implicit brief if no explicit brief command is found,

    @li A regular paragraph containing an explicit
        brief and the surrounding paragraphs as
        exposition.
*/
struct FirstParagraph
    : ConstCommentVisitor<FirstParagraph>
    , InlineStyler
{
    explicit
    FirstParagraph(
        std::string& s,
        ASTContext const& ctx)
        : InlineStyler(os_)
        , s_(s)
        , os_(s_)
        , ctx_(ctx)
    {
    }

    ~FirstParagraph()
    {
        // clear the string if it
        // only consists of whitespace
        llvm::StringRef s(s_);
        s = s.ltrim();
        if(! s.empty())
            s_.erase(0, s_.size() - s.size()),
            s_.clear();
    }

    void
    start(
        ParagraphComment const* C)
    {
        visit_children(*this, *C);
    }

    void visitTextComment(
        TextComment const* C)
    {
        // If this is the very first node, the
        // paragraph has no doxygen command,
        // so there will be a leading space -> Trim it
        // Otherwise just trim trailing space
        if (s_.empty())
            s_.append(C->getText().trim());
        else
            s_.append(C->getText().rtrim());
    }

    void
    visitInlineCommandComment(
        InlineCommandComment const* C)
    {
        if(handleInlineStyle(C))
            return;
    }

    void
    visitBlockCommandComment(
        BlockCommandComment const* B)
    {
        auto const* cmd = ctx_
            .getCommentCommandTraits()
            .getCommandInfo(B->getCommandID());
        if( cmd != nullptr &&
            cmd->IsBriefCommand)
        {
            // handle brief
            gotBrief_ = true;
        }
    }

private:
    std::string& s_;
    llvm::raw_string_ostream os_;
    bool gotBrief_ = false;
    ASTContext const& ctx_;

};

//------------------------------------------------

class BlockCommentToString
    : public ConstCommentVisitor<BlockCommentToString>
{
public:
    BlockCommentToString(std::string& os_, const ASTContext& Ctx)
        : os_(os_), Ctx(Ctx)
    {
    }

    void visitParagraphComment(const ParagraphComment* C)
    {
        visit_children(*this, *C);
    }

    void visitBlockCommandComment(const BlockCommandComment* B)
    {
        os_ << (B->getCommandMarker() == (CommandMarkerKind::CMK_At)
            ? '@'
            : '\\')
            << B->getCommandName(Ctx.getCommentCommandTraits());

        visit(B->getParagraph());
    }

    void visitTextComment(const TextComment* C)
    {
        // If this is the very first node, the paragraph has no doxygen command,
        // so there will be a leading space -> Trim it
        // Otherwise just trim trailing space
        if (os_.str().empty())
            os_ << C->getText().trim();
        else
            os_ << C->getText().rtrim();
    }

    void visitInlineCommandComment(const InlineCommandComment* C)
    {
        const std::string SurroundWith = [C]
        {
            switch (C->getRenderKind()) {
            case InlineCommandComment::RenderKind::RenderMonospaced:
                return "`";
            case InlineCommandComment::RenderKind::RenderBold:
                return "**";
            case InlineCommandComment::RenderKind::RenderEmphasized:
                return "*";
            default:
                return "";
            }
        }();

        os_ << " " << SurroundWith;
        for (unsigned I = 0; I < C->getNumArgs(); ++I) {
            os_ << C->getArgText(I);
        }
        os_ << SurroundWith;
    }

private:
    llvm::raw_string_ostream os_;
    const ASTContext& Ctx;
};

//------------------------------------------------

struct RawCommentVisitor
    : ConstCommentVisitor<RawCommentVisitor>
{
    RawCommentVisitor(
        SymbolDocumentation& Doc,
        RawComment const& RC,
        ASTContext const& Ctx,
        Decl const* D)
        : Output(Doc)
        , Ctx(Ctx)
    {
        // Doc.CommentText = RC.getFormattedText(Ctx.getSourceManager(), Ctx.getDiagnostics());
        FullComment* fullComment(RC.parse(Ctx, nullptr, D));

        // Special case: the first paragraph is the
        // implicit brief if no @brief command is found.
        auto const& blocks = fullComment->getBlocks();
        BlockContentComment const* const* it = blocks.begin();
        if(it == blocks.end())
            return;
        if(auto paragraphComment = dyn_cast<ParagraphComment>(*it))
        {
            std::string text;
            FirstParagraph visitor(text, Ctx);
            visitor.start(paragraphComment);
            ++it;
        }
        for(;it != blocks.end(); ++it)
        {
            visit(*it);
        }
    }

    void
    visitInlineContentComment(
        InlineContentComment const* C)
    {
    }

    void
    visitBlockCommandComment(
        BlockCommandComment const* B)
    {
        llvm::StringRef const CommandName =
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

    void visitParagraphComment(const ParagraphComment* P)
    {
        BlockCommentToString(Output.Description, Ctx).visit(P);
    }

    void visitParamCommandComment(const ParamCommandComment* P)
    {
        if (P->hasParamName() && P->hasNonWhitespaceParagraph()) {
            ParameterDocumentation Doc;
            Doc.Name = P->getParamNameAsWritten().str();
            BlockCommentToString(Doc.Description, Ctx).visit(P->getParagraph());
            Output.Parameters.push_back(std::move(Doc));
        }
    }

private:
    SymbolDocumentation& Output;
    ASTContext const& Ctx;
};

template<class Pred>
void
dumpCommandTraits(
    char const* title,
    llvm::raw_ostream& os,
    Pred&& pred)
{
    using namespace comments;

    std::vector<CommandInfo const*> list;
    list.reserve(CommandTraits::KCI_Last);
    for(std::size_t id = 0; id < CommandTraits::KCI_Last; ++id)
    {
        auto const& cmd = *CommandTraits::getBuiltinCommandInfo(id);
        if(pred(cmd))
            list.push_back(&cmd);
    }
    std::sort(list.begin(), list.end(),
        []( CommandInfo const* cmd0,
            CommandInfo const* cmd1)
        {
            llvm::StringRef s0(cmd0->Name);
            llvm::StringRef s1(cmd1->Name);
            return s0.compare(s1) < 0;
        });
    for(auto const& cmd : list)
    {
        if(title)
        {
            os << '\n' << title << '\n';
            title = nullptr;
        }
        os << '\\' << cmd->Name;
        if(cmd->EndCommandName && cmd->EndCommandName[0])
            os << ", \\" << cmd->EndCommandName << "\\";
        if(cmd->NumArgs > 0)
            os << " [" << std::to_string(cmd->NumArgs) << "]";

    #if 0
        if(cmd->IsInlineCommand)
            os << " inline";
        if(cmd->IsBlockCommand)
            os << " block";
        if(cmd->IsVerbatimBlockCommand)
            os << " verbatim";
        if(cmd->IsVerbatimBlockEndCommand)
            os << " verbatim-end";
        if(cmd->IsVerbatimLineCommand)
            os << " verbatim-line";
    #endif

        if(cmd->IsBriefCommand)
            os << " brief";
        if(cmd->IsReturnsCommand)
            os << " returns";
        if(cmd->IsParamCommand)
            os << " param";
        if(cmd->IsTParamCommand)
            os << " tparam";
        if(cmd->IsThrowsCommand)
            os << " throws";
        if(cmd->IsDeprecatedCommand)
            os << " deprecated";
        if(cmd->IsHeaderfileCommand)
            os << " header";
        if(cmd->IsBlockCommand)
        {
            if(cmd->IsEmptyParagraphAllowed)
                os << " empty-ok";
            else
                os << " no-empty";
        }
        if(cmd->IsDeclarationCommand)
            os << " decl";
        if(cmd->IsFunctionDeclarationCommand)
            os << " fn-decl";
        if(cmd->IsRecordLikeDetailCommand)
            os << " record-detail";
        if(cmd->IsRecordLikeDeclarationCommand)
            os << " record-decl";
        if(cmd->IsUnknownCommand)
            os << " unknown";
        os << '\n';
    }
}

} // (anon)

//------------------------------------------------

void
dumpCommentTypes()
{
    auto& os = llvm::outs();

    #define COMMENT(Type, Base) os << #Type << " : " << #Base << '\n';
    //#define COMMENT_RANGE(Base, First, Last)
    //#define LAST_COMMENT_RANGE(Base, First, Last)
    //#define ABSTRACT_COMMENT(Type, Base) os << #Type << #Base << '\n';
    #include <clang/AST/CommentNodes.inc>
    os << "\n\n";
}

void
dumpCommentCommands()
{
    auto& os = llvm::outs();

    dumpCommandTraits(
        "Inline Commands\n"
        "---------------", os,
        [](CommandInfo const& cmd) -> bool
        {
            return cmd.IsInlineCommand;
        });

    dumpCommandTraits(
        "Block Commands\n"
        "--------------", os,
        [](CommandInfo const& cmd) -> bool
        {
            return cmd.IsBlockCommand;
        });

    dumpCommandTraits(
        "Verbatim Commands\n"
        "-----------------", os,
        [](CommandInfo const& cmd) -> bool
        {
            return
                cmd.IsVerbatimBlockCommand ||
                cmd.IsVerbatimBlockEndCommand ||
                cmd.IsVerbatimLineCommand;
        });
}

SymbolDocumentation
parseDoxygenComment(
    RawComment const& RC,
    ASTContext const& Ctx,
    Decl const* D)
{
    SymbolDocumentation Doc;

    RawCommentVisitor visitor(Doc, RC, Ctx, D);

    // Clang requires source to be UTF-8,
    // but doesn't enforce this in comments.
    ensureUTF8(Doc.Brief);
    ensureUTF8(Doc.Returns);

    ensureUTF8(Doc.Notes);
    ensureUTF8(Doc.Warnings);

    for (auto& Param : Doc.Parameters)
    {
        ensureUTF8(Param.Name);
        ensureUTF8(Param.Description);
    }

    ensureUTF8(Doc.Description);
    ensureUTF8(Doc.CommentText);

    return Doc;
}

} // mrdox
} // clang
