//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ParseJavadoc.hpp"
#include <mrdocs/Metadata/Javadoc.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/String.hpp>
#include <mrdocs/Support/ScopeExit.hpp>
#include <clang/AST/CommentCommandTraits.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RawCommentList.h>
#include <clang/Lex/Lexer.h>
#include <clang/Basic/SourceLocation.h>
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
#include <ranges>

#ifdef NDEBUG
#define MRDOCS_COMMENT_TRACE(D, C)
#else

#    define MRDOCS_COMMENT_TRACE_MERGE_(a, b) a## b
#    define MRDOCS_COMMENT_TRACE_LABEL_(a)    MRDOCS_COMMENT_TRACE_MERGE_(comment_content_, a)
#    define MRDOCS_COMMENT_TRACE_UNIQUE_NAME  MRDOCS_COMMENT_TRACE_LABEL_(__LINE__)

namespace detail {
    template <class T>
    void
    dumpCommentContent(T const* C, clang::ASTContext const& Ctx, llvm::SmallString<1024>& contents)
    {
        if (!C)
        {
            return;
        }
        llvm::raw_svector_ostream os(contents);
        if constexpr (std::derived_from<T, clang::comments::Comment>)
        {
            auto const* CC = static_cast<clang::comments::Comment const*>(C);
            clang::SourceRange const R = CC->getSourceRange();
            clang::SourceManager const& SM = Ctx.getSourceManager();
            contents = clang::Lexer::getSourceText(
                clang::CharSourceRange::getTokenRange(R),
                SM,
                Ctx.getLangOpts());
        }
    }

    template <class T>
    requires (!std::is_pointer_v<T>)
    void
    dumpCommentContent(T const& C, clang::ASTContext const& Ctx, llvm::SmallString<1024>& contents)
    {
        dumpCommentContent(&C, Ctx, contents);
    }
} // namespace detail

#define MRDOCS_COMMENT_TRACE(D, C) \
    SmallString<1024> MRDOCS_COMMENT_TRACE_UNIQUE_NAME;                 \
    ::detail::dumpCommentContent(D, C, MRDOCS_COMMENT_TRACE_UNIQUE_NAME);        \
    report::debug("{}", std::string_view(MRDOCS_COMMENT_TRACE_UNIQUE_NAME.str()))
#endif


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

/** A visitor for extracting javadoc from comments.

    This class is a visitor for extracting javadoc from
    comments in `clang::comments::FullComment` objects.

  */
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
    doc::Text* last_child_ = nullptr;
    std::size_t htmlTagNesting_ = 0;
    Comment::child_iterator it_{};
    Comment::child_iterator end_{};

    /** Ensure that a string is valid UTF-8.

        This function checks if the specified string is valid
        UTF-8, and if not it fixes it and returns the result.

     */
    static std::string& ensureUTF8(std::string&&);

    /** Visit the children of a comment.

        This is a helper function that sets the current
        iterator to the beginning of the children of the
        specified comment, and then restores the original
        iterator when the function returns.

        The children of the comment are visited in order
        and appended to the current block.

     */
    void visitChildren(Comment const* C);


    /** Temporarily set a block as the current block.

        This sets the current block of the visitor to the
        specified block, and restores the previous block
        when the scope is exited.

        The current block is where the visitor will
        append new children when visiting comments.

     */
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
            visitor_.last_child_ = nullptr;
        }

        ~BlockScope()
        {
            visitor_.block_ = prev_;
            visitor_.last_child_ = nullptr;
        }
    };

    /** Enter a block scope.

        This function creates a `BlockScope` object associated
        to the visitor to temporarily set the current block to
        the specified block.

        New children will be appended to the specified block
        until the `BlockScope` object goes out of scope.

        @param blk The block to set as the current block
        @returns A `BlockScope` object that will restore
                 the previous block when it goes out of scope

     */
    BlockScope enterScope(doc::Block& blk)
    {
        return {*this, &blk};
    }

    struct TagComponents
    {
        std::string tag;
        std::string text;
        std::size_t n_siblings{0};
    };

public:
    /** Constructor

        Construct a JavadocVisitor to visit the specified
        comment and extract javadoc from it.

        @param FC The full comment to visit
        @param D The declaration to which the comment is attached
        @param config The MrDocs configuration
        @param diags The diagnostics sink

     */
    JavadocVisitor(
        FullComment const*, Decl const*,
        Config const&, Diagnostics&);

    /** Extract the javadoc from the comment.

        The comment specified in the constructor is visited
        and the javadoc is extracted. The result is returned
        by the function.

     */
    Javadoc build();

    /** Visit any abstract comment.

        This is the base case for all comments.
        It simply attempts to visit the children
        of the comment.

     */
    void
    visitComment(Comment const* C);

    /** Visit a plain text comment.
     */
    void
    visitTextComment(TextComment const* C);

    /** Visit an opening HTML tag with attributes.
     */
    void
    visitHTMLStartTagComment(HTMLStartTagComment const* C);

    /** Visit a closing HTML tag.
     */
    void
    visitHTMLEndTagComment(HTMLEndTagComment const* C);

    /** Visit a command with word-like arguments that is considered inline content.

        This is a command with word-like arguments that is considered
        inline content. The number of arguments depends on the command
        name.

        For instance, this could be something like "@ref foo",
        where "foo" is the argument.

     */
    void
    visitInlineCommandComment(InlineCommandComment const* C);

    /** Visit a single paragraph that contains inline content.
     */
    void
    visitParagraphComment(ParagraphComment const* C);

    /** Visit a command that has zero or more word-like arguments

       The number of word-like arguments depends on command name and
       a paragraph as an argument (e. g., @brief).

     */
    void
    visitBlockCommandComment(BlockCommandComment const* C);

    /** Visit the doxygen @param command.
     */
    void
    visitParamCommandComment(ParamCommandComment const* C);


    /** Visit the doxygen @tparam command

        The @tparam describes a template parameter.
    */
    void
    visitTParamCommandComment(TParamCommandComment const* C);

    /** Visit a verbatim block command (e. g., preformatted code).

        Verbatim block has an opening and a closing command and contains
        multiple lines of text (VerbatimBlockLineComment nodes).
     */
    void
    visitVerbatimBlockComment(VerbatimBlockComment const* C);

    /** Visit a verbatim line command.

        Verbatim line has an opening command, a single line of text
        (up to the newline after the opening command) and has no
        closing command.
     */
    void
    visitVerbatimLineComment(VerbatimLineComment const* C);

    /** Visit a line of text contained in a verbatim block.
     */
    void
    visitVerbatimBlockLineComment(VerbatimBlockLineComment const* C);

    /** @name Helpers
     * @{
     */

    /** Check if the number of arguments is correct for a command.

        This function checks if the number of arguments
        in the specified command is correct, and if not
        it emits a diagnostic and returns false.

        @returns `true` if the number of arguments is correct
     */
    bool
    goodArgCount(std::size_t n, InlineCommandComment const& C);


    Expected<TagComponents>
    parseHTMLTag(HTMLStartTagComment const* C);

    /** Append a text node to the current block.

        This function appends a text node to the current
        block.

        If the previous child appended was of the same type,
        the content is merged instead of creating a new node.

        If the text ends with a newline, it sets the last
        child to null to indicate the next element cannot be
        merged.

        @param end_with_nl `true` if the text ends with a newline
        @param args The arguments to construct the text node
     */
    template<std::derived_from<doc::Text> TextTy, typename... Args>
    void
    emplaceText(bool end_with_nl, Args&&... args)
    {
        TextTy elem(std::forward<Args>(args)...);
        bool can_merge = false;

        if(last_child_ && last_child_->kind == elem.kind)
        {
            if constexpr(TextTy::static_kind == doc::Kind::text)
                can_merge = true;

            if constexpr(TextTy::static_kind == doc::Kind::styled)
                can_merge = static_cast<doc::Styled*>(
                    last_child_)->style == elem.style;
        }

        if(! can_merge)
        {
            auto new_text = std::make_unique<TextTy>(std::move(elem));
            last_child_ = new_text.get();
            block_->children.emplace_back(std::move(new_text));

        }
        else
        {
            last_child_->string.append(elem.string);
        }

        if(end_with_nl)
            last_child_ = nullptr;
    }

    /** @} */
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

/*  Parse the inline content of a text

    This function takes a string from a comment
    and parses it into a sequence of styled text
    nodes.

    The string may contain inline commands that
    change the style of the text:

    Regular text is stored as a doc::Text.
    Styled text is stored as a doc::Styled.

    The styles can be one of: mono, bold, or italic.

    The tags "`", "*", and "_" are used to indicate
    the start and end of styled text. They can be
    escaped by prefixing them with a backslash.

 */
doc::List<doc::Text>
parseStyled(StringRef s)
{
    doc::List<doc::Text> result;
    std::string currentText;
    doc::Style currentStyle = doc::Style::none;
    bool escapeNext = false;

    auto isStyleMarker = [](char c) {
        return c == '`' || c == '*' || c == '_';
    };

    auto flushCurrentText = [&]() {
        if (!currentText.empty()) {
            if (currentStyle == doc::Style::none) {
                bool const lastIsSame =
                    !result.empty() &&
                    result.back()->kind == doc::Kind::text;
                if (lastIsSame)
                {
                    auto& lastText = static_cast<doc::Text&>(*result.back());
                    lastText.string.append(currentText);
                }
                else
                {
                    result.emplace_back(std::make_unique<doc::Text>(std::move(currentText)));
                }
            } else {
                bool const lastIsSame =
                    !result.empty() &&
                    result.back()->kind == doc::Kind::styled &&
                    static_cast<doc::Styled&>(*result.back()).style == currentStyle;
                if (lastIsSame)
                {
                    auto& lastStyled = static_cast<doc::Styled&>(*result.back());
                    lastStyled.string.append(currentText);
                }
                else
                {
                    result.emplace_back(std::make_unique<doc::Styled>(std::move(currentText), currentStyle));
                }
            }
            currentText.clear();
        }
    };

    auto isPunctuationOrSpace = [](char c) {
        return std::isspace(c) || std::ispunct(c);
    };

    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (escapeNext) {
            currentText.push_back(c);
            escapeNext = false;
        } else if (c == '\\') {
            escapeNext = true;
        } else if (isStyleMarker(c)) {
            bool const atWordBoundary =
                (currentStyle == doc::Style::none && ((i == 0) || isPunctuationOrSpace(s[i - 1]))) ||
                (currentStyle != doc::Style::none && ((i == s.size() - 1) || isPunctuationOrSpace(s[i + 1])));
            if (atWordBoundary) {
                flushCurrentText();
                if (c == '`') {
                    currentStyle = (currentStyle == doc::Style::mono) ? doc::Style::none : doc::Style::mono;
                } else if (c == '*') {
                    currentStyle = (currentStyle == doc::Style::bold) ? doc::Style::none : doc::Style::bold;
                } else if (c == '_') {
                    currentStyle = (currentStyle == doc::Style::italic) ? doc::Style::none : doc::Style::italic;
                }
            } else {
                currentText.push_back(c);
            }
        } else {
            currentText.push_back(c);
        }
    }

    // Whatever style we started, we should end it because
    // we reached the end of the string without a closing
    // marker.
    currentStyle = doc::Style::none;
    flushCurrentText();

    return result;
}

void
JavadocVisitor::
visitChildren(
    Comment const* C)
{
    MRDOCS_COMMENT_TRACE(C, ctx_);
    ScopeExitRestore s1(it_, C->child_begin());
    ScopeExitRestore s2(end_, C->child_end());
    while(it_ != end_)
    {
        MRDOCS_COMMENT_TRACE(*it_, ctx_);
        visit(*it_);
        ++it_;
    }

    if (!block_)
    {
        return;
    }

    bool const isVerbatim = block_->kind == doc::Kind::code;
    if (isVerbatim)
    {
        return;
    }

    // Merge consecutive plain text nodes in the current block
    auto it = block_->children.begin();
    while(it != block_->children.end())
    {
        auto& child = *it;
        if (child.get()->kind == doc::Kind::text)
        {
            auto* text = dynamic_cast<doc::Text*>(child.get());
            MRDOCS_ASSERT(text);
            auto next = std::next(it);
            if(next != block_->children.end())
            {
                if(next->get()->kind == doc::Kind::text)
                {
                    auto* next_text = dynamic_cast<doc::Text*>(next->get());
                    MRDOCS_ASSERT(next_text);
                    text->string.append(next_text->string);
                    it = block_->children.erase(next);
                    continue;
                }
            }
        }
        ++it;
    }

    // Parse any Text nodes for styled text
    for (auto it = block_->children.begin(); it != block_->children.end();)
    {
        MRDOCS_ASSERT(it->get());
        if (it->get()->kind == doc::Kind::text)
        {
            auto* text = dynamic_cast<doc::Text*>(it->get());
            auto styledText = parseStyled(text->string);
            std::size_t const offset = std::distance(block_->children.begin(), it);
            std::size_t const n = styledText.size();
            block_->children.erase(it);
            block_->children.insert(
                block_->children.begin() + offset,
                std::make_move_iterator(styledText.begin()),
                std::make_move_iterator(styledText.end()));
            it = block_->children.begin() + offset + n;
        }
        else
        {
            ++it;
        }
    }
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
    MRDOCS_COMMENT_TRACE(FC_, ctx_);
    visit(FC_);

    // Merge ListItems into UnorderedList
    auto& blocks = jd_.getBlocks();
    for (auto it = blocks.begin(); it != blocks.end(); ) {
        if ((*it)->kind == doc::Kind::list_item) {
            doc::UnorderedList ul;
            // Find last list item
            auto const begin = it;
            auto last = it;
            while (last != blocks.end() && (*last)->kind == doc::Kind::list_item) {
                ++last;
            }
            // Move list items to ul.items
            ul.items.reserve(std::distance(it, last));
            for (auto li_it = begin; li_it != last; ++li_it) {
                std::unique_ptr<doc::Block> block = std::move(*li_it);
                MRDOCS_ASSERT(dynamic_cast<doc::ListItem*>(block.get()));
                doc::Block* raw_block_ptr = block.release();
                auto const raw_li_ptr = static_cast<doc::ListItem*>(raw_block_ptr);
                auto li = std::make_unique<doc::ListItem>(std::move(*raw_li_ptr));
                ul.items.emplace_back(std::move(li));
            }
            // Remove the list items and insert the ul
            it = blocks.erase(begin, last);
            it = blocks.insert(it, std::make_unique<doc::UnorderedList>(std::move(ul)));
        }
        ++it;
    }

    return std::move(jd_);
}

void
JavadocVisitor::
visitComment(
    Comment const* C)
{
    MRDOCS_COMMENT_TRACE(C, ctx_);
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
    MRDOCS_COMMENT_TRACE(C, ctx_);
    llvm::StringRef s = C->getText();
    // If this is the first text comment in the
    // paragraph then remove all the leading space.
    // Otherwise, just remove the trailing space.
    if (block_->children.empty())
    {
        s = s.ltrim();
    }

    // Only insert non-empty text nodes
    if(! s.empty())
    {
        emplaceText<doc::Text>(
            C->hasTrailingNewline(),
            ensureUTF8(s.str()));
    }
}

Expected<JavadocVisitor::TagComponents>
JavadocVisitor::
parseHTMLTag(HTMLStartTagComment const* C)
{
    MRDOCS_COMMENT_TRACE(C, ctx_);
    TagComponents res;
    res.tag = C->getTagName().str();

    static constexpr std::array noEndTagTags =
            {"br", "img", "input", "hr", "meta", "link", "base", "area", "col",
             "command", "embed", "keygen", "param", "source", "track", "wbr"};
    bool const requiresEndTag = std::ranges::find(noEndTagTags, res.tag) == noEndTagTags.end();
    if (!requiresEndTag)
    {
        return res;
    }

    // Find Comment::HTMLEndTagCommentKind
    auto const tagEndIt =
        requiresEndTag ? std::ranges::find_if(it_ + 1, end_, [](Comment const* c)
        {
            return c->getCommentKind() == CommentKind::HTMLEndTagComment;
        }) : it_;
    if (tagEndIt == end_)
    {
        return Unexpected(formatError("warning: HTML <{}> tag not followed by end tag", res.tag));
    }

    // Check if end tag matches start tag
    auto const& cEndTag =
        *static_cast<HTMLEndTagComment const*>(*tagEndIt);
    if(cEndTag.getTagName() != res.tag)
    {
        return Unexpected(Error(fmt::format("warning: HTML <{}> tag not followed by same end tag (</{}> found)", res.tag, cEndTag.getTagName().str())));
    }

    // Check if all the siblings are text nodes
    bool const areAllText = std::all_of(it_ + 1, tagEndIt, [](Comment const* c)
    {
        return c->getCommentKind() == CommentKind::TextComment;
    });
    if (!areAllText)
    {
        return Unexpected(Error(fmt::format("warning: HTML <{}> tag not followed by text", res.tag)));
    }

    // Extract text from all the siblings
    res.n_siblings = std::distance(it_, tagEndIt);
    for (auto it = std::next(it_); it != tagEndIt; ++it)
    {
        auto const& cText =
            *static_cast<TextComment const*>(*it);
        res.text += cText.getText();
    }

    return res;
}

void
JavadocVisitor::
visitHTMLStartTagComment(
    HTMLStartTagComment const* C)
{
    MRDOCS_COMMENT_TRACE(C, ctx_);
    MRDOCS_ASSERT(C->child_begin() == C->child_end());
    PresumedLoc const loc = sm_.getPresumedLoc(C->getBeginLoc());
    auto filename = files::makePosixStyle(loc.getFilename());

    auto getAttribute = [&C](StringRef name) -> Expected<std::string>
    {
        auto idxs = std::views::iota(unsigned(0), C->getNumAttrs());
        auto attr_it = std::ranges::find_if(idxs, [&C, name](std::size_t i)
        {
            return C->getAttr(i).Name == name;
        });
        if (attr_it == idxs.end())
        {
            return Unexpected(Error(fmt::format("warning: HTML <{}> tag has no {} attribute", C->getTagName().str(), name.str())));
        }
        return C->getAttr(*attr_it).Value.str();
    };

    last_child_ = nullptr;
    auto tagComponentsExp = parseHTMLTag(C);
    if (!tagComponentsExp)
    {
        Error e = tagComponentsExp.error();
        report::error("{} at {} ({})", e.message(), filename, loc.getLine());
        return;
    }
    auto tagComponents = *tagComponentsExp;
    if(tagComponents.tag == "a")
    {
        auto r = getAttribute("href");
        if (!r)
        {
            report::error(r.error().message());
            return;
        }
        std::string href = *r;
        emplaceText<doc::Link>(
            C->hasTrailingNewline(),
            ensureUTF8(std::move(tagComponents.text)),
            ensureUTF8(std::move(href)));
    }
    else if(tagComponents.tag == "br")
    {
        emplaceText<doc::Text>(true,"");
    }
    else if(tagComponents.tag == "em")
    {
        emplaceText<doc::Styled>(
            C->hasTrailingNewline(),
            ensureUTF8(std::move(tagComponents.text)),
            doc::Style::italic);
    }
    else
    {
        report::warn(fmt::format("warning: unsupported HTML tag <{}>", tagComponents.tag), filename, loc.getLine());
    }
    // Skip the children we consumed in parseHTMLTag
    it_ += tagComponents.n_siblings;
    --htmlTagNesting_;
}

void
JavadocVisitor::
visitHTMLEndTagComment(
    HTMLEndTagComment const* C)
{
    MRDOCS_COMMENT_TRACE(C, ctx_);
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
    {
        report::error("error: unsupported CommandTrait id <{}>", id);
        MRDOCS_UNREACHABLE();
    }
    }
}

static
doc::Style
convertStyle(InlineCommandRenderKind kind)
{
    switch(kind)
    {
    case InlineCommandRenderKind::Monospaced:
        return doc::Style::mono;
    case InlineCommandRenderKind::Bold:
        return doc::Style::bold;
    case InlineCommandRenderKind::Emphasized:
        return doc::Style::italic;
    case InlineCommandRenderKind::Normal:
    case InlineCommandRenderKind::Anchor:
        return doc::Style::none;
    default:
        // unknown RenderKind
        report::error("error: unsupported InlineCommandRenderKind <{}>", static_cast<int>(kind));
        MRDOCS_UNREACHABLE();
    }
}

static
doc::ParamDirection
convertDirection(ParamCommandPassDirection kind)
{
    switch(kind)
    {
    case ParamCommandPassDirection::In:
        return doc::ParamDirection::in;
    case ParamCommandPassDirection::Out:
        return doc::ParamDirection::out;
    case ParamCommandPassDirection::InOut:
        return doc::ParamDirection::inout;
    default:
        report::error("error: unsupported ParamCommandPassDirection <{}>", static_cast<int>(kind));
        MRDOCS_UNREACHABLE();
    }
}

/** Parse first chars of string that represent an identifier
 */
std::string_view
parseIdentifier(std::string_view str)
{
    static constexpr auto idChars =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "_";
    static constexpr auto operatorChars =
        "~!%^&*()-+=|[]{};:,.<>?/";
    if (str.empty())
    {
        return {};
    }

    std::size_t p = str.find_first_not_of(idChars);
    if (p == std::string_view::npos)
    {
        return str;
    }

    if (str.substr(0, p) == "operator")
    {
        p = str.find_first_not_of(operatorChars, p);
        if (p == std::string_view::npos)
        {
            return str;
        }
    }

    return str.substr(0, p);
}

/** Parse first chars of string that represent an identifier
 */
std::string_view
parseQualifiedIdentifier(std::string_view str)
{
    auto str0 = str;
    std::size_t off = 0;
    if (str.starts_with("::"))
    {
        off += 2;
        str.remove_prefix(2);
    }

    bool atIdentifier = true;
    while (!str.empty())
    {
        if (atIdentifier)
        {
            auto idStr = parseIdentifier(str);
            if (!idStr.empty())
            {
                off += idStr.size();
                str = str.substr(idStr.size());
                atIdentifier = false;
            }
            else
            {
                break;
            }
        }
        else
        {
            // At delimiter
            if (str.starts_with("::"))
            {
                off += 2;
                str = str.substr(2);
                atIdentifier = true;
            }
            else
            {
                break;
            }
        }
    }
    std::string_view result = str0.substr(0, off);
    if (result.ends_with("::"))
    {
        result = result.substr(0, result.size() - 2);
    }
    return result;
}

void
JavadocVisitor::
visitInlineCommandComment(
    InlineCommandComment const* C)
{
    MRDOCS_COMMENT_TRACE(C, ctx_);
    auto const* cmd = ctx_
        .getCommentCommandTraits()
        .getCommandInfo(C->getCommandID());

    // VFALCO I'd like to know when this happens
    MRDOCS_ASSERT(cmd != nullptr);

    switch(unsigned ID = cmd->getID())
    {
    // Newline
    case CommandTraits::KCI_n:
    {
        if(! goodArgCount(0, *C))
            return;
        last_child_ = nullptr;
        emplaceText<doc::Text>(true, "\n");
        return;
    }
    // Emphasis
    case CommandTraits::KCI_a:
    case CommandTraits::KCI_e:
    case CommandTraits::KCI_em:
    {
        if(! goodArgCount(1, *C))
            return;
        auto style = doc::Style::italic;
        emplaceText<doc::Styled>(
            C->hasTrailingNewline(),
            C->getArgText(0).str(),
            style);
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
        // the finalization step once all symbols are extracted
        std::string const &s = C->getArgText(0).str();
        bool const copyingFunctionDoc = s.find('(') != std::string::npos;
        std::string ref = s;
        if (copyingFunctionDoc)
        {
            // Clang parses the copydoc command breaking
            // before the complete overload information. For instance,
            // `@copydoc operator()(unsigned char) const` will create
            // a node with the text `operator()(unsigned` and another
            // with `char) const`. We need to merge these nodes.
            std::size_t open = std::ranges::count(s, '(');
            std::size_t close = std::ranges::count(s, ')');
            while (open != close)
            {
                ++it_;
                if (it_ == end_)
                {
                    break;
                }
                auto const* c = *it_;
                if (c->getCommentKind() == CommentKind::TextComment)
                {
                    ref += static_cast<TextComment const*>(c)->getText();
                }
                else
                {
                    break;
                }
                open = std::ranges::count(ref, '(');
                close = std::ranges::count(ref, ')');
            }
        }
        emplaceText<doc::Copied>(
            C->hasTrailingNewline(),
            ref,
            convertCopydoc(ID));
        return;
    }
    case CommandTraits::KCI_ref:
    {
        if(! goodArgCount(1, *C))
            return;
        // The parsed reference often includes characters
        // that are not valid in identifiers, so we need to
        // clean it up.
        // Find the first character that is not a valid C++
        // identifier character, and truncate the string there.
        // This potentially creates two text nodes.
        auto const s = C->getArgText(0).str();
        std::string_view ref = parseQualifiedIdentifier(s);
        bool const hasExtraText = ref.size() != s.size();
        if (!ref.empty())
        {
            // the referenced symbol will be resolved during
            // the finalization step once all symbols are extracted
            emplaceText<doc::Reference>(
                C->hasTrailingNewline() && !hasExtraText,
                std::string(ref));
        }
        // Emplace the rest of the string as doc::Text
        if(hasExtraText)
        {
            emplaceText<doc::Text>(
                C->hasTrailingNewline(),
                s.substr(ref.size()));
        }
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
        emplaceText<doc::Styled>(
            C->hasTrailingNewline(),
            std::move(s),
            style);
    else
        emplaceText<doc::Text>(
            C->hasTrailingNewline(),
            std::move(s));
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
    MRDOCS_COMMENT_TRACE(C, ctx_);
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
    MRDOCS_COMMENT_TRACE(C, ctx_);
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
    case CommandTraits::KCI_result:
    {
        auto itr = std::ranges::find_if(
            jd_.getBlocks(),
            [&](const std::unique_ptr<doc::Block> & b)
        {
            return b->kind == doc::Kind::returns;
        });
        if (itr != jd_.getBlocks().end())
        {
            report::warn("{}: Duplicate @returns statement", C->getBeginLoc().printToString(sm_));
        }

        doc::Returns returns;
        auto scope = enterScope(returns);
        // Scope scope(returns, block_);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(returns));
        return;
    }
    case CommandTraits::KCI_throw:
    case CommandTraits::KCI_throws:
    case CommandTraits::KCI_exception:
    {
        doc::Throws throws;
        auto scope = enterScope(throws);
        visitChildren(C->getParagraph());
        if (C->getNumArgs())
        {
            throws.exception.string = C->getArgText(0);
        }
        else
        {
            throws.exception.string = "undefined";
        }
        jd_.emplace_back(std::move(throws));
        return;
    }
    case CommandTraits::KCI_note:
    case CommandTraits::KCI_warning:
    {
        doc::Admonish admonish = cmd->getID() == CommandTraits::KCI_note
            ? doc::Admonish::note
            : doc::Admonish::warning;
        doc::Admonition paragraph(admonish);
        auto scope = enterScope(paragraph);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(paragraph));
        return;
    }
    case CommandTraits::KCI_par:
    {
        // VFALCO This is legacy compatibility
        // for Boost libraries using @par as a
        // section heading.
        doc::Paragraph paragraph;
        auto scope = enterScope(paragraph);
        visitChildren(C->getParagraph());
        if (C->getNumArgs() > 0)
        {
            jd_.emplace_back(doc::Heading(C->getArgText(0).str()));
        }
        if (!paragraph.children.empty())
        {
            // the first TextComment is the heading text
            if (C->getNumArgs() == 0)
            {
                doc::String text(std::move(
                        paragraph.children.front()->string));

                // VFALCO Unfortunately clang puts at least
                // one space in front of the text, which seems
                // incorrect.
                if (auto const s = trim(text);
                    s.size() != text.size())
                {
                    text = s;
                }

                doc::Heading heading(std::move(text));
                jd_.emplace_back(std::move(heading));

                // remaining TextComment, if any
                paragraph.children.erase(paragraph.children.begin());
            }

            if (!paragraph.children.empty())
            {
                jd_.emplace_back(std::move(paragraph));
            }
        }
        return;
    }
    case CommandTraits::KCI_li:
    {
        doc::ListItem paragraph;
        auto scope = enterScope(paragraph);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(paragraph));
        return;
    }
    case CommandTraits::KCI_details:
    {
        doc::Details paragraph;
        auto scope = enterScope(paragraph);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(paragraph));
        return;
    }
    case CommandTraits::KCI_see:
    {
        doc::See paragraph;
        auto scope = enterScope(paragraph);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(paragraph));
        return;
    }
    case CommandTraits::KCI_pre:
    {
        doc::Precondition paragraph;
        auto scope = enterScope(paragraph);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(paragraph));
        return;
    }
    case CommandTraits::KCI_post:
    {
        doc::Postcondition paragraph;
        auto scope = enterScope(paragraph);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(paragraph));
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
    case CommandTraits::KCI_line:
    //case CommandTraits::KCI_lineinfo:
    case CommandTraits::KCI_link:
    case CommandTraits::KCI_mainpage:
    case CommandTraits::KCI_maninclude:
    case CommandTraits::KCI_manonly:
    case CommandTraits::KCI_memberof:
    case CommandTraits::KCI_msc:
    case CommandTraits::KCI_mscfile:
    case CommandTraits::KCI_name:
    case CommandTraits::KCI_namespace:
    case CommandTraits::KCI_noop:
    case CommandTraits::KCI_nosubgrouping:
    case CommandTraits::KCI_overload:
    case CommandTraits::KCI_p:
    //case CommandTraits::KCI_package:
    case CommandTraits::KCI_page:
    case CommandTraits::KCI_paragraph:
    case CommandTraits::KCI_param:
    case CommandTraits::KCI_parblock:
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

    case CommandTraits::KCI_retval:
    case CommandTraits::KCI_rtfinclude:
    case CommandTraits::KCI_rtfonly:
    case CommandTraits::KCI_sa:
    case CommandTraits::KCI_secreflist:
    case CommandTraits::KCI_section:
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
    case CommandTraits::KCI_n:
    case CommandTraits::KCI_copybrief:
    case CommandTraits::KCI_copydetails:
    case CommandTraits::KCI_copydoc:
        report::error("error: inline command {} should be handled elsewhere", cmd->Name);
        MRDOCS_UNREACHABLE();
    default:
        report::error("error: unsupported block command {}", cmd->Name);
        MRDOCS_UNREACHABLE();
    }
}

void
JavadocVisitor::
visitParamCommandComment(
    ParamCommandComment const* C)
{
    MRDOCS_COMMENT_TRACE(C, ctx_);
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

    auto itr = std::ranges::find_if(
        jd_.getBlocks(),
        [&](const std::unique_ptr<doc::Block> & b)
    {
        if (b->kind != doc::Kind::param)
            return false;
        auto p = dynamic_cast<const doc::Param*>(b.get());
        MRDOCS_ASSERT(p != nullptr);
        return p->name == param.name;
    });
    if (itr != jd_.getBlocks().end())
    {
        report::warn(
            "{}: Duplicate @param for argument {}",
            C->getBeginLoc().printToString(sm_), param.name);
    }

    // We want the node even if it is empty
    jd_.emplace_back(std::move(param));
}

void
JavadocVisitor::
visitTParamCommandComment(
    TParamCommandComment const* C)
{
    MRDOCS_COMMENT_TRACE(C, ctx_);
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

    auto itr = std::ranges::find_if(
        jd_.getBlocks(),
        [&](const std::unique_ptr<doc::Block> & b)
    {
        if (b->kind != doc::Kind::tparam)
            return false;
        auto tp = dynamic_cast<const doc::TParam*>(b.get());
        MRDOCS_ASSERT(tp != nullptr);
        return tp->name == tparam.name;
    });
    if (itr != jd_.getBlocks().end())
    {
        report::warn(
            "{}: Duplicate @tparam for argument {}",
            C->getBeginLoc().printToString(sm_), tparam.name);
    }

    // We want the node even if it is empty
    jd_.emplace_back(std::move(tparam));
}

void
JavadocVisitor::
visitVerbatimBlockComment(
    VerbatimBlockComment const* C)
{
    MRDOCS_COMMENT_TRACE(C, ctx_);
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
    MRDOCS_COMMENT_TRACE(C, ctx_);
    // VFALCO This doesn't seem to be used
    //        in any of my codebases, follow up
}

void
JavadocVisitor::
visitVerbatimBlockLineComment(
    VerbatimBlockLineComment const* C)
{
    MRDOCS_COMMENT_TRACE(C, ctx_);
    emplaceText<doc::Text>(true, C->getText().str());
}

//------------------------------------------------

bool
JavadocVisitor::
goodArgCount(std::size_t n,
    InlineCommandComment const& C)
{
    MRDOCS_COMMENT_TRACE(C, ctx_);
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
    FullComment const* FC,
    Decl const* D,
    Config const& config,
    Diagnostics& diags)
{
    MRDOCS_COMMENT_TRACE(FC, D->getASTContext());
    JavadocVisitor visitor(FC, D, config, diags);
    auto result = visitor.build();
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
