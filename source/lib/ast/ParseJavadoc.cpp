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

#include "clangASTComment.hpp"
#include "ParseJavadoc.hpp"
#include <mrdox/meta/Javadoc.hpp>
#include <clang/AST/CommentCommandTraits.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RawCommentList.h>
#include <llvm/Support/JSON.h>
#include <cassert>

#include <llvm/Support/Mutex.h>

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

    template<class T, class R, class... Args>
    static void visit_children(
        ConstCommentVisitor<T, R, Args...>* visitor,
        Comment const* C)
    {
        for(auto const* it = C->child_begin();
                it != C->child_end(); ++it)
            visitor->visit(*it);
    }

public:
    JavadocVisitor(
        RawComment const* RC,
        ASTContext const& ctx,
        Decl const* D)
        : FC_(RC->parse(ctx, nullptr, D))
        , ctx_(ctx)
    {
    }

    Javadoc build()
    {
        visit(FC_);

        return Javadoc(
            std::move(blocks_),
            std::move(params_),
            std::move(tparams_),
            std::move(returns_));
    }

    void visitComment(
        Comment const* C)
    {
        visit_children(this, C);
    }

    void visitTextComment(
        TextComment const* C)
    {
        llvm::StringRef s;
        // If this is the very first node, the
        // paragraph has no doxygen command,
        // so we will trim the leading space.
        // Otherwise just trim trailing space
        if(para_->children.empty())
            s = C->getText().ltrim().rtrim();
        else
            s = C->getText().rtrim();
            //s = C->getText().ltrim().rtrim();

        // VFALCO Figure out why we get empty TextComment
        if(! s.empty())
            Javadoc::append(*para_,
                Javadoc::Text(ensureUTF8(s.str())));
    }

    void visitInlineCommandComment(
        InlineCommandComment const* C)
    {
        Javadoc::Style style(Javadoc::Style::none);
        switch (C->getRenderKind())
        {
        case InlineCommandComment::RenderKind::RenderMonospaced:
            style = Javadoc::Style::mono;
            break;
        case InlineCommandComment::RenderKind::RenderBold:
            style = Javadoc::Style::bold;
            break;
        case InlineCommandComment::RenderKind::RenderEmphasized:
            style = Javadoc::Style::italic;
            break;
        case InlineCommandComment::RenderKind::RenderNormal:
        case InlineCommandComment::RenderKind::RenderAnchor:
            break;
        default:
            llvm_unreachable("unknown RenderKind");
        }

        // It looks like the clang parser does not
        // emit nested styles, so only one inline
        // style command can be applied per args.

        Javadoc::String s;
        std::size_t n = 0;
        for(unsigned i = 0; i < C->getNumArgs(); ++i)
            n += C->getArgText(i).size();
        s.reserve(n);
        for(unsigned i = 0; i < C->getNumArgs(); ++i)
            s.append(C->getArgText(i));

        if(style != Javadoc::Style::none)
            Javadoc::append(*para_,
                Javadoc::StyledText(std::move(s), style));
        else
            Javadoc::append(*para_,
                Javadoc::Text(std::move(s)));
    }

    //
    // Block Content
    //

    void visitParagraphComment(
        ParagraphComment const* C)
    {
        if(para_)
            return visit_children(this, C);
        Javadoc::Paragraph para;
        Scope scope(para, para_);
        visit_children(this, C);
        // VFALCO Figure out why we get empty ParagraphComment
        if(! para.empty())
            Javadoc::append(blocks_, std::move(para));
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
            Javadoc::Brief brief;
            Scope scope(brief, para_);
            visit_children(this, C->getParagraph());
            Javadoc::append(blocks_, std::move(brief));
            return;
        }
        if(cmd->IsReturnsCommand)
        {
            Scope scope(returns_, para_);
            visit_children(this, C->getParagraph());
            return;
        }
        if(cmd->getID() == CommandTraits::KCI_note)
        {
            Javadoc::Admonition para(Javadoc::Admonish::note);
            Scope scope(para, para_);
            visit_children(this, C->getParagraph());
            Javadoc::append(blocks_, std::move(para));
            return;
        }
        if(cmd->getID() == CommandTraits::KCI_warning)
        {
            Javadoc::Admonition para(Javadoc::Admonish::warning);
            Scope scope(para, para_);
            visit_children(this, C->getParagraph());
            Javadoc::append(blocks_, std::move(para));
            return;
        }
    }

    void visitParamCommandComment(
        ParamCommandComment const* C)
    {
        Javadoc::String name;
        Javadoc::Paragraph para;
        if(C->hasParamName())
            name = ensureUTF8(C->getParamNameAsWritten().str());
        else
            name = "@anon";
        Scope scope(para, para_);
        //if(C->hasNonWhitespaceParagraph())
        visit_children(this, C->getParagraph());
        params_.emplace_back(Javadoc::Param(
            std::move(name), std::move(para)));
    }

    void visitTParamCommandComment(
        TParamCommandComment const* C)
    {
        Javadoc::String name;
        Javadoc::Paragraph para;
        if(C->hasParamName())
            name = ensureUTF8(C->getParamNameAsWritten().str());
        else
            name = "@anon";
        Scope scope(para, para_);
        //if(C->hasNonWhitespaceParagraph())
        visit_children(this, C->getParagraph());
        tparams_.emplace_back(Javadoc::TParam(
            std::move(name), std::move(para)));
    }

    void visitVerbatimBlockComment(
        VerbatimBlockComment const* C)
    {
        Javadoc::Code code;
        Scope scope(code, code_);
        //if(C->hasNonWhitespaceParagraph())
        visit_children(this, C);
        Javadoc::append(blocks_, std::move(code));
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
        Javadoc::append(*code_,
            Javadoc::Text(C->getText().str()));
    }

private:
    FullComment const* FC_;
    ASTContext const& ctx_;
    List<Javadoc::Block> blocks_;
    List<Javadoc::Param> params_;
    List<Javadoc::TParam> tparams_;
    Javadoc::Returns returns_;
    Javadoc::Paragraph* para_ = nullptr;
    Javadoc::Code* code_ = nullptr;
    llvm::raw_string_ostream* os_ = nullptr;
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

template<class T>
void dump(
    llvm::raw_ostream& os,
    List<T> const& list)
{
    for(auto const& t : list)
        dumpNode(os, t);
}

void
dump(
    llvm::raw_ostream& os,
    Javadoc::Text const& t)
{
    os << t.string << '\n';
}

void
dump(
    llvm::raw_ostream& os,
    Javadoc::StyledText const& t)
{
    char c = ' ';
    switch(t.style)
    {
    case Javadoc::Style::mono: c = '`'; break;
    case Javadoc::Style::bold: c = '*'; break;
    case Javadoc::Style::italic: c = '_'; break;
    default:
        llvm_unreachable("unknown Style");
    }
    os << c << t.string << c;
}

void
dump(
    llvm::raw_ostream& os,
    Javadoc::Paragraph const& t)
{
    dump(os, t.children);
}

void
dump(
    llvm::raw_ostream& os,
    Javadoc::Admonition const& t)
{
    switch(t.style)
    {
    case Javadoc::Admonish::note:
        os << "    @note\n";
        break;
    case Javadoc::Admonish::warning:
        os << "    @warning\n";
        break;
    default:
        llvm_unreachable("unknown style");
    }
    dump(os, t.children);
}

void
dump(
    llvm::raw_ostream& os,
    Javadoc::Param const& param)
{
    os <<
        "    @param " <<
        param.name << ' ';
    dump(os, param.children);
    os << '\n';
}

void
dump(
    llvm::raw_ostream& os,
    Javadoc::TParam const& tparam)
{
    os <<
        "    @tparam " <<
        tparam.name << ' ';
    dump(os, tparam.children);
    os << '\n';
}

void
dump(
    llvm::raw_ostream& os,
    Javadoc::Code const& code)
{
    os <<
        "    @code\n";
    dump(os, code.children);
    os <<
        "    @endcode\n";
}

void dumpNode(
    llvm::raw_ostream& os,
    Javadoc::Node const& node)
{
    switch(node.kind)
    {
    case Javadoc::Kind::text:
        dump(os, static_cast<Javadoc::Text const&>(node));
        break;
    case Javadoc::Kind::styled:
        dump(os, static_cast<Javadoc::StyledText const&>(node));
        break;
    case Javadoc::Kind::paragraph:
        dump(os, static_cast<Javadoc::Paragraph const&>(node));
        break;
    case Javadoc::Kind::admonition:
        dump(os, static_cast<Javadoc::Admonition const&>(node));
        break;
    case Javadoc::Kind::param:
        dump(os, static_cast<Javadoc::Param const&>(node));
        break;
    case Javadoc::Kind::tparam:
        dump(os, static_cast<Javadoc::TParam const&>(node));
        break;
    case Javadoc::Kind::code:
        dump(os, static_cast<Javadoc::Code const&>(node));
        break;
    default:
        llvm_unreachable("unknown Kind");
    }
}

void
dumpJavadoc(
    Javadoc const& jd)
{
    auto& os = llvm::outs();
    static llvm::sys::Mutex m;
    std::lock_guard<llvm::sys::Mutex> lock(m);

    os << "/** ";
    if(jd.getBrief())
        dump(os, *jd.getBrief());
    os << '\n';

    if(! jd.getBlocks().empty())
        dump(os, jd.getBlocks());

    if(! jd.getReturns().empty())
    {
        os << "    @returns ";
        dump(os, jd.getReturns());
    }

    if(! jd.getParams().empty())
    {
        os << '\n';
        dump(os, jd.getParams());
    }

    if(! jd.getTParams().empty())
    {
        os << '\n';
        dump(os, jd.getTParams());
    }

    os <<
        "*/\n"
        "\n\n";
}

Javadoc
parseJavadoc(
    RawComment const* RC,
    ASTContext const& Ctx,
    Decl const* D)
{
    return JavadocVisitor(RC, Ctx, D).build();
}

} // mrdox
} // clang
