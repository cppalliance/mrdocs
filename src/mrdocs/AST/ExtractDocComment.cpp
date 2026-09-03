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
#include "ParseRef.hpp"
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/DocComment/Inline/Parts.hpp>
#include <mrdocs/Support/Container/Algorithm.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Reflection/MergeReflectedType.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/ScopeExit.hpp>
#include <mrdocs/Support/String/String.hpp>
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
#include <cstddef>
#include <format>
#include <mutex>
#include <set>
#include <tuple>
#include <ranges>
#include <string_view>
#include <utility>

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

// -------- Custom doc commands that set a boolean flag

/** Entry for a custom block command that sets a boolean flag
    on DocComment and processes its paragraph content.

    To add a new custom flag command, add an entry to the
    customFlagCommands array below: both registration and
    handling are driven from this single table.
*/
struct CustomFlagCommand
{
    char const* name;
    bool DocComment::* flag;
};

// Compile-time ASCII lowercasing of a PascalCase command name, so the
// FlagCommands.inc entries stay PascalCase (e.g. FunctionObject) while
// the parser matches the lowercase spelling (functionobject).
template <std::size_t N>
struct LoweredName
{
    char value[N]{};
    constexpr LoweredName(char const (&s)[N])
    {
        for (std::size_t i = 0; i < N; ++i)
        {
            char const c = s[i];
            value[i] = (c >= 'A' && c <= 'Z')
                ? static_cast<char>(c - 'A' + 'a')
                : c;
        }
    }
};

template <std::size_t N>
LoweredName(char const (&)[N]) -> LoweredName<N>;

// One static lowercased spelling per flag command.
#define INFO(Name) constexpr LoweredName lowered_##Name{#Name};
#include <mrdocs/Metadata/DocComment/FlagCommands.inc>

// The dispatch table is generated from FlagCommands.inc; each command
// maps to its DocComment::Is<Name> flag. Aliases target the same flag,
// so they are spelled out by hand.
#define INFO(Name) {lowered_##Name.value, &DocComment::Is##Name},
constexpr CustomFlagCommand customFlagCommands[] = {
#include <mrdocs/Metadata/DocComment/FlagCommands.inc>
    // `functionobject` is the canonical spelling; `functor` reads
    // better, so it is offered as an alias for the same flag.
    { "functor", &DocComment::IsFunctionObject },
};

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

    // Construct a cursor that walks a subrange of another cursor's
    // siblings. Used to traverse the content between an HTML start
    // and end tag without disturbing the outer cursor.
    Cursor(Cursor const& src, std::size_t from, std::size_t to)
    {
        children.assign(
            src.children.begin() + from,
            src.children.begin() + to);
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

    // A `\f$...\f$` inline formula is a verbatim block command to Clang, so it
    // splits the surrounding paragraph in two. When that happens we record the
    // source line just past the formula; a paragraph that starts on the next
    // line (no blank line between) is merged back so the formula stays inline.
    unsigned mergeParagraphAfterLine_{ 0 };

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

    /** Report a doc-comment warning once per file, line, and message.

        Every translation unit that includes a header parses its comments
        again, so without this the same finding at the same place would be
        printed once per including translation unit.
    */
    static void
    warnOnce(
        std::string const& filename,
        unsigned const line,
        std::string const& message)
    {
        static std::mutex mutex;
        static std::set<std::tuple<std::string, unsigned, std::string>> seen;
        {
            std::lock_guard<std::mutex> const lock(mutex);
            if (!seen.emplace(filename, line, message).second)
            {
                return;
            }
        }
        report::warn("{} at {} ({})", message, filename, line);
    }

    // Small predicates on Clang comment children.
    static bool
    isStartTagNamed(
        clang::comments::Comment const* c, llvm::StringRef name)
    {
        using namespace clang::comments;
        return c->getCommentKind() == CommentKind::HTMLStartTagComment
               && static_cast<HTMLStartTagComment const*>(c)->getTagName()
                      == name;
    }

    static bool
    isEndTagNamed(
        clang::comments::Comment const* c, llvm::StringRef name)
    {
        using namespace clang::comments;
        return c->getCommentKind() == CommentKind::HTMLEndTagComment
               && static_cast<HTMLEndTagComment const*>(c)->getTagName()
                      == name;
    }

    static bool
    isNonWhitespaceText(clang::comments::Comment const* c)
    {
        using namespace clang::comments;
        return c->getCommentKind() == CommentKind::TextComment
               && !isWhitespace(
                   static_cast<TextComment const*>(c)->getText());
    }

    // Emit a warning attached to the source location of `anchor`.
    void
    warnNear(
        clang::comments::Comment const* anchor, std::string_view msg)
    {
        clang::PresumedLoc const loc
            = sm_.getPresumedLoc(anchor->getBeginLoc());
        report::warn(
            "{} at {} ({})",
            msg,
            files::makePosixStyle(loc.getFilename()),
            loc.getLine());
    }

    // Find the index of the end tag that matches the start tag at `cur.i`,
    // handling nesting of tags with the same name. Returns
    // `cur.children.size()` if no match is found.
    static std::size_t
    findMatchingEndTag(Cursor const& cur, llvm::StringRef tag)
    {
        std::size_t depth = 1;
        for (std::size_t j = cur.i + 1; j < cur.children.size(); ++j)
        {
            clang::comments::Comment const* c = cur.children[j];
            if (isStartTagNamed(c, tag))
            {
                ++depth;
            }
            else if (isEndTagNamed(c, tag) && --depth == 0)
            {
                return j;
            }
        }
        return cur.children.size();
    }

    // --- <td>/<th> ---

    doc::TableCell
    collectCell(Cursor sub)
    {
        doc::TableCell cell;
        BlockScope scope = enterScope(cell);
        while (!sub.done())
        {
            visitNode(sub.cur(), sub);
            sub.advance();
        }
        if (!cell.children.empty())
        {
            trim(cell);
        }
        return cell;
    }

    // On success, `cur.i` is the index of the end tag, so the outer
    // advance moves past it.
    Expected<doc::TableCell>
    parseTableCell(Cursor& cur)
    {
        using namespace clang::comments;
        HTMLStartTagComment const* start
            = static_cast<HTMLStartTagComment const*>(cur.cur());
        llvm::StringRef const tag = start->getTagName();
        std::size_t const endIdx = findMatchingEndTag(cur, tag);
        if (endIdx >= cur.children.size())
        {
            return Unexpected(Error(std::format(
                "warning: HTML <{0}> tag not followed by </{0}>",
                tag.str())));
        }
        doc::TableCell cell = collectCell(Cursor(cur, cur.i + 1, endIdx));
        cur.i = endIdx;
        return cell;
    }

    // --- <tr> ---

    void
    addRowCell(
        Cursor& sub,
        doc::TableRow& row,
        bool& allHeader,
        bool& anyCell,
        clang::comments::HTMLStartTagComment const* s)
    {
        Expected<doc::TableCell> cellExp = parseTableCell(sub);
        if (!cellExp)
        {
            warnNear(s, cellExp.error().message());
            return;
        }
        row.Cells.push_back(std::move(*cellExp));
        anyCell = true;
        if (s->getTagName() != "th")
        {
            allHeader = false;
        }
    }

    void
    addRowTag(
        Cursor& sub,
        doc::TableRow& row,
        bool& allHeader,
        bool& anyCell,
        clang::comments::HTMLStartTagComment const* s)
    {
        llvm::StringRef const tag = s->getTagName();
        if (tag != "th" && tag != "td")
        {
            warnNear(s, std::format(
                "warning: unexpected HTML <{}> inside <tr>",
                tag.str()));
            return;
        }
        addRowCell(sub, row, allHeader, anyCell, s);
    }

    void
    addRowChild(
        Cursor& sub,
        doc::TableRow& row,
        bool& allHeader,
        bool& anyCell)
    {
        using namespace clang::comments;
        Comment const* n = sub.cur();
        if (n->getCommentKind() == CommentKind::HTMLStartTagComment)
        {
            addRowTag(
                sub, row, allHeader, anyCell,
                static_cast<HTMLStartTagComment const*>(n));
        }
        else if (isNonWhitespaceText(n))
        {
            warnNear(n, "warning: stray text inside <tr>");
        }
    }

    doc::TableRow
    collectRow(Cursor sub)
    {
        doc::TableRow row;
        bool allHeader = true;
        bool anyCell = false;
        while (!sub.done())
        {
            addRowChild(sub, row, allHeader, anyCell);
            sub.advance();
        }
        if (anyCell && allHeader)
        {
            row.is_header = true;
        }
        return row;
    }

    // On success, `cur.i` is the index of the end tag, so the outer
    // advance moves past it.
    Expected<doc::TableRow>
    parseTableRow(Cursor& cur)
    {
        using namespace clang::comments;
        HTMLStartTagComment const* start
            = static_cast<HTMLStartTagComment const*>(cur.cur());
        llvm::StringRef const tag = start->getTagName();
        std::size_t const endIdx = findMatchingEndTag(cur, tag);
        if (endIdx >= cur.children.size())
        {
            return Unexpected(Error(std::format(
                "warning: HTML <{0}> tag not followed by </{0}>",
                tag.str())));
        }
        doc::TableRow row = collectRow(Cursor(cur, cur.i + 1, endIdx));
        cur.i = endIdx;
        return row;
    }

    // --- <table> ---

    void
    addTableRow(
        Cursor& sub,
        doc::TableBlock& table,
        clang::comments::HTMLStartTagComment const* s)
    {
        Expected<doc::TableRow> rowExp = parseTableRow(sub);
        if (!rowExp)
        {
            warnNear(s, rowExp.error().message());
            return;
        }
        // Skip rows whose cells all failed to parse: a row with
        // no cells has no semantic content and only bloats output.
        if (rowExp->Cells.empty())
        {
            return;
        }
        table.items.push_back(std::move(*rowExp));
    }

    // Walks an already-sub-ranged cursor collecting <tr> children of
    // <thead>/<tbody>/<tfoot>, warning about anything else.
    void
    collectRowsFromGroup(Cursor sub, doc::TableBlock& table)
    {
        using namespace clang::comments;
        while (!sub.done())
        {
            Comment const* n = sub.cur();
            if (isStartTagNamed(n, "tr"))
            {
                addTableRow(
                    sub, table,
                    static_cast<HTMLStartTagComment const*>(n));
            }
            else if (isNonWhitespaceText(n))
            {
                warnNear(n, "warning: stray text inside <table>");
            }
            sub.advance();
        }
    }

    void
    addTableGroup(
        Cursor& sub,
        doc::TableBlock& table,
        clang::comments::HTMLStartTagComment const* s)
    {
        llvm::StringRef const group = s->getTagName();
        std::size_t const endIdx = findMatchingEndTag(sub, group);
        if (endIdx >= sub.children.size())
        {
            warnNear(s, std::format(
                "warning: HTML <{0}> tag not followed by </{0}>",
                group.str()));
            return;
        }
        collectRowsFromGroup(Cursor(sub, sub.i + 1, endIdx), table);
        sub.i = endIdx;
    }

    void
    addTableTag(
        Cursor& sub,
        doc::TableBlock& table,
        clang::comments::HTMLStartTagComment const* s)
    {
        llvm::StringRef const inner = s->getTagName();
        if (inner == "tr")
        {
            addTableRow(sub, table, s);
            return;
        }
        if (inner == "thead" || inner == "tbody" || inner == "tfoot")
        {
            addTableGroup(sub, table, s);
            return;
        }
        warnNear(s, std::format(
            "warning: unexpected HTML <{}> inside <table>", inner.str()));
    }

    void
    addTableChild(Cursor& sub, doc::TableBlock& table)
    {
        using namespace clang::comments;
        Comment const* n = sub.cur();
        if (n->getCommentKind() == CommentKind::HTMLStartTagComment)
        {
            addTableTag(
                sub, table,
                static_cast<HTMLStartTagComment const*>(n));
        }
        else if (isNonWhitespaceText(n))
        {
            warnNear(n, "warning: stray text inside <table>");
        }
    }

    doc::TableBlock
    collectTable(Cursor sub)
    {
        doc::TableBlock table;
        while (!sub.done())
        {
            addTableChild(sub, table);
            sub.advance();
        }
        return table;
    }

    // On success, `cur.i` is the index of the end tag, so the outer
    // advance moves past it.
    Expected<doc::TableBlock>
    parseTable(Cursor& cur)
    {
        using namespace clang::comments;
        HTMLStartTagComment const* start
            = static_cast<HTMLStartTagComment const*>(cur.cur());
        llvm::StringRef const tag = start->getTagName();
        std::size_t const endIdx = findMatchingEndTag(cur, tag);
        if (endIdx >= cur.children.size())
        {
            return Unexpected(Error(std::format(
                "warning: HTML <{0}> tag not followed by </{0}>",
                tag.str())));
        }
        doc::TableBlock table = collectTable(Cursor(cur, cur.i + 1, endIdx));
        cur.i = endIdx;
        return table;
    }

    // Flush any accumulated inlines into a `ParagraphBlock` and emplace
    // it into the document, clearing the current inline container.
    // Used before emplacing a block that interrupts the current
    // paragraph (e.g. <table>).
    void
    flushCurrentParagraphAsBlock()
    {
        if (!curInlines_ || curInlines_->children.empty())
        {
            return;
        }
        doc::ParagraphBlock pending;
        doc::InlineContainer& pendingInlines
            = static_cast<doc::InlineContainer&>(pending);
        pendingInlines.children = std::move(curInlines_->children);
        curInlines_->children.clear();
        trim(pendingInlines);
        if (!pendingInlines.children.empty())
        {
            jd_.Document.emplace_back(std::move(pending));
        }
    }

    void
    visitHTMLTable(
        clang::comments::HTMLStartTagComment const* C, Cursor& cur)
    {
        Expected<doc::TableBlock> tbExp = parseTable(cur);
        if (!tbExp)
        {
            warnNear(C, tbExp.error().message());
            return;
        }
        // A table with no rows (e.g., because every row's content
        // failed to parse) carries no information. Drop it instead
        // of emplacing an empty <table> into the document.
        if (tbExp->items.empty())
        {
            return;
        }
        flushCurrentParagraphAsBlock();
        jd_.Document.emplace_back(std::move(*tbExp));
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
                    "HTML <{}> tag has no {} attribute",
                    C->getTagName().str(),
                    name.str())));
        };

        // Container tags with nested HTML are handled separately;
        // `parseHTMLStartSpan` can only gather pure text between start/end.
        if (C->getTagName() == "table")
        {
            visitHTMLTable(C, cur);
            return;
        }

        auto compsExp = parseHTMLStartSpan(C, cur);
        if (!compsExp)
        {
            warnOnce(filename, loc.getLine(), compsExp.error().message());
            return;
        }
        auto comps = *compsExp;

        if (comps.tag == "a")
        {
            auto r = getAttr("href");
            if (!r)
            {
                // An <a> without href (an anchor, or malformed markup) is a
                // problem in the user's doc comment, not in Mr.Docs: warn
                // with the location and skip the tag instead of failing the
                // whole run with an internal-error banner.
                warnOnce(filename, loc.getLine(), r.error().message());
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
        else if (comps.tag == "strong")
        {
            emplaceInline<doc::StrongInline>(
                C->hasTrailingNewline(),
                ensureUTF8(std::move(comps.text)));
        }
        else if (comps.tag == "mark")
        {
            emplaceInline<doc::HighlightInline>(
                C->hasTrailingNewline(),
                ensureUTF8(std::move(comps.text)));
        }
        else if (comps.tag == "sub")
        {
            emplaceInline<doc::SubscriptInline>(
                C->hasTrailingNewline(),
                ensureUTF8(std::move(comps.text)));
        }
        else if (comps.tag == "sup")
        {
            emplaceInline<doc::SuperscriptInline>(
                C->hasTrailingNewline(),
                ensureUTF8(std::move(comps.text)));
        }
        else if (comps.tag == "del" || comps.tag == "s")
        {
            emplaceInline<doc::StrikethroughInline>(
                C->hasTrailingNewline(),
                ensureUTF8(std::move(comps.text)));
        }
        else if (comps.tag == "code")
        {
            emplaceInline<doc::CodeInline>(
                C->hasTrailingNewline(),
                ensureUTF8(std::move(comps.text)));
        }
        else if (comps.tag == "img")
        {
            // <img> is a void tag: src/alt come from attributes, not
            // enclosed text. A missing src makes the image meaningless,
            // so warn and drop; a missing alt is allowed (empty alt).
            auto srcAttr = getAttr("src");
            if (!srcAttr)
            {
                warnOnce(filename, loc.getLine(), srcAttr.error().message());
                return;
            }
            std::string alt = getAttr("alt").value_or(std::string());
            emplaceInline<doc::ImageInline>(
                C->hasTrailingNewline(),
                ensureUTF8(std::move(*srcAttr)),
                ensureUTF8(std::move(alt)));
        }
        else
        {
            warnOnce(
                filename, loc.getLine(),
                std::format("warning: unsupported HTML tag <{}>", comps.tag));
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
        // An unrecognized command carries its meaning in the command name,
        // which Clang does not expose as an argument. Dropping it silently
        // loses content: most importantly LaTeX macros inside math spans
        // (e.g. \pi, \epsilon in `$\pi r^2$`), which Clang tokenizes as
        // unknown commands. Preserve the command verbatim as literal text.
        if (cmd->IsUnknownCommand)
        {
            s.push_back('\\');
            s.append(C->getCommandName(ctx_.getCommentCommandTraits()));
        }
        s.reserve(s.size() + [&] {
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
        // Merge back a paragraph split off by a preceding `\f$...\f$` inline
        // formula, but only when it is contiguous (no blank line between).
        unsigned const mergeLine = std::exchange(mergeParagraphAfterLine_, 0);
        if (mergeLine != 0
            && !jd_.Document.empty()
            && jd_.Document.back()->Kind == doc::BlockKind::Paragraph
            && sm_.getPresumedLoc(C->getBeginLoc()).getLine() <= mergeLine + 1)
        {
            auto& inlines = static_cast<doc::InlineContainer&>(
                jd_.Document.back()->asParagraph());
            auto scope = enterScope(inlines);
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
            // Check if it's a custom command we registered (e.g. "tip", "important").
            // These don't have KCI_ constants, since they are registered at runtime via
            // initCustomCommentCommands().
            llvm::StringRef name = cmd->Name;
            if (name == "tip" || name == "important" || name == "caution")
            {
                doc::ParagraphBlock p = parseBlock(std::in_place_type<doc::ParagraphBlock>);
                doc::AdmonitionKind k = name == "tip"
                                        ? doc::AdmonitionKind::tip
                                        : name == "important"
                                        ? doc::AdmonitionKind::important
                                        : doc::AdmonitionKind::caution;
                doc::AdmonitionBlock adm(k);
                adm.blocks.emplace_back(std::move(p));
                jd_.Document.emplace_back(std::move(adm));
                return;
            }

            // Custom flag commands registered at runtime:
            // set the corresponding flag and preserve any
            // paragraph text in the doc comment.
            for (auto const& custom : customFlagCommands)
            {
                if (llvm::StringRef(cmd->Name) == custom.name)
                {
                    jd_.*(custom.flag) = true;
                    auto p = parseBlock(
                        std::in_place_type<doc::ParagraphBlock>);
                    if (!p.children.empty())
                    {
                        jd_.Document.emplace_back(std::move(p));
                    }
                    return;
                }
            }
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

        // Skip a parameter that is already documented. `@param` blocks are
        // stored in `jd_.params` (not `jd_.Document`), and the same parameter
        // can be documented more than once: either twice in a single comment
        // (a genuine authoring mistake) or once on each of several
        // redeclarations of the same symbol whose comments are parsed into the
        // same DocComment (e.g. two overloads MrDocs treats as redeclarations).
        // In both cases the first documentation wins; keeping the duplicate
        // would surface a spurious "Duplicate parameter documentation" later.
        auto const dupIt = std::ranges::find_if(
            jd_.params,
            [&](doc::ParamBlock const& existing)
            {
                return existing.name == param.name;
            });
        if (dupIt != jd_.params.end())
        {
            return;
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

        // Doxygen LaTeX formula commands, which Clang models as verbatim
        // block commands. They give users a math syntax that does not trip
        // Clang's -Wdocumentation-unknown-command (unlike the `$...$` /
        // `$$...$$` Markdown forms, whose bodies Clang cannot parse).
        //   \f$ ... \f$  inline formula
        //   \f[ ... \f]  displayed formula
        auto const name = C->getCommandName(ctx_.getCommentCommandTraits());
        if (name == "f$" || name == "f[")
        {
            payload = std::string(trim(payload));
        }
        if (name == "f$")
        {
            // Inline formula. Clang positions the verbatim block outside the
            // surrounding paragraph, so emit into the current inline flow when
            // one is open, append to the paragraph that was just closed, or
            // otherwise wrap it in its own paragraph.
            if (curInlines_)
            {
                emplaceInline<doc::MathInline>(true, std::move(payload));
                return;
            }
            doc::InlineContainer* inlines = nullptr;
            if (!jd_.Document.empty()
                && jd_.Document.back()->Kind == doc::BlockKind::Paragraph)
            {
                inlines = &static_cast<doc::InlineContainer&>(
                    jd_.Document.back()->asParagraph());
            }
            else
            {
                jd_.Document.emplace_back(doc::ParagraphBlock{});
                inlines = &static_cast<doc::InlineContainer&>(
                    jd_.Document.back()->asParagraph());
            }
            inlines->children.emplace_back(
                std::in_place_type<doc::MathInline>, std::move(payload));
            mergeParagraphAfterLine_
                = sm_.getPresumedLoc(C->getEndLoc()).getLine();
            return;
        }
        if (name == "f[")
        {
            // Displayed formula: always a block.
            flushCurrentParagraphAsBlock();
            doc::MathBlock math;
            math.literal = std::move(payload);
            jd_.Document.emplace_back(std::move(math));
            return;
        }

        // Every non-formula verbatim command is rendered as a fenced code
        // block; the command implies the fence's info (language) string.
        doc::CodeBlock code;
        if (name == "code")
        {
            // Doxygen lets `@code` name a language, spelled `@code{.cpp}`. Clang
            // does not understand the `{.lang}` suffix and keeps it as the first
            // line of the verbatim block, so lift it into the info string and
            // drop it from the rendered body.
            std::size_t const nl = payload.find('\n');
            std::string_view const first = trim(std::string_view(payload).substr(
                0, nl == std::string::npos ? payload.size() : nl));
            if (first.size() > 3 &&
                first.starts_with("{.") &&
                first.ends_with("}"))
            {
                code.info = std::string(first.substr(2, first.size() - 3));
                payload.erase(0, nl == std::string::npos ? payload.size() : nl + 1);
            }
        }
        else if (llvm::StringRef lang = name; lang.consume_back("only"))
        {
            // The output-format commands name their language directly:
            // `@htmlonly` -> html, `@xmlonly` -> xml, `@latexonly` -> latex,
            // `@docbookonly` -> docbook, `@manonly` -> man, `@rtfonly` -> rtf.
            code.info = lang.str();
        }
        else if (name == "dot")
        {
            code.info = "dot";
        }
        else if (name == "msc")
        {
            code.info = "msc";
        }
        else if (name == "startuml")
        {
            code.info = "plantuml";
        }
        // `@verbatim` and the structural blocks (internal, parblock,
        // secreflist) carry no language.
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
        clang::ASTContext const& ctx,
        Config const& config,
        Diagnostics& diags)
        : config_(config)
        , ctx_(ctx)
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
    clang::comments::CommandTraits& traits = context.getCommentCommandTraits();
    traits.registerBlockCommand("tip");
    traits.registerBlockCommand("important");
    traits.registerBlockCommand("caution");
    for (auto const& cmd : customFlagCommands)
    {
        traits.registerBlockCommand(cmd.name);
    }
}

void
populateDocComment(
    Optional<DocComment>& jd,
    clang::comments::FullComment const* FC,
    clang::ASTContext const& ctx,
    Config const& config,
    Diagnostics& diags)
{
    MRDOCS_COMMENT_TRACE(FC, ctx);
    DocCommentVisitor visitor(FC, ctx, config, diags);
    auto result = visitor.build();
    if (!result.empty())
    {
        if (!jd)
        {
            jd = std::move(result);
        }
        else
        {
            // Fill in only the fields this comment is missing rather than
            // appending, so multiple declarations (or namespace reopenings)
            // don't duplicate briefs and descriptions.
            merge(*jd, std::move(result));
        }
    }
}

} // namespace mrdocs
