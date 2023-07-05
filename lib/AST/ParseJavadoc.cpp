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
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/Path.hpp>
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
#include <clang/Basic/SourceManager.h>
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

class JavadocVisitor
    : public ConstCommentVisitor<JavadocVisitor>
{
    Config const& config_;
    ASTContext const& ctx_;
    SourceManager const& sm_;
    FullComment const* FC_;
    Javadoc jd_;
    doc::List<doc::Param> params_;
    doc::Paragraph* paragraph_ = nullptr;
    std::size_t htmlTagNesting_ = 0;
    Comment::child_iterator it_;
    Comment::child_iterator end_;

    static std::string& ensureUTF8(std::string&&);
    void visitChildren(Comment const* C);

public:
    JavadocVisitor(
        RawComment const*, Decl const*, Config const&);
    Javadoc build();

    void visitComment(Comment const* C);

    // inline content
    void visitTextComment(TextComment const* C);
    void visitHTMLStartTagComment(HTMLStartTagComment const* C);
    void visitHTMLEndTagComment(HTMLEndTagComment const* C);
    void visitInlineCommandComment(InlineCommandComment const* C);

    // block content
    void visitParagraphComment(ParagraphComment const* C);
    void visitBlockCommandComment(BlockCommandComment const* C);
    void visitParamCommandComment(ParamCommandComment const* C);
    void visitTParamCommandComment(TParamCommandComment const* C);
    void visitVerbatimBlockComment(VerbatimBlockComment const* C);
    void visitVerbatimLineComment(VerbatimLineComment const* C);
    void visitVerbatimBlockLineComment(VerbatimBlockLineComment const* C);
};

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

std::string&
JavadocVisitor::
ensureUTF8(
    std::string&& s)
{
    if (!llvm::json::isUTF8(s))
        s = llvm::json::fixUTF8(s);
    return s;
}

void
JavadocVisitor::
visitChildren(
    Comment const* C)
{
    auto const it0 = it_;
    auto const end0 = end_;
    it_ = C->child_begin();
    end_ = C->child_end();
    while(it_ != end_)
    {
        visit(*it_);
        ++it_; // must happen after
    }
    it_ = it0;
    end_ = end0;
}

//------------------------------------------------

JavadocVisitor::
JavadocVisitor(
    RawComment const* RC,
    Decl const* D,
    Config const& config)
    : config_(config)
    , ctx_(D->getASTContext())
    , sm_(ctx_.getSourceManager())
    , FC_(RC->parse(D->getASTContext(), nullptr, D))
{
}

Javadoc
JavadocVisitor::
build()
{
    visit(FC_);
    return std::move(jd_);
}

void
JavadocVisitor::
visitComment(
    Comment const* C)
{
    visitChildren(C);
}

//------------------------------------------------
//
// inline content
//
//------------------------------------------------

void
JavadocVisitor::
visitTextComment(
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
        paragraph_->emplace_back(doc::Text(ensureUTF8(s.str())));
}

void
JavadocVisitor::
visitHTMLStartTagComment(
    HTMLStartTagComment const* C)
{
    MRDOX_ASSERT(C->child_begin() == C->child_end());
    auto const tag = C->getTagName();
    if(tag == "a")
    {
        PresumedLoc const loc = sm_.getPresumedLoc(C->getBeginLoc());

        if(end_ - it_ < 3)
        {
            // error
            reportError(
                "warning: invalid HTML <a> tag at {}({})",
                files::makePosixStyle(loc.getFilename()),
                loc.getLine());
            return;
        }
        if(it_[1]->getCommentKind() != Comment::TextCommentKind)
        {
            // error
            return;
        }
        if(it_[2]->getCommentKind() != Comment::HTMLEndTagCommentKind)
        {
            // error
            return;
        }
        auto const& cText =
            *static_cast<TextComment const*>(it_[1]);
        auto const& cEndTag =
            *static_cast<HTMLEndTagComment const*>(it_[2]);
        if(cEndTag.getTagName() != "a")
        {
            // error
            return;
        }
        std::string text;
        std::string href;
        text = cText.getText();
        for(std::size_t i = 0; i < C->getNumAttrs(); ++i)
        {
            auto const& attr = C->getAttr(i);
            if(attr.Name == "href")
            {
                href = attr.Value;
                break;
            }
        }
        paragraph_->emplace_back(doc::Link(
            ensureUTF8(std::move(text)),
            ensureUTF8(std::move(href))));

        it_ += 2; // bit of a hack
    }
    --htmlTagNesting_;
}

void
JavadocVisitor::
visitHTMLEndTagComment(
    HTMLEndTagComment const* C)
{
    MRDOX_ASSERT(C->child_begin() == C->child_end());
    --htmlTagNesting_;
}

void
JavadocVisitor::
visitInlineCommandComment(
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
        paragraph_->emplace_back(doc::Styled(std::move(s), style));
    else
        paragraph_->emplace_back(doc::Text(std::move(s)));
}

//------------------------------------------------
//
// block Content
//
//------------------------------------------------

void
JavadocVisitor::
visitParagraphComment(
    ParagraphComment const* C)
{
    if(paragraph_)
        return visitChildren(C);
    doc::Paragraph paragraph;
    Scope scope(paragraph, paragraph_);
    visitChildren(C);
    // VFALCO Figure out why we get empty ParagraphComment
    if(! paragraph.empty())
        jd_.emplace_back(std::move(paragraph));
}

void
JavadocVisitor::
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
        jd_.emplace_back(std::move(brief));
        return;
    }
    if(cmd->IsReturnsCommand)
    {
        doc::Returns returns;
        Scope scope(returns, paragraph_);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(returns));
        return;
    }
    if(cmd->getID() == CommandTraits::KCI_note)
    {
        doc::Admonition paragraph(doc::Admonish::note);
        Scope scope(paragraph, paragraph_);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(paragraph));
        return;
    }
    if(cmd->getID() == CommandTraits::KCI_warning)
    {
        doc::Admonition paragraph(doc::Admonish::warning);
        Scope scope(paragraph, paragraph_);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(paragraph));
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
            jd_.emplace_back(std::move(heading));

            // remaining TextComment, if any
            paragraph.children.erase(paragraph.children.begin());
            if(! paragraph.children.empty())
                jd_.emplace_back(std::move(paragraph));
        }
        return;
    }
    if(cmd->getID() == CommandTraits::KCI_li)
    {
        doc::ListItem paragraph;
        Scope scope(paragraph, paragraph_);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(paragraph));
        return;
    }
}

void
JavadocVisitor::
visitParamCommandComment(
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
    jd_.emplace_back(std::move(param));
}

void
JavadocVisitor::
visitTParamCommandComment(
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
    jd_.emplace_back(std::move(tparam));
}

void
JavadocVisitor::
visitVerbatimBlockComment(
    VerbatimBlockComment const* C)
{
    doc::Code code;
    Scope scope(code, paragraph_);
    //if(C->hasNonWhitespaceParagraph())
    visitChildren(C);
    jd_.emplace_back(std::move(code));
}

void
JavadocVisitor::
visitVerbatimLineComment(
    VerbatimLineComment const* C)
{
    // VFALCO This doesn't seem to be used
    //        in any of my codebases, follow up
}

void
JavadocVisitor::
visitVerbatimBlockLineComment(
    VerbatimBlockLineComment const* C)
{
    paragraph_->emplace_back(doc::Text(C->getText().str()));
}

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

void
parseJavadoc(
    std::unique_ptr<Javadoc>& jd,
    RawComment* RC,
    Decl const* D,
    Config const& config)
{
    if(RC)
    {
        RC->setAttached();

        auto result = JavadocVisitor(RC, D, config).build();

        if(jd == nullptr)
        {
            jd = std::make_unique<Javadoc>(std::move(result));
        }
        else
        {
            // merge
            MRDOX_UNREACHABLE();
        }
    }
    else
    {
        MRDOX_ASSERT(jd == nullptr);
    }
}

} // mrdox
} // clang
