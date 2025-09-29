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

#include "ExtractDocComment.hpp"
#include <lib/AST/ParseRef.hpp>
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/DocComment/Inline/Parts.hpp>
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/ScopeExit.hpp>
#include <mrdocs/Support/String.hpp>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Comment.h>
#include <clang/AST/CommentCommandTraits.h>
#include <clang/AST/CommentVisitor.h>
#include <clang/AST/RawCommentList.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/JSON.h>
#include <cctype>
#include <format>
#include <ranges>
#include <string_view>

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable: 5054)
#    pragma warning(pop)
#endif

#ifdef NDEBUG
#    define MRDOCS_COMMENT_TRACE(D, Ctx)
#else
#    define MRDOCS_COMMENT_TRACE_MERGE_(a, b) a##b
#    define MRDOCS_COMMENT_TRACE_LABEL_(a) \
        MRDOCS_COMMENT_TRACE_MERGE_(comment_content_, a)
#    define MRDOCS_COMMENT_TRACE_UNIQUE_NAME \
        MRDOCS_COMMENT_TRACE_LABEL_(__LINE__)
namespace detail {
template <class T>
static void
dumpCommentContent(
    T const* C,
    clang::ASTContext const& Ctx,
    llvm::SmallString<1024>& contents)
{
    if (!C)
    {
        return;
    }

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
requires(!std::is_pointer_v<T>)
static void
dumpCommentContent(
    T const& C,
    clang::ASTContext const& Ctx,
    llvm::SmallString<1024>& contents)
{
    dumpCommentContent(&C, Ctx, contents);
}
} // namespace detail

#    define MRDOCS_COMMENT_TRACE(D, Ctx)                                        \
        llvm::SmallString<1024> MRDOCS_COMMENT_TRACE_UNIQUE_NAME;               \
        ::detail::dumpCommentContent(D, Ctx, MRDOCS_COMMENT_TRACE_UNIQUE_NAME); \
        report::trace(                                                          \
            "{}",                                                               \
            std::string_view(MRDOCS_COMMENT_TRACE_UNIQUE_NAME.str()))
#endif

namespace mrdocs {
namespace {

// -------- Small helpers

static std::string
ensureUTF8(std::string s)
{
    if (!llvm::json::isUTF8(s))
    {
        s = llvm::json::fixUTF8(s);
    }
    return s;
}

static doc::InlineKind
convertStyle(clang::comments::InlineCommandRenderKind k)
{
    using K = clang::comments::InlineCommandRenderKind;
    switch (k)
    {
    case K::Monospaced:
        return doc::InlineKind::Code;
    case K::Bold:
        return doc::InlineKind::Strong;
    case K::Emphasized:
        return doc::InlineKind::Emph;
    case K::Normal:
    case K::Anchor:
    default:
        return doc::InlineKind::Text;
    }
}

static doc::ParamDirection
convertDirection(clang::comments::ParamCommandPassDirection d)
{
    using D = clang::comments::ParamCommandPassDirection;
    switch (d)
    {
    case D::In:
        return doc::ParamDirection::in;
    case D::Out:
        return doc::ParamDirection::out;
    case D::InOut:
        return doc::ParamDirection::inout;
    }
    report::error(
        "error: unsupported ParamCommandPassDirection <{}>",
        static_cast<int>(d));
    MRDOCS_UNREACHABLE();
}

static doc::Parts
convertCopydoc(unsigned id)
{
    using T = clang::comments::CommandTraits;
    switch (id)
    {
    case T::KCI_copydoc:
        return doc::Parts::all;
    case T::KCI_copybrief:
        return doc::Parts::brief;
    case T::KCI_copydetails:
        return doc::Parts::description;
    default:
        report::error("error: unsupported CommandTrait id <{}>", id);
        MRDOCS_UNREACHABLE();
    }
}

// Cursor: immutable snapshot of children with index-based look-ahead/consume.
struct Cursor {
    llvm::SmallVector<clang::comments::Comment const*, 8> children;
    std::size_t i{ 0 };

    Cursor() = default;
    explicit Cursor(clang::comments::Comment const* parent)
    {
        children.assign(parent->child_begin(), parent->child_end());
    }

    bool
    done() const
    {
        return i >= children.size();
    }
    clang::comments::Comment const*
    cur() const
    {
        return done() ? nullptr : children[i];
    }
    clang::comments::Comment const*
    peek(std::size_t k = 1) const
    {
        std::size_t j = i + k;
        return (j < children.size()) ? children[j] : nullptr;
    }
    void
    advance()
    {
        if (!done())
        {
            ++i;
        }
    }
    // consume n *intermediate* siblings after current (not including current)
    void
    consume_intermediate(std::size_t n)
    {
        // We will call this only after we've already processed current;
        // It should skip *n* immediately following items.
        i += n;
        if (i > children.size())
        {
            i = children.size();
        }
    }
};

//------------------------------------------------

class DocCommentVisitor
    : public clang::comments::ConstCommentVisitor<DocCommentVisitor> {
    Config const& config_;
    clang::ASTContext const& ctx_;
    clang::SourceManager const& sm_;
    clang::comments::FullComment const* FC_;
    Diagnostics& diags_;

    DocComment jd_;
    doc::InlineContainer* curInlines_{ nullptr };
    bool newline_blocks_merge_{ false };

    // --- inline assembly

    template <std::derived_from<doc::Inline> InlineTy, class... Args>
    void
    emplaceInline(bool end_with_nl, Args&&... args)
    {
        MRDOCS_ASSERT(curInlines_ != nullptr);
        auto& vec = curInlines_->children;

        auto mergeable = [](doc::InlineKind k) {
            return is_one_of(
                k,
                { doc::InlineKind::Text,
                  doc::InlineKind::Emph,
                  doc::InlineKind::Strong,
                  doc::InlineKind::Code });
        };

        InlineTy elem(std::forward<Args>(args)...);

        if (!newline_blocks_merge_ && !vec.empty())
        {
            doc::Inline& last = *vec.back();
            if (last.Kind == elem.Kind && mergeable(elem.Kind))
            {
                if constexpr (std::is_same_v<InlineTy, doc::TextInline>)
                {
                    last.asText().literal.append(elem.asText().literal);
                    newline_blocks_merge_ = end_with_nl;
                    return;
                }
                // For Emph/Strong/Code we preserve node boundaries (safer).
            }
        }

        vec.emplace_back(std::in_place_type<InlineTy>, std::move(elem));
        newline_blocks_merge_ = end_with_nl;
    }

    struct BlockScope {
        DocCommentVisitor& v_;
        doc::InlineContainer* prev_;
        bool prev_merge_;
        BlockScope(DocCommentVisitor& v, doc::InlineContainer* dst)
            : v_(v)
            , prev_(v.curInlines_)
            , prev_merge_(v.newline_blocks_merge_)
        {
            v.curInlines_ = dst;
            v.newline_blocks_merge_ = false;
        }
        ~BlockScope()
        {
            v_.curInlines_ = prev_;
            v_.newline_blocks_merge_ = prev_merge_;
        }
    };

    BlockScope
    enterScope(doc::InlineContainer& dst)
    {
        return BlockScope(*this, &dst);
    }

    // --- diagnostics helpers

    bool
    goodArgCount(std::size_t n, clang::comments::InlineCommandComment const& C)
    {
        if (C.getNumArgs() != n)
        {
            auto loc = sm_.getPresumedLoc(C.getBeginLoc());
            diags_.error(
                std::format(
                    "Expected {} but got {} args\nFile: {}, line {}, col {}\n",
                    n,
                    C.getNumArgs(),
                    files::makePosixStyle(loc.getFilename()),
                    loc.getLine(),
                    loc.getColumn()));
            return false;
        }
        return true;
    }

    // --- “peek next text” & reference fixer using Cursor

    Optional<std::string_view>
    peekNextText(Cursor const& cur) const
    {
        using namespace clang::comments;
        auto* n = cur.peek();
        if (!n || n->getCommentKind() != CommentKind::TextComment)
        {
            return std::nullopt;
        }
        return static_cast<TextComment const*>(n)->getText();
    }

    std::string
    fixReference(std::string& ref, Cursor& cur)
    {
        ParsedRef v;
        for (;;)
        {
            char const* first = ref.data();
            char const* last = first + ref.size();
            auto pres = parse(first, last, v);
            if (!pres)
            {
                if (auto nextText = peekNextText(cur))
                {
                    ref += *nextText;
                    cur.advance(); // consume that sibling
                    continue;
                }
                return {};
            }

            if (pres.ptr != last)
            {
                std::string leftover(pres.ptr, last - pres.ptr);
                if (!isWhitespace(leftover))
                {
                    ref.erase(pres.ptr - first);
                    return leftover;
                }
            }

            bool const mightHaveMoreQualifiers
                = v.HasFunctionParameters && v.ExceptionSpec.Implicit
                  && v.ExceptionSpec.Operand.empty();

            if (mightHaveMoreQualifiers)
            {
                llvm::SmallVector<std::string_view, 4> quals;
                if (v.Kind == ReferenceKind::None)
                {
                    if (!v.IsConst)
                    {
                        quals.push_back("const");
                    }
                    if (!v.IsVolatile)
                    {
                        quals.push_back("volatile");
                    }
                    quals.push_back("&");
                }
                else if (v.Kind == ReferenceKind::LValue && ref.ends_with('&'))
                {
                    quals.push_back("&");
                }
                quals.push_back("noexcept");

                if (auto nextText = peekNextText(cur))
                {
                    auto trimmed = ltrim(*nextText);
                    if (trimmed.empty()
                        || std::ranges::any_of(quals, [&](std::string_view s) {
                        return trimmed.starts_with(s);
                    }))
                    {
                        ref += *nextText;
                        cur.advance();
                        continue;
                    }
                }
                else
                {
                    std::string leftover(pres.ptr, last - pres.ptr);
                    ref.erase(pres.ptr - first);
                    return leftover;
                }
            }

            if (!v.HasFunctionParameters)
            {
                if (auto nextText = peekNextText(cur))
                {
                    std::string_view trimmed = ltrim(*nextText);
                    static constexpr std::string_view idChars
                        = "abcdefghijklmnopqrstuvwxyz"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "0123456789"
                          "_:";
                    if (trimmed.empty() || contains(idChars, trimmed.front()))
                    {
                        ref += *nextText;
                        cur.advance();
                        continue;
                    }
                }
                else
                {
                    std::string leftover(pres.ptr, last - pres.ptr);
                    ref.erase(pres.ptr - first);
                    return leftover;
                }
            }

            return {};
        }
    }

    // --- Visiting using Cursor (no member iterators)

    void
    visitChildrenWithCursor(clang::comments::Comment const* C)
    {
        MRDOCS_COMMENT_TRACE(C, ctx_);
        Cursor cur(C);
        while (!cur.done())
        {
            auto* n = cur.cur();
            MRDOCS_COMMENT_TRACE(n, ctx_);
            visitNode(n, cur); // may advance/consume
            cur.advance();
        }
    }

    // “HTML <span>…</span> text gatherer” equivalent with cursor
    struct TagComponents {
        std::string tag;
        std::string text;
        std::size_t n_intermediate{ 0 };
    };

    Expected<TagComponents>
    parseHTMLStartSpan(
        clang::comments::HTMLStartTagComment const* C,
        Cursor const& cur) const
    {
        TagComponents res;
        res.tag = C->getTagName().str();

        static constexpr std::array noEndTag = {
            "br",    "img",    "input", "hr",      "meta",  "link",
            "base",  "area",   "col",   "command", "embed", "keygen",
            "param", "source", "track", "wbr"
        };
        bool requiresEnd = std::ranges::find(noEndTag, res.tag)
                           == noEndTag.end();
        if (!requiresEnd)
        {
            return res;
        }

        using namespace clang::comments;
        // find matching end tag ahead
        std::size_t j = 1;
        for (; cur.i + j < cur.children.size(); ++j)
        {
            auto* c = cur.children[cur.i + j];
            if (c->getCommentKind() != CommentKind::HTMLEndTagComment)
            {
                continue;
            }
            auto* e = static_cast<HTMLEndTagComment const*>(c);
            if (e->getTagName() == res.tag)
            {
                break;
            }
        }
        if (cur.i + j >= cur.children.size())
        {
            return Unexpected(formatError(
                "warning: HTML <{}> tag not followed by end tag",
                res.tag));
        }

        // ensure all in-between are TextComment
        for (std::size_t k = 1; k < j; ++k)
        {
            if (cur.children[cur.i + k]->getCommentKind()
                != clang::comments::CommentKind::TextComment)
            {
                return Unexpected(Error(
                    std::format(
                        "warning: HTML <{}> tag not followed by pure text",
                        res.tag)));
            }
        }

        res.n_intermediate = j - 1;
        for (std::size_t k = 1; k < j; ++k)
        {
            auto* t = static_cast<clang::comments::TextComment const*>(
                cur.children[cur.i + k]);
            res.text += t->getText();
        }
        return res;
    }

    // Single-dispatch “node” entry that can use/modify the cursor
    void
    visitNode(clang::comments::Comment const* C, Cursor& cur)
    {
        using namespace clang::comments;
        switch (C->getCommentKind())
        {
        case CommentKind::TextComment:
            visitText(static_cast<TextComment const*>(C));
            return;

        case CommentKind::HTMLStartTagComment:
            visitHTMLStart(static_cast<HTMLStartTagComment const*>(C), cur);
            return;

        case CommentKind::HTMLEndTagComment:
            // noop; already handled when start is processed
            return;

        case CommentKind::InlineCommandComment:
            visitInlineCommand(static_cast<InlineCommandComment const*>(C), cur);
            return;

        case CommentKind::ParagraphComment:
            visitParagraph(static_cast<ParagraphComment const*>(C));
            return;

        case CommentKind::BlockCommandComment:
            visitBlockCommand(static_cast<BlockCommandComment const*>(C));
            return;

        case CommentKind::ParamCommandComment:
            visitParam(static_cast<ParamCommandComment const*>(C));
            return;

        case CommentKind::TParamCommandComment:
            visitTParam(static_cast<TParamCommandComment const*>(C));
            return;

        case CommentKind::VerbatimBlockComment:
            visitVerbatimBlock(static_cast<VerbatimBlockComment const*>(C));
            return;

        case CommentKind::VerbatimBlockLineComment:
            visitVerbatimBlockLine(
                static_cast<VerbatimBlockLineComment const*>(C));
            return;

        case CommentKind::VerbatimLineComment:
            // not used
            return;

        default:
            // generic: recurse
            visitChildrenWithCursor(C);
            return;
        }
    }

    // ---- Implementations

    void
    visitText(clang::comments::TextComment const* C)
    {
        MRDOCS_COMMENT_TRACE(C, ctx_);
        llvm::StringRef s = C->getText();
        if (curInlines_ && curInlines_->children.empty())
        {
            s = s.ltrim();
        }
        if (!s.empty())
        {
            emplaceInline<
                doc::TextInline>(C->hasTrailingNewline(), ensureUTF8(s.str()));
        }
    }

    void
    visitHTMLStart(clang::comments::HTMLStartTagComment const* C, Cursor& cur)
    {
        MRDOCS_COMMENT_TRACE(C, ctx_);
        MRDOCS_ASSERT(C->child_begin() == C->child_end());

        auto loc = sm_.getPresumedLoc(C->getBeginLoc());
        auto filename = files::makePosixStyle(loc.getFilename());

        auto getAttr = [&C](llvm::StringRef name) -> Expected<std::string> {
            for (unsigned i = 0, n = C->getNumAttrs(); i < n; ++i)
            {
                auto const& a = C->getAttr(i);
                if (a.Name == name)
                {
                    return std::string(a.Value);
                }
            }
            return Unexpected(Error(
                std::format(
                    "warning: HTML <{}> tag has no {} attribute",
                    C->getTagName().str(),
                    name.str())));
        };

        auto compsExp = parseHTMLStartSpan(C, cur);
        if (!compsExp)
        {
            report::error(
                "{} at {} ({})",
                compsExp.error().message(),
                filename,
                loc.getLine());
            return;
        }
        auto comps = *compsExp;

        if (comps.tag == "a")
        {
            auto r = getAttr("href");
            if (!r)
            {
                report::error(r.error().message());
                return;
            }
            emplaceInline<doc::LinkInline>(
                C->hasTrailingNewline(),
                ensureUTF8(std::move(comps.text)),
                ensureUTF8(std::move(*r)));
        }
        else if (comps.tag == "br")
        {
            emplaceInline<doc::TextInline>(true, "");
        }
        else if (comps.tag == "em")
        {
            emplaceInline<doc::EmphInline>(
                C->hasTrailingNewline(),
                ensureUTF8(std::move(comps.text)));
        }
        else
        {
            report::warn(
                std::format("warning: unsupported HTML tag <{}>", comps.tag),
                filename,
                loc.getLine());
        }

        // Skip the intermediate siblings consumed for text gathering
        cur.consume_intermediate(comps.n_intermediate);
    }

    void
    visitInlineCommand(
        clang::comments::InlineCommandComment const* C,
        Cursor& cur)
    {
        MRDOCS_COMMENT_TRACE(C, ctx_);
        auto const* cmd = ctx_.getCommentCommandTraits().getCommandInfo(
            C->getCommandID());
        MRDOCS_ASSERT(cmd != nullptr);

        using T = clang::comments::CommandTraits;
        switch (unsigned ID = cmd->getID())
        {
        case T::KCI_n:
            if (!goodArgCount(0, *C))
            {
                return;
            }
            emplaceInline<doc::TextInline>(true, "\n");
            return;

        case T::KCI_a:
        case T::KCI_e:
        case T::KCI_em:
        {
            MRDOCS_CHECK_OR(goodArgCount(1, *C));
            emplaceInline<doc::EmphInline>(
                C->hasTrailingNewline(),
                C->getArgText(0).str());
            return;
        }

        case T::KCI_copybrief:
        case T::KCI_copydetails:
        case T::KCI_copydoc:
        {
            MRDOCS_CHECK_OR(goodArgCount(1, *C));
            std::string ref = C->getArgText(0).str();
            std::string leftover = fixReference(ref, cur);
            bool hasExtra = !leftover.empty();

            doc::Parts parts = convertCopydoc(ID);
            bool copyBrief = parts == doc::Parts::brief
                             || parts == doc::Parts::all;
            bool copyDetails = parts == doc::Parts::description
                               || parts == doc::Parts::all;

            if (copyBrief)
            {
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
                emplaceInline<doc::CopyDetailsInline>(
                    C->hasTrailingNewline() && !hasExtra,
                    ref);
            }
            if (hasExtra)
            {
                emplaceInline<doc::TextInline>(
                    C->hasTrailingNewline(),
                    std::move(leftover));
            }
            return;
        }

        case T::KCI_ref:
        {
            MRDOCS_CHECK_OR(goodArgCount(1, *C));
            std::string ref = C->getArgText(0).str();
            std::string leftover = fixReference(ref, cur);
            bool hasExtra = !leftover.empty();
            emplaceInline<
                doc::ReferenceInline>(C->hasTrailingNewline() && !hasExtra, ref);
            if (hasExtra)
            {
                emplaceInline<doc::TextInline>(
                    C->hasTrailingNewline(),
                    std::move(leftover));
            }
            return;
        }

        case T::KCI_related:
        case T::KCI_relates:
        case T::KCI_relatedalso:
        case T::KCI_relatesalso:
        case T::KCI_memberof:
        {
            MRDOCS_CHECK_OR(goodArgCount(1, *C));
            std::string ref = C->getArgText(0).str();
            std::string leftover = fixReference(ref, cur);
            bool hasExtra = !leftover.empty();
            jd_.relates.emplace_back(std::move(ref));
            if (hasExtra)
            {
                emplaceInline<doc::TextInline>(
                    C->hasTrailingNewline(),
                    std::move(leftover));
            }
            return;
        }

        default:
            break;
        }

        // default rendering: concatenate all args and style accordingly
        std::string s;
        s.reserve([&] {
            size_t n = 0;
            for (unsigned i = 0; i < C->getNumArgs(); ++i)
            {
                n += C->getArgText(i).size();
            }
            return n;
        }());
        for (unsigned i = 0; i < C->getNumArgs(); ++i)
        {
            s.append(C->getArgText(i));
        }

        switch (convertStyle(C->getRenderKind()))
        {
        case doc::InlineKind::Emph:
            emplaceInline<
                doc::EmphInline>(C->hasTrailingNewline(), std::move(s));
            break;
        case doc::InlineKind::Strong:
            emplaceInline<
                doc::StrongInline>(C->hasTrailingNewline(), std::move(s));
            break;
        case doc::InlineKind::Code:
            emplaceInline<
                doc::CodeInline>(C->hasTrailingNewline(), std::move(s));
            break;
        default:
            emplaceInline<
                doc::TextInline>(C->hasTrailingNewline(), std::move(s));
            break;
        }
    }

    void
    visitParagraph(clang::comments::ParagraphComment const* C)
    {
        MRDOCS_COMMENT_TRACE(C, ctx_);
        if (curInlines_)
        {
            visitChildrenWithCursor(C);
            return;
        }
        doc::ParagraphBlock paragraph;
        auto scope = enterScope(paragraph);
        visitChildrenWithCursor(C);
        if (!paragraph.empty())
        {
            jd_.Document.emplace_back(std::move(paragraph));
        }
    }

    void
    visitBlockCommand(clang::comments::BlockCommandComment const* C)
    {
        MRDOCS_COMMENT_TRACE(C, ctx_);
        auto const* cmd = ctx_.getCommentCommandTraits().getCommandInfo(
            C->getCommandID());
        if (!cmd)
        {
            return;
        }

        auto parseBlock =
            [this, C]<class BlockTy>(std::in_place_type_t<BlockTy>) -> BlockTy
        requires std::derived_from<BlockTy, doc::Block>
                 && std::derived_from<BlockTy, doc::InlineContainer>
        {
            BlockTy b;
            auto& inlines = static_cast<doc::InlineContainer&>(b);
            auto scope = enterScope(inlines);

            // Paragraph may be null for some block commands; guard it.
            if (auto* P = C->getParagraph())
            {
                visitChildrenWithCursor(P);
            }

            if constexpr (requires { BlockTy::name; })
            {
                if (C->getNumArgs() > 0)
                {
                    b.name = C->getArgText(0).str();
                }
            }
            if (!inlines.children.empty())
            {
                trim(inlines);
            }
            return b;
        };

        using T = clang::comments::CommandTraits;
        switch (cmd->getID())
        {
        case T::KCI_brief:
        case T::KCI_short:
        {
            auto b = parseBlock(std::in_place_type<doc::BriefBlock>);
            jd_.brief.emplace(std::move(b));
            return;
        }
        case T::KCI_return:
        case T::KCI_returns:
        case T::KCI_result:
        {
            auto b = parseBlock(std::in_place_type<doc::ReturnsBlock>);
            jd_.returns.push_back(std::move(b));
            return;
        }
        case T::KCI_throw:
        case T::KCI_throws:
        case T::KCI_exception:
        {
            auto b = parseBlock(std::in_place_type<doc::ThrowsBlock>);
            if (C->getNumArgs() > 0)
            {
                b.exception.literal = C->getArgText(0).str();
            }
            jd_.exceptions.push_back(std::move(b));
            return;
        }
        case T::KCI_note:
        case T::KCI_warning:
        {
            auto p = parseBlock(std::in_place_type<doc::ParagraphBlock>);
            doc::AdmonitionKind k = (cmd->getID() == T::KCI_note) ?
                                        doc::AdmonitionKind::note :
                                        doc::AdmonitionKind::warning;
            doc::AdmonitionBlock adm(k);
            adm.blocks.emplace_back(std::move(p));
            jd_.Document.emplace_back(std::move(adm));
            return;
        }
        case T::KCI_par:
        {
            auto paragraph = parseBlock(
                std::in_place_type<doc::ParagraphBlock>);
            if (C->getNumArgs() > 0)
            {
                jd_.Document.emplace_back(
                    doc::HeadingBlock(C->getArgText(0).str()));
            }

            if (!paragraph.children.empty()
                && paragraph.children.front()->isText())
            {
                if (C->getNumArgs() == 0)
                {
                    std::string text = std::move(
                        paragraph.children.front()->asText().literal);
                    if (auto s = trim(text); s.size() != text.size())
                    {
                        text = s;
                    }
                    jd_.Document.emplace_back(
                        doc::HeadingBlock(std::move(text)));
                    paragraph.children.erase(paragraph.children.begin());
                }
                if (!paragraph.children.empty())
                {
                    jd_.Document.emplace_back(std::move(paragraph));
                }
            }
            return;
        }
        case T::KCI_li:
        {
            if (jd_.Document.empty() || !jd_.Document.back()->isList())
            {
                jd_.Document.emplace_back(doc::ListBlock{});
            }
            auto& list = jd_.Document.back()->asList();
            auto& item = list.items.emplace_back();
            auto p = parseBlock(std::in_place_type<doc::ParagraphBlock>);
            item.blocks.emplace_back(std::move(p));
            return;
        }
        case T::KCI_details:
        {
            auto details = parseBlock(std::in_place_type<doc::ParagraphBlock>);
            jd_.Document.emplace_back(std::move(details));
            return;
        }
        case T::KCI_see:
        {
            auto see = parseBlock(std::in_place_type<doc::SeeBlock>);
            jd_.sees.push_back(std::move(see));
            return;
        }
        case T::KCI_pre:
        {
            auto pre = parseBlock(std::in_place_type<doc::PreconditionBlock>);
            jd_.preconditions.push_back(std::move(pre));
            return;
        }
        case T::KCI_post:
        {
            auto post = parseBlock(std::in_place_type<doc::PostconditionBlock>);
            jd_.postconditions.push_back(std::move(post));
            return;
        }

        // Inline-only kinds here would be a logic error:
        case T::KCI_a:
        case T::KCI_e:
        case T::KCI_em:
        case T::KCI_n:
        case T::KCI_copybrief:
        case T::KCI_copydetails:
        case T::KCI_copydoc:
            report::error(
                "error: inline command {} should be handled elsewhere",
                cmd->Name);
            MRDOCS_UNREACHABLE();

        default:
            // unsupported → ignore
            return;
        }
    }

    void
    visitParam(clang::comments::ParamCommandComment const* C)
    {
        MRDOCS_COMMENT_TRACE(C, ctx_);
        doc::ParamBlock param;
        if (C->hasParamName())
        {
            param.name = ensureUTF8(C->getParamNameAsWritten().str());
        }
        else
        {
            diags_.error("Missing parameter name in @param");
            param.name = "@anon";
        }

        if (C->isDirectionExplicit())
        {
            param.direction = convertDirection(C->getDirection());
        }

        if (auto* P = C->getParagraph())
        {
            auto scope = enterScope(param);
            visitChildrenWithCursor(P);
        }

        // warn on duplicates
        auto it = std::ranges::
            find_if(jd_.Document, [&](Polymorphic<doc::Block> const& b) {
            if (!b->isParam())
            {
                return false;
            }
            auto const* p = dynamic_cast<doc::ParamBlock const*>(
                b.operator->());
            MRDOCS_ASSERT(p != nullptr);
            return p->name == param.name;
        });
        if (it != jd_.Document.end())
        {
            report::warn(
                "{}: Duplicate @param for argument {}",
                C->getBeginLoc().printToString(sm_),
                param.name);
        }

        jd_.params.push_back(std::move(param));
    }

    void
    visitTParam(clang::comments::TParamCommandComment const* C)
    {
        MRDOCS_COMMENT_TRACE(C, ctx_);
        doc::TParamBlock tparam;
        if (C->hasParamName())
        {
            tparam.name = ensureUTF8(C->getParamNameAsWritten().str());
        }
        else
        {
            diags_.error("Missing parameter name in @tparam");
            tparam.name = "@anon";
        }

        if (auto* P = C->getParagraph())
        {
            auto scope = enterScope(tparam);
            visitChildrenWithCursor(P);
        }

        auto it = std::ranges::
            find_if(jd_.Document, [&](Polymorphic<doc::Block> const& b) {
            if (!b->isTParam())
            {
                return false;
            }
            auto const* tp = dynamic_cast<doc::TParamBlock const*>(
                b.operator->());
            MRDOCS_ASSERT(tp != nullptr);
            return tp->name == tparam.name;
        });
        if (it != jd_.Document.end())
        {
            report::warn(
                "{}: Duplicate @tparam for argument {}",
                C->getBeginLoc().printToString(sm_),
                tparam.name);
        }

        jd_.tparams.push_back(std::move(tparam));
    }

    void
    visitVerbatimBlock(clang::comments::VerbatimBlockComment const* C)
    {
        MRDOCS_COMMENT_TRACE(C, ctx_);
        doc::CodeBlock code;
        std::string payload;
        unsigned n = C->getNumLines();
        for (unsigned i = 0; i < n; ++i)
        {
            llvm::StringRef line = C->getText(i);
            payload.append(line.data(), line.size());
            if (i + 1 != n)
            {
                payload.push_back('\n');
            }
        }
        code.literal = std::move(payload);
        jd_.Document.emplace_back(std::move(code));
    }

    void
    visitVerbatimBlockLine(clang::comments::VerbatimBlockLineComment const* C)
    {
        MRDOCS_COMMENT_TRACE(C, ctx_);
        emplaceInline<doc::TextInline>(true, C->getText().str());
    }

public:
    DocCommentVisitor(
        clang::comments::FullComment const* FC,
        clang::Decl const* D,
        Config const& config,
        Diagnostics& diags)
        : config_(config)
        , ctx_(D->getASTContext())
        , sm_(ctx_.getSourceManager())
        , FC_(FC)
        , diags_(diags)
    {}

    DocComment
    build()
    {
        MRDOCS_COMMENT_TRACE(FC_, ctx_);
        visitChildrenWithCursor(FC_);
        return std::move(jd_);
    }
};

//------------------------------------------------

} // namespace

void
initCustomCommentCommands(clang::ASTContext& context)
{
    (void) context;
    // Reserved for future custom commands registration.
}

void
populateDocComment(
    Optional<DocComment>& jd,
    clang::comments::FullComment const* FC,
    clang::Decl const* D,
    Config const& config,
    Diagnostics& diags)
{
    MRDOCS_COMMENT_TRACE(FC, D->getASTContext());
    DocCommentVisitor visitor(FC, D, config, diags);
    auto result = visitor.build();
    if (!result.empty())
    {
        if (!jd)
        {
            jd = std::move(result);
        }
        else if (*jd != result)
        {
            jd->append(std::move(result));
        }
    }
}

} // namespace mrdocs
