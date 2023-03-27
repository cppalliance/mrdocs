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

#include "CommentVisitor.h "
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

using namespace clang::comments;

class CommentVisitor
    : public ConstCommentVisitor<CommentVisitor, bool>
{
public:
    explicit
    CommentVisitor(
        comments::FullComment const& fc,
        clang::ASTContext& ctx,
        Javadoc& javadoc)
        : ctx_(ctx)
        , jd_(javadoc)
    {
    }

    void parse(comments::Comment const* c, CommentInfo& ci);
    bool parseChildren(comments::Comment const* c);
    bool visitFullComment(FullComment const* c);
    bool visitParagraphComment(ParagraphComment const* c);
    bool visitTextComment(TextComment const* c);
    bool visitInlineCommandComment(InlineCommandComment const* c);
    bool visitHTMLStartTagComment(HTMLStartTagComment const* c);
    bool visitHTMLEndTagComment(HTMLEndTagComment const* c);
    bool visitBlockCommandComment(BlockCommandComment const* c);
    bool visitParamCommandComment(ParamCommandComment const* c);
    bool visitTParamCommandComment(TParamCommandComment const* c);
    bool visitVerbatimBlockComment(VerbatimBlockComment const* c);
    bool visitVerbatimBlockLineComment(VerbatimBlockLineComment const* c);
    bool visitVerbatimLineComment(VerbatimLineComment const* c);

private:
    std::string getCommandName(unsigned CommandID) const;
    bool isWhitespaceOnly(StringRef S) const;

    clang::ASTContext& ctx_;
    Javadoc& jd_;
    CommentInfo* ci_ = nullptr;
    std::string verbatim_;
    bool in_brief_ = false;
    bool brief_done_ = false;
};

void
CommentVisitor::
parse(
    comments::Comment const* c,
    CommentInfo& ci)
{
    auto const prev = ci_;
    ci_ = &ci;
    ci_->Kind = c->getCommentKindName();

    // Allow the visit function to handle recursing
    auto const parsedChildren = this->ConstCommentVisitor<
        CommentVisitor, bool>::visit(c);
    if(! parsedChildren)
        parseChildren(c);
    ci_ = prev;
}

bool
CommentVisitor::
parseChildren(
    comments::Comment const* c)
{
    for (comments::Comment* Child :
        llvm::make_range(c->child_begin(), c->child_end()))
    {
        ci_->Children.emplace_back(std::make_unique<CommentInfo>());
        parse(Child, *ci_->Children.back());
    }
    return true;
}

bool
CommentVisitor::
visitFullComment(
    FullComment const* c)
{
    return parseChildren(c);
}

bool
CommentVisitor::
visitParagraphComment(
    ParagraphComment const* c)
{
    if(! brief_done_)
    {
        if(! in_brief_)
        {
            // First ParagraphComment starts the brief.
            in_brief_ = true;
        }
        else
        {
            // Subsequent ParagraphComment ends the brief.
            in_brief_ = false;
            brief_done_ = true;
        }
    }
    return false;
}

bool
CommentVisitor::
visitTextComment(
    TextComment const* c)
{
    llvm::StringRef s = c->getText();
    if(! isWhitespaceOnly(s))
        ci_->Text = s;
    // VFALCO do we want brief to have the whitespace?
    if(in_brief_)
    {
        if(jd_.brief.empty())
            s = s.ltrim(); // "\n\v\f\r"
        jd_.brief += s;
    }
    else
    {
        if(jd_.desc.empty())
            s = s.ltrim(); // "\n\v\f\r"
        jd_.desc += s;
    }
    return false;
}

bool
CommentVisitor::
visitInlineCommandComment(
    InlineCommandComment const* c)
{
    ci_->Name = getCommandName(c->getCommandID());
    for (unsigned I = 0, E = c->getNumArgs(); I != E; ++I)
        ci_->Args.push_back(c->getArgText(I));
    return false;
}

bool
CommentVisitor::
visitHTMLStartTagComment(
    HTMLStartTagComment const* c)
{
    ci_->Name = c->getTagName();
    ci_->SelfClosing = c->isSelfClosing();
    for (unsigned I = 0, E = c->getNumAttrs(); I < E; ++I)
    {
        HTMLStartTagComment::Attribute const& Attr = c->getAttr(I);
        ci_->AttrKeys.push_back(Attr.Name);
        ci_->AttrValues.push_back(Attr.Value);
    }
    return false;
}

bool
CommentVisitor::
visitHTMLEndTagComment(
    HTMLEndTagComment const* c)
{
    ci_->Name = c->getTagName();
    ci_->SelfClosing = true;
    return false;
}

bool
CommentVisitor::
visitBlockCommandComment(
    BlockCommandComment const* c)
{
    switch(c->getCommandID())
    {
    case CommandTraits::KnownCommandIDs::KCI_code:
        break;
    case CommandTraits::KnownCommandIDs::KCI_endcode:
        break;
    default:
        break;
    }
    ci_->Name = getCommandName(c->getCommandID());
    for (unsigned I = 0, E = c->getNumArgs(); I < E; ++I)
        ci_->Args.push_back(c->getArgText(I));
    return false;
}

bool
CommentVisitor::
visitParamCommandComment(
    ParamCommandComment const* c)
{
    ci_->Direction =
        ParamCommandComment::getDirectionAsString(c->getDirection());
    ci_->Explicit = c->isDirectionExplicit();
    if (c->hasParamName())
        ci_->ParamName = c->getParamNameAsWritten();
    return false;
}

bool
CommentVisitor::
visitTParamCommandComment(
    TParamCommandComment const* c)
{
    if (c->hasParamName())
        ci_->ParamName = c->getParamNameAsWritten();
    return false;
}

bool
CommentVisitor::
visitVerbatimBlockComment(
    VerbatimBlockComment const* c)
{
    if( c->getCommandID() ==
        CommandTraits::KnownCommandIDs::KCI_code)
    {
        if(c->getCloseName() != "endcode")
        {
            // VFALCO Report an error on the file and line
            clang::SourceLocation loc = c->getEndLoc();
            llvm::outs() <<
                "Error: wrong closing tag '" << c->getCloseName() << "'\n";
            // VFALCO this is broken
            if(loc.isValid())
                loc.print(llvm::outs(), ctx_.getSourceManager());
        }
        //jd_.verbatim.push_back(mrdox::VerbatimBlock{});

        verbatim_ = "";
        parseChildren(c);
        llvm::raw_string_ostream os(jd_.desc);
        os <<
            "\n" <<
            "[,cpp]\n" <<
            "----\n" <<
            verbatim_ <<
            "----\n\n";
        verbatim_ = "";
        return true;
    }

    ci_->Name = getCommandName(c->getCommandID());
    ci_->CloseName = c->getCloseName();
    return false;
}

bool
CommentVisitor::
visitVerbatimBlockLineComment(
    VerbatimBlockLineComment const* c)
{
    llvm::StringRef s = c->getText();
    //if (!isWhitespaceOnly(c->getText()))
        //ci_->Text = c->getText();
    verbatim_.append(s);
    verbatim_.push_back('\n');
    return false;
}

bool
CommentVisitor::
visitVerbatimLineComment(
    VerbatimLineComment const* c)
{
    if (!isWhitespaceOnly(c->getText()))
        ci_->Text = c->getText();
    return false;
}

bool
CommentVisitor::
isWhitespaceOnly(
    llvm::StringRef s) const
{
    return llvm::all_of(s, isspace);
}

std::string
CommentVisitor::
getCommandName(
    unsigned CommandID) const
{
    CommandInfo const* Info =
        CommandTraits::getBuiltinCommandInfo(CommandID);
    if (Info)
        return Info->Name;
    // TODO: Add parsing for \file command.
    return "<not a builtin command>";
}

} // namespace serialize

//------------------------------------------------

void
parseComment(
    comments::FullComment const* c,
    ASTContext& ctx,
    Javadoc& javadoc,
    CommentInfo& ci)
{
    clang::doc::serialize::CommentVisitor v(*c, ctx, javadoc);
    v.parse(c, ci);

    // debugging
    /*
    llvm::outs() <<
        "javadoc.brief = '" << javadoc.brief << "'\n" <<
        "javadoc.desc  = '" << javadoc.desc << "'\n" <<
        "\n";
    */
}

} // namespace doc
} // namespace clang
