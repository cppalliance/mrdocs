//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ParseJavadoc.hpp"
#include <mrdocs/Metadata/Javadoc.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/String.hpp>
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

/*  AST Types

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

    BlockCommandComment
        Always has one child of type ParagraphComment child?

*/

namespace clang {
namespace mrdocs {

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
    Diagnostics& diags_;
    doc::List<doc::Param> params_;
    doc::Block* block_ = nullptr;
    std::size_t htmlTagNesting_ = 0;
    Comment::child_iterator it_;
    Comment::child_iterator end_;

    static std::string& ensureUTF8(std::string&&);
    void visitChildren(Comment const* C);


    class [[nodiscard]] BlockScope
    {
        JavadocVisitor& visitor_;
        doc::Block* prev_;

    public:
        BlockScope(
            JavadocVisitor& visitor,
            doc::Block* blk)
            : visitor_(visitor)
            , prev_(visitor.block_)
        {
            visitor_.block_ = blk;
        }

        ~BlockScope()
        {
            visitor_.block_ = prev_;
        }
    };

    BlockScope enterScope(doc::Block& blk)
    {
        return BlockScope(*this, &blk);
    }

public:
    JavadocVisitor(
        FullComment const*, Decl const*,
        Config const&, Diagnostics&);

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

    // helpers
    bool goodArgCount(std::size_t n, InlineCommandComment const& C);
};

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
    FullComment const* FC,
    Decl const* D,
    Config const& config,
    Diagnostics& diags)
    : config_(config)
    , ctx_(D->getASTContext())
    , sm_(ctx_.getSourceManager())
    , FC_(FC)
    , diags_(diags)
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

    // If this is the first text comment in the
    // paragraph then remove all the leading space.
    // Otherwise, just remove the trailing space.
    if(block_->children.empty())
        s = s.ltrim();
    else
        s = s.rtrim();

    // Only insert non-empty text nodes
    if(! s.empty())
        block_->emplace_back(doc::Text(ensureUTF8(s.str())));
}

void
JavadocVisitor::
visitHTMLStartTagComment(
    HTMLStartTagComment const* C)
{
    MRDOCS_ASSERT(C->child_begin() == C->child_end());
    auto const tag = C->getTagName();
    if(tag == "a")
    {
        PresumedLoc const loc = sm_.getPresumedLoc(C->getBeginLoc());

        if(end_ - it_ < 3)
        {
            // error
            report::error(
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
        block_->emplace_back(doc::Link(
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
    MRDOCS_ASSERT(C->child_begin() == C->child_end());
    --htmlTagNesting_;
}

static
doc::Parts
convertCopydoc(unsigned id)
{
    switch(id)
    {
    case CommandTraits::KCI_copydoc:
        return doc::Parts::all;
    case CommandTraits::KCI_copybrief:
        return doc::Parts::brief;
    case CommandTraits::KCI_copydetails:
        return doc::Parts::description;
    default:
        MRDOCS_UNREACHABLE();
    }
}

static
doc::Style
convertStyle(InlineCommandComment::RenderKind kind)
{
    switch(kind)
    {
    case InlineCommandComment::RenderKind::RenderMonospaced:
        return doc::Style::mono;
    case InlineCommandComment::RenderKind::RenderBold:
        return doc::Style::bold;
    case InlineCommandComment::RenderKind::RenderEmphasized:
        return doc::Style::italic;
    case InlineCommandComment::RenderKind::RenderNormal:
    case InlineCommandComment::RenderKind::RenderAnchor:
        return doc::Style::none;
    default:
        // unknown RenderKind
        MRDOCS_UNREACHABLE();
    }
}

static
doc::ParamDirection
convertDirection(ParamCommandComment::PassDirection kind)
{
    switch(kind)
    {
    case ParamCommandComment::PassDirection::In:
        return doc::ParamDirection::in;
    case ParamCommandComment::PassDirection::Out:
        return doc::ParamDirection::out;
    case ParamCommandComment::PassDirection::InOut:
        return doc::ParamDirection::inout;
    default:
        MRDOCS_UNREACHABLE();
    }
}

void
JavadocVisitor::
visitInlineCommandComment(
    InlineCommandComment const* C)
{
    auto const* cmd = ctx_
        .getCommentCommandTraits()
        .getCommandInfo(C->getCommandID());

    // VFALCO I'd like to know when this happens
    MRDOCS_ASSERT(cmd != nullptr);

    // KRYSTIAN FIXME: the text for a copydoc/ref command
    // should not include illegal characters
    // (e.g. periods that occur after the symbol name)
    switch(unsigned ID = cmd->getID())
    {
    // Emphasis
    case CommandTraits::KCI_a:
    case CommandTraits::KCI_e:
    case CommandTraits::KCI_em:
    {
        if(! goodArgCount(1, *C))
            return;
        auto style = doc::Style::italic;
        block_->emplace_back(doc::Styled(
            C->getArgText(0).str(), style));
        return;
    }

    // copy
    case CommandTraits::KCI_copybrief:
    case CommandTraits::KCI_copydetails:
    case CommandTraits::KCI_copydoc:
    {
        if(! goodArgCount(1, *C))
            return;
        // the referenced symbol will be resolved during
        // the finalization step once all symbol are extracted
        block_->emplace_back(doc::Copied(
            C->getArgText(0).str(), convertCopydoc(ID)));
        return;
    }
    case CommandTraits::KCI_ref:
    {
        if(! goodArgCount(1, *C))
            return;
        // the referenced symbol will be resolved during
        // the finalization step once all symbol are extracted
        block_->emplace_back(doc::Reference(
            C->getArgText(0).str()));
        return;
    }

    default:
        break;
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

    doc::Style style = convertStyle(C->getRenderKind());
    if(style != doc::Style::none)
        block_->emplace_back(doc::Styled(std::move(s), style));
    else
        block_->emplace_back(doc::Text(std::move(s)));
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
    if(block_)
        return visitChildren(C);
    doc::Paragraph paragraph;
    auto scope = enterScope(paragraph);
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

    switch(cmd->getID())
    {
    case CommandTraits::KCI_brief:
    case CommandTraits::KCI_short:
    {
        doc::Brief brief;
        auto scope = enterScope(brief);
        // Scope scope(brief, block_);
        visitChildren(C->getParagraph());
        // Here, we want empty briefs, because
        // the @brief command was explicitly given.
        jd_.emplace_back(std::move(brief));
        return;
    }

    case CommandTraits::KCI_return:
    case CommandTraits::KCI_returns:
    {
        doc::Returns returns;
        auto scope = enterScope(returns);
        // Scope scope(returns, block_);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(returns));
        return;
    }

    case CommandTraits::KCI_addindex:
    case CommandTraits::KCI_addtogroup:
    case CommandTraits::KCI_anchor:
    case CommandTraits::KCI_arg:
    case CommandTraits::KCI_attention:
    case CommandTraits::KCI_author:
    case CommandTraits::KCI_authors:
    case CommandTraits::KCI_b:
    case CommandTraits::KCI_bug:
    case CommandTraits::KCI_c:
    case CommandTraits::KCI_callergraph:
    case CommandTraits::KCI_callgraph:
    case CommandTraits::KCI_category:
    case CommandTraits::KCI_cite:
    case CommandTraits::KCI_class:
    case CommandTraits::KCI_code:
    case CommandTraits::KCI_concept:
    case CommandTraits::KCI_cond:
    case CommandTraits::KCI_copyright:
    case CommandTraits::KCI_date:
    case CommandTraits::KCI_def:
    case CommandTraits::KCI_defgroup:
    case CommandTraits::KCI_deprecated:
    case CommandTraits::KCI_details:
    case CommandTraits::KCI_diafile:
    case CommandTraits::KCI_dir:
    case CommandTraits::KCI_docbookinclude:
    case CommandTraits::KCI_docbookonly:
    case CommandTraits::KCI_dontinclude:
    case CommandTraits::KCI_dot:
    case CommandTraits::KCI_dotfile:
    //case CommandTraits::KCI_doxyconfig:
    case CommandTraits::KCI_else:
    case CommandTraits::KCI_elseif:
    case CommandTraits::KCI_emoji:
    case CommandTraits::KCI_endcode:
    case CommandTraits::KCI_endcond:
    case CommandTraits::KCI_enddocbookonly:
    case CommandTraits::KCI_enddot:
    case CommandTraits::KCI_endhtmlonly:
    case CommandTraits::KCI_endif:
    case CommandTraits::KCI_endinternal:
    case CommandTraits::KCI_endlatexonly:
    //case CommandTraits::KCI_endlink:
    case CommandTraits::KCI_endmanonly:
    case CommandTraits::KCI_endmsc:
    case CommandTraits::KCI_endparblock:
    case CommandTraits::KCI_endrtfonly:
    case CommandTraits::KCI_endsecreflist:
    case CommandTraits::KCI_endverbatim:
    case CommandTraits::KCI_enduml:
    case CommandTraits::KCI_endxmlonly:
    case CommandTraits::KCI_enum:
    case CommandTraits::KCI_example:
    case CommandTraits::KCI_exception:
    case CommandTraits::KCI_extends:
    case CommandTraits::KCI_flparen:  // @f(
    case CommandTraits::KCI_frparen:  // @f)
    case CommandTraits::KCI_fdollar:  // @f$
    case CommandTraits::KCI_flsquare: // @f[
    case CommandTraits::KCI_frsquare: // @f]
    case CommandTraits::KCI_flbrace:  // @f{
    case CommandTraits::KCI_frbrace:  // @f}
    case CommandTraits::KCI_file:
    //case CommandTraits::KCI_fileinfo:
    case CommandTraits::KCI_fn:
    case CommandTraits::KCI_headerfile:
    case CommandTraits::KCI_hidecallergraph:
    case CommandTraits::KCI_hidecallgraph:
    case CommandTraits::KCI_hiderefby:
    case CommandTraits::KCI_hiderefs:
    case CommandTraits::KCI_hideinitializer:
    case CommandTraits::KCI_htmlinclude:
    case CommandTraits::KCI_htmlonly:
    case CommandTraits::KCI_idlexcept:
    case CommandTraits::KCI_if:
    case CommandTraits::KCI_ifnot:
    case CommandTraits::KCI_image:
    case CommandTraits::KCI_implements:
    case CommandTraits::KCI_include:
    //case CommandTraits::KCI_includedoc:
    //case CommandTraits::KCI_includelineno:
    case CommandTraits::KCI_ingroup:
    case CommandTraits::KCI_internal:
    case CommandTraits::KCI_invariant:
    case CommandTraits::KCI_interface:
    case CommandTraits::KCI_latexinclude:
    case CommandTraits::KCI_latexonly:
    case CommandTraits::KCI_li:
    case CommandTraits::KCI_line:
    //case CommandTraits::KCI_lineinfo:
    case CommandTraits::KCI_link:
    case CommandTraits::KCI_mainpage:
    case CommandTraits::KCI_maninclude:
    case CommandTraits::KCI_manonly:
    case CommandTraits::KCI_memberof:
    case CommandTraits::KCI_msc:
    case CommandTraits::KCI_mscfile:
    case CommandTraits::KCI_n:
    case CommandTraits::KCI_name:
    case CommandTraits::KCI_namespace:
    case CommandTraits::KCI_noop:
    case CommandTraits::KCI_nosubgrouping:
    case CommandTraits::KCI_note:
    case CommandTraits::KCI_overload:
    case CommandTraits::KCI_p:
    //case CommandTraits::KCI_package:
    case CommandTraits::KCI_page:
    case CommandTraits::KCI_par:
    case CommandTraits::KCI_paragraph:
    case CommandTraits::KCI_param:
    case CommandTraits::KCI_parblock:
    case CommandTraits::KCI_post:
    case CommandTraits::KCI_pre:
    case CommandTraits::KCI_private:
    case CommandTraits::KCI_privatesection:
    case CommandTraits::KCI_property:
    case CommandTraits::KCI_protected:
    case CommandTraits::KCI_protectedsection:
    case CommandTraits::KCI_protocol:
    case CommandTraits::KCI_public:
    case CommandTraits::KCI_publicsection:
    case CommandTraits::KCI_pure:
    //case CommandTraits::KCI_qualifier:
    //case CommandTraits::KCI_raisewarning:
    case CommandTraits::KCI_ref:
    case CommandTraits::KCI_refitem:
    case CommandTraits::KCI_related:
    case CommandTraits::KCI_relates:
    case CommandTraits::KCI_relatedalso:
    case CommandTraits::KCI_relatesalso:
    case CommandTraits::KCI_remark:
    case CommandTraits::KCI_remarks:
    case CommandTraits::KCI_result:

    case CommandTraits::KCI_retval:
    case CommandTraits::KCI_rtfinclude:
    case CommandTraits::KCI_rtfonly:
    case CommandTraits::KCI_sa:
    case CommandTraits::KCI_secreflist:
    case CommandTraits::KCI_section:
    case CommandTraits::KCI_see:
    //case CommandTraits::KCI_showdate:
    case CommandTraits::KCI_showinitializer:
    case CommandTraits::KCI_showrefby:
    case CommandTraits::KCI_showrefs:
    case CommandTraits::KCI_since:
    case CommandTraits::KCI_skip:
    case CommandTraits::KCI_skipline:
    case CommandTraits::KCI_snippet:
    //case CommandTraits::KCI_snippetdoc:
    //case CommandTraits::KCI_snippetlineno:
    case CommandTraits::KCI_static:
    case CommandTraits::KCI_startuml:
    case CommandTraits::KCI_struct:
    case CommandTraits::KCI_subpage:
    case CommandTraits::KCI_subsection:
    case CommandTraits::KCI_subsubsection:
    case CommandTraits::KCI_tableofcontents:
    case CommandTraits::KCI_test:
    case CommandTraits::KCI_throw:
    case CommandTraits::KCI_throws:
    case CommandTraits::KCI_todo:
    case CommandTraits::KCI_tparam:
    case CommandTraits::KCI_typedef:
    case CommandTraits::KCI_union:
    case CommandTraits::KCI_until:
    case CommandTraits::KCI_var:
    case CommandTraits::KCI_verbatim:
    case CommandTraits::KCI_verbinclude:
    case CommandTraits::KCI_version:
    //case CommandTraits::KCI_vhdlflow:
    case CommandTraits::KCI_warning:
    case CommandTraits::KCI_weakgroup:
    case CommandTraits::KCI_xmlinclude:
    case CommandTraits::KCI_xmlonly:
    case CommandTraits::KCI_xrefitem:
    /*
        @$
        @@
        @\
        @&
        @~
        @<
        @=
        @>
        @#
        @%
        @"
        @.
        @::
        @|
        @--
        @---
    */
        break;

    // inline commands handled elsewhere
    case CommandTraits::KCI_a:
    case CommandTraits::KCI_e:
    case CommandTraits::KCI_em:
    case CommandTraits::KCI_copybrief:
    case CommandTraits::KCI_copydetails:
    case CommandTraits::KCI_copydoc:
        MRDOCS_UNREACHABLE();

    default:
        MRDOCS_UNREACHABLE();
    }
    if(cmd->IsReturnsCommand)
    {
    }
   if(cmd->getID() == CommandTraits::KCI_note)
    {
        doc::Admonition paragraph(doc::Admonish::note);
        auto scope = enterScope(paragraph);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(paragraph));
        return;
    }
    if(cmd->getID() == CommandTraits::KCI_warning)
    {
        doc::Admonition paragraph(doc::Admonish::warning);
        auto scope = enterScope(paragraph);
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
        auto scope = enterScope(paragraph);
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
        auto scope = enterScope(paragraph);
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
    {
        param.name = ensureUTF8(C->getParamNameAsWritten().str());
    }
    else
    {
        // VFALCO report SourceLocation here
        diags_.error("Missing parameter name in @param");
        param.name = "@anon";
    }
    if(C->isDirectionExplicit())
        param.direction = convertDirection(C->getDirection());

    auto scope = enterScope(param);
    visitChildren(C->getParagraph());
    // We want the node even if it is empty
    jd_.emplace_back(std::move(param));
}

void
JavadocVisitor::
visitTParamCommandComment(
    TParamCommandComment const* C)
{
    doc::TParam tparam;
    if(C->hasParamName())
    {
        tparam.name = ensureUTF8(C->getParamNameAsWritten().str());
    }
    else
    {
        // VFALCO report SourceLocation here
        diags_.error("Missing parameter name in @tparam");
        tparam.name = "@anon";
    }
    auto scope = enterScope(tparam);
    visitChildren(C->getParagraph());
    // We want the node even if it is empty
    jd_.emplace_back(std::move(tparam));
}

void
JavadocVisitor::
visitVerbatimBlockComment(
    VerbatimBlockComment const* C)
{
    doc::Code code;
    auto scope = enterScope(code);
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
    block_->emplace_back(doc::Text(C->getText().str()));
}

//------------------------------------------------

bool
JavadocVisitor::
goodArgCount(std::size_t n,
    InlineCommandComment const& C)
{
    if(C.getNumArgs() != n)
    {
        auto loc = sm_.getPresumedLoc(C.getBeginLoc());

        diags_.error(fmt::format(
            "Expected {} but got {} args\n"
            "File: {}, line {}, col {}\n",
            n, C.getNumArgs(),
            loc.getFilename(),
            loc.getLine(),
            loc.getColumn()));

        return false;
    }
    return true;
}

//------------------------------------------------

} // (anon)

void
initCustomCommentCommands(ASTContext& context)
{
    auto& traits = context.getCommentCommandTraits();

    {
        //auto cmd = traits.registerBlockCommand("mrdocs");
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
    FullComment* FC,
    Decl const* D,
    Config const& config,
    Diagnostics& diags)
{
    auto result = JavadocVisitor(FC, D, config, diags).build();
    if(jd == nullptr)
    {
        // Do not create javadocs which have no nodes
        if(! result.getBlocks().empty())
            jd = std::make_unique<Javadoc>(std::move(result));
    }
    else if(*jd != result)
    {
        // merge
        jd->append(std::move(result));
    }
}

} // mrdocs
} // clang
