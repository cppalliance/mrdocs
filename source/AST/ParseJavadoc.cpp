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

#include "ParseJavadoc.hpp"
#include <mrdox/Metadata/Javadoc.hpp>
#include <mrdox/Support/String.hpp>
#include <clang/AST/CommentCommandTraits.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RawCommentList.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 5054) // C5054: operator '+': deprecated between enumerations of different types
#endif
#include <clang/AST/Comment.h>
#include <clang/AST/CommentVisitor.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

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
namespace mrdox {

namespace {

using namespace comments;

//------------------------------------------------

template<class U, class T>
struct Scope
{
    Scope(
        U& u,
        T*& t0) noexcept
        : t0_(t0)
        , pt_(t0)
    {
        pt_ = &u;
    }

    ~Scope()
    {
        pt_ = t0_;
    }

private:
    T* t0_;
    T*& pt_;
};

template<class U, class T>
Scope(U&, T*&) -> Scope<T, T>;

//------------------------------------------------

class JavadocVisitor
    : public ConstCommentVisitor<JavadocVisitor>
{
    static
    std::string&
    ensureUTF8(
        std::string&& s)
    {
        if (!llvm::json::isUTF8(s))
            s = llvm::json::fixUTF8(s);
        return s;
    }

    template<class... Args>
    void visitChildren(
        Comment const* C)
    {
        for(auto const* it = C->child_begin();
                it != C->child_end(); ++it)
            visit(*it);
    }

public:
    JavadocVisitor(
        RawComment const* RC,
        Decl const* D)
        : FC_(RC->parse(D->getASTContext(), nullptr, D))
        , ctx_(D->getASTContext())
    {
    }

    Javadoc build()
    {
        visit(FC_);

        // This must cause has_value() to return true
        return Javadoc(std::move(blocks_));
    }

    void visitComment(
        Comment const* C)
    {
        visitChildren(C);
    }

    void visitTextComment(
        TextComment const* C)
    {
        llvm::StringRef s = C->getText();
        // If this is the very first node, the
        // paragraph has no doxygen command,
        // so we will trim the leading space.
        // Otherwise just trim trailing space
#if 0
        if(paragraph_->children.empty())
            s = s.ltrim().rtrim();
        else
            s = s.rtrim(); //.ltrim()
#else
        s = s.rtrim();
#endif

        // VFALCO Figure out why we get empty TextComment
#if 0
        if(! s.empty())
#endif
            Javadoc::append(*paragraph_, std::make_unique<
                doc::Text>(ensureUTF8(s.str())));
    }

    void visitHTMLTagComment(
        HTMLTagComment const* C)
    {
        visitChildren(C);
    }

    void visitHTMLStartTagComment(
        HTMLStartTagComment const* C)
    {
        visitChildren(C);
    }

    void HTMLEndTagComment(
        HTMLStartTagComment const* C)
    {
    }

    void visitInlineCommandComment(
        InlineCommandComment const* C)
    {
        doc::Style style(doc::Style::none);
        switch (C->getRenderKind())
        {
        case InlineCommandComment::RenderKind::RenderMonospaced:
            style = doc::Style::mono;
            break;
        case InlineCommandComment::RenderKind::RenderBold:
            style = doc::Style::bold;
            break;
        case InlineCommandComment::RenderKind::RenderEmphasized:
            style = doc::Style::italic;
            break;
        case InlineCommandComment::RenderKind::RenderNormal:
        case InlineCommandComment::RenderKind::RenderAnchor:
            break;
        default:
            // unknown RenderKind
            MRDOX_UNREACHABLE();
        }

        // It looks like the clang parser does not
        // emit nested styles, so only one inline
        // style command can be applied per args.

        doc::String s;
        std::size_t n = 0;
        for(unsigned i = 0; i < C->getNumArgs(); ++i)
            n += C->getArgText(i).size();
        s.reserve(n);
        for(unsigned i = 0; i < C->getNumArgs(); ++i)
            s.append(C->getArgText(i));

        if(style != doc::Style::none)
            Javadoc::append(*paragraph_, std::make_unique<
                doc::StyledText>(std::move(s), style));
        else
            Javadoc::append(*paragraph_, std::make_unique<
                doc::Text>(std::move(s)));
    }

    //
    // Block Content
    //

    void visitParagraphComment(
        ParagraphComment const* C)
    {
        if(paragraph_)
            return visitChildren(C);
        doc::Paragraph paragraph;
        Scope scope(paragraph, paragraph_);
        visitChildren(C);
        // VFALCO Figure out why we get empty ParagraphComment
        if(! paragraph.empty())
            Javadoc::append(blocks_, std::make_unique<
                doc::Paragraph>(std::move(paragraph)));
    }

    void
    visitBlockCommandComment(
        BlockCommandComment const* C)
    {
        auto const* cmd = ctx_
            .getCommentCommandTraits()
            .getCommandInfo(C->getCommandID());
        if(cmd == nullptr)
        {
            // ignore this command and the
            // text that follows for now.
            return;
        }
        if(cmd->IsBriefCommand)
        {
            doc::Brief brief;
            Scope scope(brief, paragraph_);
            visitChildren(C->getParagraph());
            Javadoc::append(blocks_, std::make_unique<
                doc::Brief>(std::move(brief)));
            return;
        }
        if(cmd->IsReturnsCommand)
        {
            doc::Returns returns;
            Scope scope(returns, paragraph_);
            visitChildren(C->getParagraph());
            Javadoc::append(blocks_, std::make_unique<
                doc::Returns>(std::move(returns)));
            return;
        }
        if(cmd->getID() == CommandTraits::KCI_note)
        {
            doc::Admonition paragraph(doc::Admonish::note);
            Scope scope(paragraph, paragraph_);
            visitChildren(C->getParagraph());
            Javadoc::append(blocks_, std::make_unique<
                doc::Admonition>(std::move(paragraph)));
            return;
        }
        if(cmd->getID() == CommandTraits::KCI_warning)
        {
            doc::Admonition paragraph(doc::Admonish::warning);
            Scope scope(paragraph, paragraph_);
            visitChildren(C->getParagraph());
            Javadoc::append(blocks_, std::make_unique<
                doc::Admonition>(std::move(paragraph)));
            return;
        }
        if(cmd->getID() == CommandTraits::KCI_par)
        {
            // VFALCO This is legacy compatibility
            // for Boost libraries using @par as a
            // section heading.
            doc::Paragraph paragraph;
            Scope scope(paragraph, paragraph_);
            visitChildren(C->getParagraph());
            if(! paragraph.children.empty())
            {
                // first TextComment is the heading text
                doc::String text(std::move(
                    paragraph.children.front()->string));

                // VFALCO Unfortunately clang puts at least
                // one space in front of the text, which seems
                // incorrect.
                auto const s = trim(text);
                if(s.size() != text.size())
                    text = s;

                doc::Heading heading(std::move(text));
                Javadoc::append(blocks_, std::make_unique<
                    doc::Heading>(std::move(heading)));

                // remaining TextComment, if any
                paragraph.children.erase(paragraph.children.begin());
                if(! paragraph.children.empty())
                Javadoc::append(blocks_, std::make_unique<
                    doc::Paragraph>(std::move(paragraph)));
            }
            return;
        }
    }

    void visitParamCommandComment(
        ParamCommandComment const* C)
    {
        doc::Param param;
        if(C->hasParamName())
            param.name = ensureUTF8(C->getParamNameAsWritten().str());
        else
            param.name = "@anon";
        if(C->isDirectionExplicit())
        {
            switch(C->getDirection())
            {
            case ParamCommandComment::PassDirection::In:
                param.direction = doc::ParamDirection::in;
                break;
            case ParamCommandComment::PassDirection::Out:
                param.direction = doc::ParamDirection::out;
                break;
            case ParamCommandComment::PassDirection::InOut:
                param.direction = doc::ParamDirection::inout;
                break;
            }
        }
        Scope scope(param, paragraph_);
        //if(C->hasNonWhitespaceParagraph())
        visitChildren(C->getParagraph());
        Javadoc::append(blocks_, std::make_unique<
            doc::Param>(std::move(param)));
    }

    void visitTParamCommandComment(
        TParamCommandComment const* C)
    {
        doc::TParam tparam;
        if(C->hasParamName())
            tparam.name = ensureUTF8(C->getParamNameAsWritten().str());
        else
            tparam.name = "@anon";
        Scope scope(tparam, paragraph_);
        //if(C->hasNonWhitespaceParagraph())
        visitChildren(C->getParagraph());
        Javadoc::append(blocks_, std::make_unique<
            doc::TParam>(std::move(tparam)));
    }

    void visitVerbatimBlockComment(
        VerbatimBlockComment const* C)
    {
        doc::Code code;
        Scope scope(code, paragraph_);
        //if(C->hasNonWhitespaceParagraph())
        visitChildren(C);
        Javadoc::append(blocks_, std::make_unique<
            doc::Code>(std::move(code)));
    }

    void visitVerbatimLineComment(
        VerbatimLineComment const* C)
    {
        // VFALCO This doesn't seem to be used
        //        in any of my codebases, follow up
    }

    void visitVerbatimBlockLineComment(
        VerbatimBlockLineComment const* C)
    {
        Javadoc::append(*paragraph_, std::make_unique<
            doc::Text>(C->getText().str()));
    }

private:
    FullComment const* FC_;
    ASTContext const& ctx_;
    doc::List<doc::Block> blocks_;
    doc::List<doc::Param> params_;
    doc::Paragraph* paragraph_ = nullptr;
};

//------------------------------------------------

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
    #include <clang/AST/CommentNodes.inc>
    #undef COMMENT

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

//------------------------------------------------

void
initCustomCommentCommands(ASTContext& context)
{
    auto& traits = context.getCommentCommandTraits();

    {
        //auto cmd = traits.registerBlockCommand("mrdox");
        //auto cmd = traits.registerBlockCommand("par");

        //CommentOptions opt;
        //opt.BlockCommandNames.push_back("par");
        //traits.registerCommentOptions(opt);
    }

    (void)traits;
}

Javadoc
parseJavadoc(
    RawComment const* RC,
    Decl const* D)
{
    return JavadocVisitor(RC, D).build();
}

} // mrdox
} // clang
