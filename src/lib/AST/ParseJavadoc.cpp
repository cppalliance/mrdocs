//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ParseJavadoc.hpp"
#include "lib/AST/ParseRef.hpp"
#include <cctype>
#include <clang/AST/ASTContext.h>
#include <clang/AST/CommentCommandTraits.h>
#include <clang/AST/RawCommentList.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/Lexer.h>
#include <format>
#include <mrdocs/Metadata/Javadoc.hpp>
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/ScopeExit.hpp>
#include <mrdocs/Support/String.hpp>

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
#include <llvm/ADT/StringExtras.h>
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
    report::trace("{}", std::string_view(MRDOCS_COMMENT_TRACE_UNIQUE_NAME.str()))
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

namespace clang::mrdocs {

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
    doc::Block* block_ = nullptr;
    doc::Text* last_child_ = nullptr;
    std::size_t htmlTagNesting_ = 0;

    // Used to visit children of a comment
    Comment const* parent_ = nullptr;
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

    /** Fix a reference string

        The clang parser considers all chars up to the first
        whitespace as part of the reference.

        In some cases, this causes the reference to be incomplete,
        especially when handling a function signature. In this
        case, we have to extract the text from the next comments
        to complete the reference.

        In other cases, the reference may contain characters that
        are not valid in identifiers. For instance, when the
        identifier is followed by punctuation. In this case, we
        have to truncate the reference at the last valid identifier
        character.

        @param ref The reference string to fix
        @return Any leftover text that removed from the reference
     */
    std::string
    fixReference(std::string& ref);

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
    emplaceText(bool const end_with_nl, Args&&... args)
    {
        TextTy elem(std::forward<Args>(args)...);
        bool can_merge = false;

        if (last_child_ && last_child_->Kind == elem.Kind)
        {
            if constexpr(TextTy::static_kind == doc::NodeKind::text)
                can_merge = true;

            if constexpr(TextTy::static_kind == doc::NodeKind::styled)
                can_merge = dynamic_cast<doc::Styled*>(
                    last_child_)->style == elem.style;
        }

        if (!can_merge)
        {
            auto new_text = MakePolymorphic<TextTy>(std::move(elem));
            last_child_ = new_text.operator->();
            block_->children.emplace_back(std::move(new_text));
        }
        else
        {
            last_child_->string.append(elem.string);
        }

        if (end_with_nl)
        {
            last_child_ = nullptr;
        }
    }

    /** @} */
};

//------------------------------------------------

std::string &JavadocVisitor::ensureUTF8(std::string &&s) {
  if (!llvm::json::isUTF8(s))
    s = llvm::json::fixUTF8(s);
  return static_cast<std::string &>(s);
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
std::vector<Polymorphic<doc::Text>>
parseStyled(StringRef s)
{
    std::vector<Polymorphic<doc::Text>> result;
    std::string currentText;
    auto currentStyle = doc::Style::none;
    bool escapeNext = false;

    auto isStyleMarker = [](char c) {
        return c == '`' || c == '*' || c == '_';
    };

    auto flushCurrentText = [&]() {
        if (!currentText.empty()) {
            if (currentStyle == doc::Style::none) {
                bool const lastIsSame =
                    !result.empty() &&
                    result.back()->Kind == doc::NodeKind::text;
                if (lastIsSame)
                {
                    auto& lastText = static_cast<doc::Text&>(*result.back());
                    lastText.string.append(currentText);
                }
                else
                {
                    result.emplace_back(MakePolymorphic<doc::Text>(std::move(currentText)));
                }
            } else {
                bool const lastIsSame =
                    !result.empty() &&
                    result.back()->Kind == doc::NodeKind::styled &&
                    dynamic_cast<doc::Styled&>(*result.back()).style == currentStyle;
                if (lastIsSame)
                {
                    auto& lastStyled = dynamic_cast<doc::Styled&>(*result.back());
                    lastStyled.string.append(currentText);
                }
                else
                {
                    result.emplace_back(MakePolymorphic<doc::Styled>(std::move(currentText), currentStyle));
                }
            }
            currentText.clear();
        }
    };

    auto isPunctuationOrSpace = [](unsigned char c) {
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
    ScopeExitRestore s3(parent_, C);
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

    bool const isVerbatim = block_->Kind == doc::NodeKind::code;
    if (isVerbatim)
    {
        return;
    }

    // Merge consecutive plain text nodes in the current block
    auto it = block_->children.begin();
    while(it != block_->children.end())
    {
        if (auto& child = *it;
            child->Kind == doc::NodeKind::text)
        {
            auto* text = dynamic_cast<doc::Text*>(child.operator->());
            MRDOCS_ASSERT(text);
            auto next = std::next(it);
            if(next != block_->children.end())
            {
                if((*next)->Kind == doc::NodeKind::text)
                {
                    auto* next_text = dynamic_cast<doc::Text*>(next->operator->());
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
    for (auto cIt = block_->children.begin(); cIt != block_->children.end();)
    {
        MRDOCS_ASSERT(cIt->operator->());
        if ((*cIt)->Kind == doc::NodeKind::text)
        {
            auto* text = dynamic_cast<doc::Text*>(cIt->operator->());
            auto styledText = parseStyled(text->string);
            std::size_t const offset = std::distance(block_->children.begin(), cIt);
            std::size_t const n = styledText.size();
            block_->children.erase(cIt);
            block_->children.insert(
                block_->children.begin() + offset,
                std::make_move_iterator(styledText.begin()),
                std::make_move_iterator(styledText.end()));
            cIt = block_->children.begin() + offset + n;
        }
        else
        {
            ++cIt;
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
        if ((*it)->Kind == doc::NodeKind::list_item) {
            doc::UnorderedList ul;
            // Find last list item
            auto const begin = it;
            auto last = it;
            while (last != blocks.end() && (*last)->Kind == doc::NodeKind::list_item) {
                ++last;
            }
            // Move list items to ul.items
            ul.items.reserve(std::distance(it, last));
            for (auto li_it = begin; li_it != last; ++li_it) {
                Polymorphic<doc::Block> block = std::move(*li_it);
                MRDOCS_ASSERT(IsA<doc::ListItem>(block));
                Polymorphic<doc::ListItem> li = DynamicCast<doc::ListItem>(std::move(block));
                ul.items.emplace_back(std::move(*li));
            }
            // Remove the list items and insert the ul
            it = blocks.erase(begin, last);
            it = blocks.insert(
                it,
                Polymorphic<doc::Block>(
                    MakePolymorphic<doc::UnorderedList>(std::move(ul))));
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
      return Unexpected(Error(std::format(
          "warning: HTML <{}> tag not followed by same end tag (</{}> found)",
          res.tag, cEndTag.getTagName().str())));
    }

    // Check if all the siblings are text nodes
    bool const areAllText = std::all_of(it_ + 1, tagEndIt, [](Comment const* c)
    {
        return c->getCommentKind() == CommentKind::TextComment;
    });
    if (!areAllText)
    {
      return Unexpected(Error(
          std::format("warning: HTML <{}> tag not followed by text", res.tag)));
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
          return Unexpected(
              Error(std::format("warning: HTML <{}> tag has no {} attribute",
                                C->getTagName().str(), name.str())));
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
      report::warn(
          std::format("warning: unsupported HTML tag <{}>", tagComponents.tag),
          filename, loc.getLine());
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
        MRDOCS_CHECK_OR(goodArgCount(1, *C));
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
        MRDOCS_CHECK_OR(goodArgCount(1, *C));
        std::string ref = C->getArgText(0).str();
        std::string leftOver = fixReference(ref);
        bool const hasExtra = !leftOver.empty();
        doc::Parts const parts = convertCopydoc(ID);
        bool const copyBrief = parts == doc::Parts::brief || parts == doc::Parts::all;
        bool const copyDetails = parts == doc::Parts::description || parts == doc::Parts::all;
        MRDOCS_ASSERT(copyBrief || copyDetails);
        if (copyBrief)
        {
            // The brief is metadata associated with the javadoc
            if (!jd_.brief)
            {
                jd_.brief.emplace();
            }
            if (!contains(jd_.brief->copiedFrom, ref))
            {
                jd_.brief->copiedFrom.emplace_back(ref);
            }
        }
        if (copyDetails)
        {
            // The details are in the main body of the javadoc
            // and are replaced at the same position as the command
            emplaceText<doc::CopyDetails>(
                C->hasTrailingNewline() && !hasExtra,
                ref);
        }
        if (hasExtra)
        {
            emplaceText<doc::Text>(
                C->hasTrailingNewline(),
                leftOver);
        }
        return;
    }
    case CommandTraits::KCI_ref:
    {
        MRDOCS_CHECK_OR(goodArgCount(1, *C));
        std::string ref = C->getArgText(0).str();
        std::string leftOver = fixReference(ref);
        bool const hasExtra = !leftOver.empty();
        emplaceText<doc::Reference>(
            C->hasTrailingNewline() && !hasExtra,
            ref);
        if (hasExtra)
        {
            emplaceText<doc::Text>(
                C->hasTrailingNewline(),
                leftOver);
        }
        return;
    }
    // KRYSTIAN FIXME: these need to be made inline commands in clang
    case CommandTraits::KCI_related:
    case CommandTraits::KCI_relates:
    // MrDocs doesn't document members inline, so there's no
    // distinction between "related" and "relatedalso"
    case CommandTraits::KCI_relatedalso:
    case CommandTraits::KCI_relatesalso:
    // Member of is a concept used only in C. MrDocs handles
    // it as a non-member function is all cases.
    case CommandTraits::KCI_memberof:
    {
        MRDOCS_CHECK_OR(goodArgCount(1, *C));
        std::string ref = C->getArgText(0).str();
        std::string leftOver = fixReference(ref);
        bool const hasExtra = !leftOver.empty();
        jd_.relates.emplace_back(std::move(ref));
        if (hasExtra)
        {
            emplaceText<doc::Text>(
                    C->hasTrailingNewline(),
                    leftOver);
        }
        return;
    }

    default:
        break;
    }

    // It looks like the clang parser does not
    // emit nested styles, so only one inline
    // style command can be applied per args.
    std::string s;
    std::size_t n = 0;
    for (unsigned i = 0; i < C->getNumArgs(); ++i)
    {
        n += C->getArgText(i).size();
    }
    s.reserve(n);
    for (unsigned i = 0; i < C->getNumArgs(); ++i)
    {
        s.append(C->getArgText(i));
    }

    if (doc::Style style = convertStyle(C->getRenderKind());
        style != doc::Style::none)
    {
        emplaceText<doc::Styled>(C->hasTrailingNewline(), std::move(s), style);
    } else
    {
        emplaceText<doc::Text>(C->hasTrailingNewline(), std::move(s));
    }
}

std::string
JavadocVisitor::
fixReference(std::string& ref)
{
    auto peekNextIt = [&]() -> std::optional<std::string_view>
    {
        ++it_;
        if (it_ == end_ ||
            (*it_)->getCommentKind() != CommentKind::TextComment)
        {
            --it_;
            return std::nullopt;
        }
        Comment const* c = *it_;
        std::string_view text = static_cast<TextComment const*>(c)->getText();
        --it_;
        return text;
    };

    ParsedRef v;
    while (true)
    {
        // Attempt to parse ref
        char const* first = ref.data();
        char const* last = first + ref.size();
        auto const pres = parse(first, last, v);
        if (!pres)
        {
            // The ref could not be parsed, add content from next
            // text comment to the ref
            auto const nextTextOpt = peekNextIt();
            if (!nextTextOpt)
            {
                return {};
            }
            ref += *nextTextOpt;
            ++it_;
            continue;
        }

        // The ref is not fully parsed
        if (pres.ptr != last)
        {
            // The ref didn't consume all the text, so we need to
            // remove the leftover text from the ref and return it
            auto leftover = std::string(pres.ptr, last - pres.ptr);
            // If leftover is only whitespace, the ref might need
            // the next text comment to complete it.
            if (!isWhitespace(leftover))
            {
                ref.erase(pres.ptr - first);
                return leftover;
            }
        }

        // The ref is fully parsed, but we might want to
        // include the next text comment if it contains
        // a valid continuation to the ref.
        bool const mightHaveMoreQualifiers =
            v.HasFunctionParameters &&
            v.ExceptionSpec.Implicit &&
            v.ExceptionSpec.Operand.empty();
        if (mightHaveMoreQualifiers)
        {
            llvm::SmallVector<std::string_view, 4> potentialQualifiers;
            if (v.Kind == ReferenceKind::None)
            {
                // "&&" or "&" not defined yet
                if (!v.IsConst)
                {
                    potentialQualifiers.push_back("const");
                }
                if (!v.IsVolatile)
                {
                    potentialQualifiers.push_back("volatile");
                }
                potentialQualifiers.push_back("&");
            }
            else if (
                v.Kind == ReferenceKind::LValue &&
                ref.ends_with('&'))
            {
                // The second "&" might be in the next Text block
                potentialQualifiers.push_back("&");
            }
            potentialQualifiers.push_back("noexcept");
            auto const nextTextOpt = peekNextIt();
            if (!nextTextOpt)
            {
                auto leftover = std::string(pres.ptr, last - pres.ptr);
                ref.erase(pres.ptr - first);
                return leftover;
            }
            std::string_view const nextText = *nextTextOpt;
            std::string_view const trimmed = ltrim(nextText);
            if (trimmed.empty() ||
                std::ranges::any_of(
                    potentialQualifiers,
                    [&](std::string_view s)
                    {
                        return trimmed.starts_with(s);
                    }))
            {
                ref += nextText;
                ++it_;
                continue;
            }
        }

        // The ref might have more components
        bool const mightHaveMoreComponents =
            !v.HasFunctionParameters;
        if (mightHaveMoreComponents)
        {
            auto const nextTextOpt = peekNextIt();
            if (!nextTextOpt)
            {
                auto leftover = std::string(pres.ptr, last - pres.ptr);
                ref.erase(pres.ptr - first);
                return leftover;
            }
            std::string_view const nextText = *nextTextOpt;
            std::string_view const trimmed = ltrim(nextText);
            static constexpr std::string_view idChars
                   = "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                     "0123456789"
                     "_:";
            if (trimmed.empty() ||
                contains(idChars, trimmed.front()))
            {
                ref += nextText;
                ++it_;
                continue;
            }
        }

        return {};
    }
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
        if (!brief.children.empty())
        {
            jd_.brief.emplace(std::move(brief));
        }
        return;
    }

    case CommandTraits::KCI_return:
    case CommandTraits::KCI_returns:
    case CommandTraits::KCI_result:
    {
        doc::Returns returns;
        auto scope = enterScope(returns);
        visitChildren(C->getParagraph());
        if (!returns.children.empty())
        {
            returns.children.front()->string = ltrim(returns.children.front()->string);
            returns.children.back()->string = rtrim(returns.children.back()->string);
            jd_.returns.push_back(std::move(returns));
        }
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
        jd_.exceptions.push_back(std::move(throws));
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
                std::string text(std::move(
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
        doc::Paragraph paragraph;
        auto scope = enterScope(paragraph);
        visitChildren(C->getParagraph());
        jd_.emplace_back(std::move(paragraph));
        return;
    }
    case CommandTraits::KCI_see:
    {
        doc::See see;
        auto scope = enterScope(see);
        visitChildren(C->getParagraph());
        jd_.sees.push_back(std::move(see));
        return;
    }
    case CommandTraits::KCI_pre:
    {
        doc::Precondition pre;
        auto scope = enterScope(pre);
        visitChildren(C->getParagraph());
        jd_.preconditions.push_back(std::move(pre));
        return;
    }
    case CommandTraits::KCI_post:
    {
        doc::Postcondition post;
        auto scope = enterScope(post);
        visitChildren(C->getParagraph());
        jd_.postconditions.push_back(std::move(post));
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

    auto const itr = std::ranges::find_if(
        jd_.getBlocks(),
        [&](Polymorphic<doc::Block> const& b)
    {
        if (b->Kind != doc::NodeKind::param)
        {
            return false;
        }
        auto p = dynamic_cast<doc::Param const*>(b.operator->());
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
    jd_.params.push_back(std::move(param));
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

    auto const itr = std::ranges::find_if(
        jd_.getBlocks(),
        [&](Polymorphic<doc::Block> const& b)
    {
        if (b->Kind != doc::NodeKind::tparam)
        {
            return false;
        }
        auto const tp = dynamic_cast<doc::TParam const*>(b.operator->());
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
    jd_.tparams.push_back(std::move(tparam));
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

        diags_.error(std::format("Expected {} but got {} args\n"
                                 "File: {}, line {}, col {}\n",
                                 n, C.getNumArgs(), loc.getFilename(),
                                 loc.getLine(), loc.getColumn()));

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
    std::optional<Javadoc>& jd,
    FullComment const* FC,
    Decl const* D,
    Config const& config,
    Diagnostics& diags)
{
    MRDOCS_COMMENT_TRACE(FC, D->getASTContext());
    JavadocVisitor visitor(FC, D, config, diags);
    auto result = visitor.build();
    if (!result.empty())
    {
        if (!jd)
        {
            jd = std::move(result);
        }
        else if(*jd != result)
        {
            // merge
            jd->append(std::move(result));
        }
    }

}

} // clang::mrdocs
