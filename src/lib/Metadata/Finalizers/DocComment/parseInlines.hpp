//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_DOCCOMMENT_PARSEINLINES_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_DOCCOMMENT_PARSEINLINES_HPP

#include <mrdocs/Metadata/DocComment/Inline.hpp>
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/Parse.hpp>
#include <mrdocs/Support/String.hpp>
#include <utility>

namespace mrdocs::doc {
namespace {
inline
bool
isPunctuation(char c)
{
    // ASCII punctuation per CommonMark (§2.2) (includes '_')
    return is_one_of(c, R"(!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~)");
}

// ========================== RULE TABLE & FLAGS ===============================

/// Flags that configure how a token behaves in the inline parser.
/// Multiple flags may be OR'ed together for a rule.
enum class RuleFlags : unsigned {
    /// No special behavior.
    None           = 0,

    /// The token follows Markdown semantics (e.g., emphasis/strong rules).
    /// Used to apply CommonMark-style delimiter and boundary logic.
    Markdown       = 1u << 0,

    /// The token represents an HTML tag (case-insensitive), allowing optional
    /// whitespace and attributes inside angle brackets (e.g., <em>, <a href="">).
    Html           = 1u << 1,

    /// Apply CommonMark left/right “flanking” checks to decide whether a
    /// delimiter may open and/or close. Crucial for underscores near identifiers.
    RequiresFlank  = 1u << 2,

    /// Allows a closing delimiter to pop through intervening frames until a
    /// matching kind is found (typical Markdown/HTML implicit closing behavior).
    ImplicitClose  = 1u << 3,

    /// Do not scan nested delimiters inside
    Barrier       = 1u << 4,

    /// The token cannot be used intraword (e.g., '^' and '~' for super/subscript).
    /// This is stricter than RequiresFlank.
    NoIntraWord = 1u << 5,
};

/// Bitwise OR operator for RuleFlags.
constexpr
RuleFlags
operator|(RuleFlags a, RuleFlags b)
{
    return static_cast<RuleFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

/// Bitwise AND operator for RuleFlags.
constexpr
bool
has(RuleFlags f, RuleFlags bit)
{
    return (static_cast<unsigned>(f) & static_cast<unsigned>(bit)) != 0;
}

/// Single inline markup rule
struct TagRule {
    /// The kind of inline node this rule produces (e.g., Emph, Strong, Code, etc.)
    InlineKind kind;
    /// The exact string that triggers this rule when opening (e.g., "*", "<em>", "^")
    std::string_view open;
    /// The exact string that closes this rule (may equal `open` for symmetric tokens)
    std::string_view close;
    /// Flags describing behavior (Markdown, HTML, AsciiDoc, boundary rules, etc.)
    RuleFlags flags;
};

// Table of tag rules
// Order matters: try longer tokens first to disambiguate (** before *).
inline
constexpr
TagRule
kRules[] = {
    // Markdown strong/emph/strike/code
    { doc::InlineKind::Strong,       "**", "**", RuleFlags::Markdown | RuleFlags::RequiresFlank | RuleFlags::ImplicitClose },
    { doc::InlineKind::Strong,       "__", "__", RuleFlags::Markdown | RuleFlags::RequiresFlank | RuleFlags::ImplicitClose | RuleFlags::NoIntraWord },
    { doc::InlineKind::Strikethrough,"~~", "~~", RuleFlags::Markdown | RuleFlags::RequiresFlank | RuleFlags::ImplicitClose },
    { doc::InlineKind::Emph,         "*",  "*",  RuleFlags::Markdown | RuleFlags::RequiresFlank | RuleFlags::ImplicitClose },
    { doc::InlineKind::Emph,         "_",  "_",  RuleFlags::Markdown | RuleFlags::RequiresFlank | RuleFlags::ImplicitClose | RuleFlags::NoIntraWord },
    { doc::InlineKind::Code,         "`",  "`",  RuleFlags::Markdown | RuleFlags::ImplicitClose | RuleFlags::Barrier },
    { doc::InlineKind::Math,         "$$", "$$", RuleFlags::Markdown | RuleFlags::ImplicitClose | RuleFlags::Barrier },
    { doc::InlineKind::Math,         "$",  "$",  RuleFlags::Markdown | RuleFlags::ImplicitClose | RuleFlags::Barrier },
    { doc::InlineKind::Superscript,  "^",  "^",  RuleFlags::Markdown | RuleFlags::RequiresFlank | RuleFlags::ImplicitClose | RuleFlags::NoIntraWord },
    { doc::InlineKind::Subscript,    "~",  "~",  RuleFlags::Markdown | RuleFlags::RequiresFlank | RuleFlags::ImplicitClose | RuleFlags::NoIntraWord },
    { doc::InlineKind::Highlight,    "==", "==", RuleFlags::Markdown | RuleFlags::RequiresFlank | RuleFlags::ImplicitClose },

    // HTML tags (case-insensitive). Allow spaces around names/attrs.
    { doc::InlineKind::Emph,         "<em>",         "</em>",         RuleFlags::Html | RuleFlags::ImplicitClose },
    { doc::InlineKind::Strong,       "<strong>",     "</strong>",     RuleFlags::Html | RuleFlags::ImplicitClose },
    { doc::InlineKind::Code,         "<code>",       "</code>",       RuleFlags::Html | RuleFlags::ImplicitClose },
    { doc::InlineKind::Superscript,  "<sup>",        "</sup>",        RuleFlags::Html | RuleFlags::ImplicitClose },
    { doc::InlineKind::Subscript,    "<sub>",        "</sub>",        RuleFlags::Html | RuleFlags::ImplicitClose },
    { doc::InlineKind::Strikethrough,"<del>",        "</del>",        RuleFlags::Html | RuleFlags::ImplicitClose },
    { doc::InlineKind::Highlight,    "<mark>",       "</mark>",       RuleFlags::Html | RuleFlags::ImplicitClose },
    { doc::InlineKind::LineBreak,    "<br>",         "",              RuleFlags::Html }, // also accept <br/> at runtime
};

// Synthetic rule to tag frames as "HTML-ish" so the stack logic can
// consult its flags (ImplicitClose etc.). Needs to be visible to helpers.
inline constexpr TagRule kHtmlRule{
    doc::InlineKind::Text, "", "", RuleFlags::Html | RuleFlags::ImplicitClose
};

// Map supported HTML tag names to InlineKind
inline
std::optional<doc::InlineKind>
html_inline_kind(std::string const& n)
{
    if (n == "em")
    {
        return doc::InlineKind::Emph;
    }
    if (n == "strong")
    {
        return doc::InlineKind::Strong;
    }
    if (n == "code")
    {
        return doc::InlineKind::Code;
    }
    if (n == "sub")
    {
        return doc::InlineKind::Subscript;
    }
    if (n == "sup")
    {
        return doc::InlineKind::Superscript;
    }
    if (n == "del")
    {
        return doc::InlineKind::Strikethrough;
    }
    if (n == "mark")
    {
        return doc::InlineKind::Highlight;
    }
    return std::nullopt;
}


// Find the first rule whose token matches s at i (opening or closing).
template <bool opening>
TagRule const*
matchRuleImpl(std::string_view s, std::size_t i)
{
    for (auto const& r: kRules)
    {
        auto tok = opening ? r.open : r.close;
        if (tok.empty())
        {
            continue;
        }
        if (i + tok.size() > s.size())
        {
            continue;
        }
        if (s.compare(i, tok.size(), tok) == 0)
        {
            return &r;
        }
    }
    return nullptr;
}

inline
TagRule const*
matchOpeningRule(std::string_view s, std::size_t i)
{
    return matchRuleImpl<true>(s, i);
}

inline
TagRule const*
matchClosingRule(std::string_view s, std::size_t i)
{
    return matchRuleImpl<false>(s, i);
}

// ========================== COMMONMARK FLANKING ==============================
//
// left/right-flanking per CM 0.30: this basically disallows opening/closing
// inside words: prev and next both alnum.
//

/// Evaluate left/right flanking at position i for a token of length len.
struct Flank {
    /// The position to the left is left-flanking, meaning the token can open
    bool left  = false;
    /// The position to the right is right-flanking, meaning the token can close
    bool right = false;
};

inline
Flank
flank_at(std::string_view s, std::size_t i, std::size_t len)
{
    const char prev = (i == 0) ? '\0' : s[i - 1];
    const char next = (i + len >= s.size()) ? '\0' : s[i + len];

    const bool prev_space = (i == 0) || isWhitespace(prev);
    const bool next_space = (i + len >= s.size()) || isWhitespace(next);
    const bool prev_punct = (i > 0) && isPunctuation(prev);
    const bool next_punct = (i + len < s.size()) && isPunctuation(next);

    Flank f;
    f.left  = !next_space && (!next_punct || prev_space || prev_punct);
    f.right = !prev_space && (!prev_punct || next_space || next_punct);
    return f;
}

inline
bool
can_open(TagRule const& r, std::string_view s, std::size_t i)
{
    if (!has(r.flags, RuleFlags::RequiresFlank))
    {
        return true;
    }

    std::size_t const len = r.open.size();
    Flank const f = flank_at(s, i, len);

    // Stricter rule: never open intraword
    if (has(r.flags, RuleFlags::NoIntraWord))
    {
        char const prev = (i == 0) ? '\0' : s[i - 1];
        char const next = (i + len >= s.size()) ? '\0' : s[i + len];
        bool const is_intraword = isAlphaNumeric(prev) && isAlphaNumeric(next);
        if (is_intraword)
        {
            return false;
        }
        return f.left;
    }

    // '*', '==', '~~' use standard flanking
    return f.left;
}

inline
bool
can_close(TagRule const& r, std::string_view s, std::size_t i)
{
    if (!has(r.flags, RuleFlags::RequiresFlank))
    {
        return true;
    }

    std::size_t const len = r.close.size();
    Flank const f = flank_at(s, i, len);

    // Stricter rule: never close intraword
    if (has(r.flags, RuleFlags::NoIntraWord))
    {
        char const prev = (i == 0) ? '\0' : s[i - 1];
        char const next = (i + len >= s.size()) ? '\0' : s[i + len];
        bool is_intraword = isAlphaNumeric(prev) && isAlphaNumeric(next);
        if (is_intraword)
        {
            return false;
        }
        return f.right;
    }

    return f.right;
}

// ============================ EMIT/START HELPERS =============================

// Emit text to the inline container, merging with previous text if possible.
inline
void
emit_text(doc::InlineContainer& out, std::string&& text)
{
    if (text.empty())
    {
        return;
    }
    if (!out.empty() && out.back()->isText())
    {
        out.back()->asText().literal += text;
        return;
    }
    out.emplace_back<doc::TextInline>(std::move(text));
}

// Start a new inline container of kind k, appending it to out.
// Returns a reference to the new container for appending children.
inline
doc::InlineContainer&
start_container(doc::InlineContainer& out, doc::InlineKind k)
{
    // Create k and return its child container.
    switch (k)
    {
    case doc::InlineKind::Emph:
        out.emplace_back<doc::EmphInline>();
        break;
    case doc::InlineKind::Strong:
        out.emplace_back<doc::StrongInline>();
        break;
    case doc::InlineKind::Strikethrough:
        out.emplace_back<doc::StrikethroughInline>();
        break;
    case doc::InlineKind::Highlight:
        out.emplace_back<doc::HighlightInline>();
        break;
    case doc::InlineKind::Superscript:
        out.emplace_back<doc::SuperscriptInline>();
        break;
    case doc::InlineKind::Subscript:
        out.emplace_back<doc::SubscriptInline>();
        break;
    case doc::InlineKind::Code:
        out.emplace_back<doc::CodeInline>();
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
    auto* ic = dynamic_cast<doc::InlineContainer*>(&*out.back());
    MRDOCS_ASSERT(ic != nullptr); // container types must derive InlineContainer
    return *ic;
}

// Emit a line break (hard or soft) to the output container.
inline
void
emit_break(doc::InlineContainer& out, bool hard)
{
    if (hard)
    {
        out.emplace_back<doc::LineBreakInline>();
    }
    else
    {
        out.emplace_back<doc::SoftBreakInline>();
    }
}

// Link/Image helpers
inline
doc::InlineContainer&
start_link(doc::InlineContainer& out, std::string&& href) {
    out.emplace_back<doc::LinkInline>();
    auto& link = out.back()->asLink();
    link.href = std::move(href);
    auto* ic = dynamic_cast<doc::InlineContainer*>(&*out.back());
    MRDOCS_ASSERT(ic != nullptr);
    return *ic;
}

inline
doc::InlineContainer&
emit_image(doc::InlineContainer& out, std::string&& src, std::string&& alt)
{
    out.emplace_back<doc::ImageInline>();
    auto& img = out.back()->asImage();
    img.src = std::move(src);
    img.alt = std::move(alt);
    auto* ic = dynamic_cast<doc::InlineContainer*>(&*out.back());
    MRDOCS_ASSERT(ic != nullptr);
    return *ic;
}

// Flatten a temporary InlineContainer to plain text (for alt text fallback).
inline
std::string
flatten_text(doc::InlineContainer const& c)
{
    std::string r;
    for (auto const& el: c.children)
    {
        switch (el->Kind)
        {
        case doc::InlineKind::Text:
            r += el->asText().literal;
            break;
        default: /* ignore formatting for alt */
            break;
        }
    }
    return r;
}

// Move all children from src → dst and clear src
inline
void move_children(doc::InlineContainer& dst, doc::InlineContainer& src)
{
    dst.insert(
        dst.end(),
        std::make_move_iterator(src.begin()),
        std::make_move_iterator(src.end()));
    src.clear();
}

// ================================ FRAMES =====================================

// A Frame represents an opened container collecting children until its
// corresponding close token is encountered.
struct Frame {
    // what we're building
    doc::InlineKind kind;
    // the rule that opened it
    TagRule const* rule;
    // where the finished node will be inserted
    doc::InlineContainer* parent;
    // children collected while open
    doc::InlineContainer scratch;
    // literal open token (for fallback)
    std::string open_tok;
};

// For pending [label] or ![alt]
struct Bracket {
    bool is_image = false;
    // where link/image will be emitted
    doc::InlineContainer* parent = nullptr;
    // label/alt children
    doc::InlineContainer label;
};

// HTML parsing helpers ---------------------------------------------------------

inline
std::size_t
skip_spaces(std::string_view s, std::size_t i)
{
    while (i < s.size() && isWhitespace(s[i]))
    {
        ++i;
    }
    return i;
}

// Parse a case-insensitive HTML tag with optional spaces/attrs.
struct HtmlTag {
    bool closing = false;
    // lowercased name
    std::string name;
    // raw inside <> minus name and /, for simple attr parsing
    std::string attrs;
    // index of char after '>'
    std::size_t end = 0;
};

// Parse an HTML tag at s[i], returning nullopt if not a tag.
// On success, end is the index after '>'.
inline
std::optional<HtmlTag>
parse_html_tag(std::string_view s, std::size_t i)
{
    if (i >= s.size() || s[i] != '<')
    {
        return std::nullopt;
    }

    std::size_t j = i + 1;
    j = skip_spaces(s, j);

    // is it a closing tag?
    bool closing = false;
    if (j < s.size() && s[j] == '/')
    {
        closing = true;
        j = skip_spaces(s, j + 1);
    }

    // tag name
    std::size_t name_start = j;
    while (j < s.size() && isAlphaNumeric(s[j]))
    {
        ++j;
    }
    if (j == name_start)
    {
        return std::nullopt;
    }
    std::string name = toLowerCase(s.substr(name_start, j - name_start));

    // attrs up to '>'
    std::size_t attrs_start = j;
    while (j < s.size() && s[j] != '>') ++j;
    if (j >= s.size()) return std::nullopt;
    // may include spaces and '/'
    std::string attrs(s.substr(attrs_start, j - attrs_start));

    return HtmlTag{closing, std::move(name), std::move(attrs), j + 1};
}

// read attribute value for key (case-insensitive). Accepts key="..."/'...' or key=bare.
inline
std::optional<std::string>
html_get_attr(std::string const& attrs, std::string_view key)
{
    auto k = toLowerCase(key);
    std::size_t i = 0;
    while (i < attrs.size()) {
        while (i < attrs.size() && isWhitespace(attrs[i]))
        {
            ++i;
        }
        std::size_t kstart = i;
        while (
            i < attrs.size()
            && (isAlphaNumeric(attrs[i]) || attrs[i] == '-' || attrs[i] == '_'))
        {
            ++i;
        }
        if (i == kstart)
        {
            break;
        }
        std::string name = toLowerCase(std::string_view(attrs).substr(kstart, i - kstart));
        i = skip_spaces(attrs, i);
        if (i >= attrs.size() || attrs[i] != '=')
        {
            if (name == k)
            {
                return std::string{};
            }
            while (i < attrs.size() && !isWhitespace(attrs[i]))
            {
                ++i;
            }
            continue;
        }
        ++i;
        i = skip_spaces(attrs, i);
        if (i >= attrs.size())
        {
            break;
        }
        std::string value;
        if (attrs[i] == '"' || attrs[i] == '\'') {
            char q = attrs[i++];
            std::size_t vstart = i;
            while (i < attrs.size() && attrs[i] != q)
            {
                ++i;
            }
            value.assign(attrs.substr(vstart, i - vstart));
            if (i < attrs.size())
            {
                ++i;
            }
        }
        else
        {
            std::size_t vstart = i;
            while (
                i < attrs.size() &&
                !isWhitespace(attrs[i]) &&
                attrs[i] != '/')
            {
                ++i;
            }
            value.assign(attrs.substr(vstart, i - vstart));
        }
        if (name == k)
        {
            return value;
        }
    }
    return std::nullopt;
}

// Minimal state holder to replace lots of lambdas in parse()
struct ParserState {
    // input
    std::string_view s;
    // stack of open frames
    std::vector<Frame> frames;
    // stack of open [label] or ![alt]
    std::vector<Bracket> brackets;
    // current container for text accumulation
    doc::InlineContainer* cur{};
    // output
    std::string text;
    // whether the next char is escaped
    bool escape_next{ false };

    void
    flush_text()
    {
        if (!text.empty())
        {
            emit_text(*cur, std::move(text));
            text.clear();
        }
    }

    static void
    push_text_node(doc::InlineContainer& out, std::string_view sv)
    {
        out.emplace_back<doc::TextInline>(std::string(sv));
    }

    void
    push_frame(TagRule const* r)
    {
        flush_text();
        frames.push_back(Frame{ r->kind, r, cur, {}, std::string(r->open) });
        cur = &frames.back().scratch;
    }

    // Emit the opening token + flattened contents as literal text into its
    // parent.
    void
    fallback_unclosed(Frame& f)
    {
        std::string literal(f.open_tok);
        for (auto& el: f.scratch)
        {
            getAsPlainText(*el, literal);
        }
        emit_text(*f.parent, std::move(literal));
    }

    // Materialize a finished frame to its parent
    inline
    void
    materialize_and_pop(Frame f)
    {
        // Line breaks are leaf inlines
        if (f.kind == doc::InlineKind::LineBreak)
        {
            emit_break(*f.parent, /*hard*/ true);
            cur = f.parent;
            return;
        }
        if (f.kind == doc::InlineKind::SoftBreak)
        {
            emit_break(*f.parent, /*hard*/ false);
            cur = f.parent;
            return;
        }

        // Math is a *leaf* (not an InlineContainer): emit a single literal payload.
        if (f.kind == doc::InlineKind::Math)
        {
            // We recorded its body into f.scratch (as Text children) while inside the barrier.
            // Flatten to a single string and emit a MathInline leaf.
            std::string lit = flatten_text(f.scratch);
            f.parent->emplace_back<doc::MathInline>();
            f.parent->back()->asMath().literal = std::move(lit);
            cur = f.parent;
            return;
        }

        // Links opened from HTML <a ...> keep href in open_tok
        if (f.kind == doc::InlineKind::Link && f.rule
            && has(f.rule->flags, RuleFlags::Html))
        {
            doc::InlineContainer& outC = start_link(*f.parent, std::move(f.open_tok));
            move_children(outC, f.scratch);
            cur = f.parent;
            return;
        }

        // All remaining supported formatting kinds are containers
        doc::InlineContainer* outC = &start_container(*f.parent, f.kind);
        move_children(*outC, f.scratch);
        cur = f.parent;
    }

    // Try to close the topmost frame of kind k, popping frames as needed.
    bool
    close_to_kind(doc::InlineKind k)
    {
        std::size_t match = frames.size();
        while (match > 0)
        {
            --match;
            if (frames[match].kind == k)
            {
                break;
            }
            auto* rr = frames[match].rule;
            bool const can_cross = rr
                                   && has(rr->flags, RuleFlags::ImplicitClose)
                                   && !has(rr->flags, RuleFlags::Barrier);
            if (!can_cross)
            {
                return false;
            }
        }
        if (match == frames.size() || frames[match].kind != k)
        {
            return false;
        }

        flush_text();

        // Literalize intervening crossed frames
        while (frames.size() > match + 1)
        {
            Frame f = std::move(frames.back());
            frames.pop_back();
            fallback_unclosed(f);
            cur = f.parent;
        }

        // Materialize the match
        Frame f = std::move(frames.back());
        frames.pop_back();
        materialize_and_pop(std::move(f));
        return true;
    }

    // Markdown link/image finalization at ']'
    std::optional<std::size_t>
    try_close_bracket(std::size_t i)
    {
        if (brackets.empty())
        {
            return std::nullopt;
        }

        flush_text();

        Bracket b = std::move(brackets.back());
        brackets.pop_back();

        // After ']', expect optional spaces then '(' dest [title] ')'
        std::size_t j = i + 1;
        j = skip_spaces(s, j);
        if (j >= s.size() || s[j] != '(')
        {
            // Not a link/image — emit literal "[...]" back to parent
            push_text_node(*b.parent, b.is_image ? "![" : "[");
            if (!b.label.empty())
            {
                move_children(*b.parent, b.label);
            }
            push_text_node(*b.parent, "]");
            cur = b.parent;
            return j - i; // we consumed ']'
        }
        ++j;
        j = skip_spaces(s, j);

        // Parse destination
        std::string dest, title;
        if (j < s.size() && (s[j] == '"' || s[j] == '\''))
        {
            char q = s[j++];
            std::size_t start = j;
            while (j < s.size() && s[j] != q)
            {
                ++j;
            }
            dest.assign(s.substr(start, j - start));
            if (j < s.size())
            {
                ++j;
            }
        }
        else
        {
            std::size_t start = j;
            while (j < s.size() && s[j] != ')' && !isWhitespace(s[j]))
            {
                ++j;
            }
            dest.assign(s.substr(start, j - start));
        }
        j = skip_spaces(s, j);

        // Optional title (ignored, but must be consumed)
        if (j < s.size() && (s[j] == '"' || s[j] == '\''))
        {
            char q = s[j++];
            std::size_t start = j;
            while (j < s.size() && s[j] != q)
            {
                ++j;
            }
            title.assign(s.substr(start, j - start));
            if (j < s.size())
            {
                ++j;
            }
            j = skip_spaces(s, j);
        }

        if (j >= s.size() || s[j] != ')')
        {
            // Invalid link — degrade to literal
            push_text_node(*b.parent, b.is_image ? "![" : "[");
            if (!b.label.empty())
            {
                move_children(*b.parent, b.label);
            }
            push_text_node(*b.parent, "]");
            cur = b.parent;
            return j - i;
        }
        ++j; // consume ')'

        // Materialize
        if (b.is_image)
        {
            emit_image(*b.parent, std::move(dest), flatten_text(b.label));
        }
        else
        {
            doc::InlineContainer& linkC = start_link(*b.parent, std::move(dest));
            move_children(linkC, b.label);
        }
        cur = b.parent;
        return j - i;
    }
};


}


/*  Parse the inline content of a text

    The furthest clang goes is
    clang::comments::TextComment, which we parse
    as doc::TextInline. However, these still contain
    javadoc, HTML, and markdown-like inline elements
    that we want to parse and to represent in the corpus,
    as they are supported by doxygen.

    This parsing happens in a post-processing finalizer step
    because the javadoc parser needs to concatenate
    text nodes in multiple forms and we won't
    have the tidied up text until after the
    post-processing.

    This is a focused implementation; it does not implement every last
    corner of CommonMark, but only the features that exist in Doxygen.
    It should implement the critical delimiter-run algorithm accurately.

    What this parsing function implements:

    - CommonMark-correct emphasis (* and _) including underscore
      intraword rules and the “multiple-of-3” restriction.
    - Backtick code spans per CommonMark (no parsing inside; we still emit a
      container Code node with a single Text child).
    - HTML tags with optional spacing and attributes; support <em>, <strong>,
      <code>, <sup>, <sub>, <del>, <mark>, <br>, <a>, <img>.
    - Markdown links/images: [text](dest "title") and ![alt](src "title").
    - Nesting via a container stack; text nodes are terminal.
    - Compact rule table for non-HTML markers.
    - No <cctype>; all checks are explicit ASCII.
    - Clear, rationale-heavy comments.
 */
inline
ParseResult
parse(char const* first, char const* last, doc::InlineContainer& out_root)
{
    std::string_view s(first, static_cast<std::size_t>(last - first));

    ParserState st{
        .s = s,
        .frames = {},
        .brackets = {},
        .cur = &out_root,
        .text = {},
        .escape_next = false
    };
    st.frames.reserve(8);
    st.brackets.reserve(4);
    st.text.reserve(64);

    for (std::size_t i = 0; i < s.size();) {
        // If inside a barrier (e.g. backticks), only look for its own closer
        if (!st.frames.empty() &&
            st.frames.back().rule &&
            has(st.frames.back().rule->flags, RuleFlags::Barrier))
        {
            TagRule const* br = st.frames.back().rule;
            if (i + br->close.size() <= s.size() &&
                s.compare(i, br->close.size(), br->close) == 0)
            {
                st.flush_text();
                Frame f = std::move(st.frames.back());
                st.frames.pop_back();
                st.materialize_and_pop(std::move(f));
                i += br->close.size();
                continue;
            }
            st.text.push_back(s[i++]); // literal inside code span
            continue;
        }

        char c = s[i];

        // Backslash escape
        if (st.escape_next) { st.text.push_back(c); st.escape_next = false; ++i; continue; }
        if (c == '\\')      { st.escape_next = true; ++i; continue; }

        // Markdown link/image openers
        if (c == '!' && i + 1 < s.size() && s[i + 1] == '[') {
            st.flush_text();
            st.brackets.push_back(Bracket{ true, st.cur, {} });
            st.cur = &st.brackets.back().label;
            i += 2;
            continue;
        }
        if (c == '[') {
            st.flush_text();
            st.brackets.push_back(Bracket{ false, st.cur, {} });
            st.cur = &st.brackets.back().label;
            ++i;
            continue;
        }
        if (c == ']') {
            if (auto adv = st.try_close_bracket(i)) { i += *adv; continue; }
            st.text.push_back(c);
            ++i;
            continue;
        }

        // HTML tags (with spaces, attrs, case-insensitive)
        if (c == '<') {
            if (auto tag = parse_html_tag(s, i)) {
                std::string const& name = tag->name;

                // <br> and <br/>
                if (!tag->closing && name == "br") {
                    st.flush_text();
                    emit_break(*st.cur, /*hard*/true);
                    i = tag->end;
                    continue;
                }

                // <img ...>
                if (!tag->closing && name == "img") {
                    st.flush_text();
                    auto src = html_get_attr(tag->attrs, "src").value_or(std::string{});
                    auto alt = html_get_attr(tag->attrs, "alt").value_or(std::string{});
                    emit_image(*st.cur, std::move(src), std::move(alt));
                    i = tag->end;
                    continue;
                }

                // <a ...> / </a>
                if (name == "a") {
                    if (tag->closing) {
                        if (st.close_to_kind(doc::InlineKind::Link)) {
                            i = tag->end;
                            continue;
                        }
                    } else {
                        auto href = html_get_attr(tag->attrs, "href").value_or(std::string{});
                        st.flush_text();
                        st.frames.push_back(Frame{ doc::InlineKind::Link, &kHtmlRule, st.cur, {}, std::string{} });
                        st.cur = &st.frames.back().scratch;
                        st.frames.back().open_tok = std::move(href); // own the href bytes
                    }
                    i = tag->end;
                    continue;
                }

                // Other supported phrasing tags
                if (auto kind = html_inline_kind(name)) {
                    if (tag->closing) {
                        if (st.close_to_kind(*kind)) {
                            i = tag->end;
                            continue;
                        }
                    } else {
                        st.flush_text();
                        st.frames.push_back(Frame{ *kind, &kHtmlRule, st.cur, {}, "<tag>" });
                        st.cur = &st.frames.back().scratch;
                    }
                    i = tag->end;
                    continue;
                }
                // Unknown <foo>… — fall through to literal
            }
        }

        // Try a closer first (for symmetric tokens)
        if (TagRule const* rc = matchClosingRule(s, i)) {
            const bool ok_close =
                !has(rc->flags, RuleFlags::Markdown) || can_close(*rc, s, i);
            if (ok_close && st.close_to_kind(rc->kind)) {
                i += rc->close.size();
                continue;
            }
        }

        // Then an opener
        if (TagRule const* ro = matchOpeningRule(s, i)) {
            if (has(ro->flags, RuleFlags::Markdown) && !can_open(*ro, s, i)) {
                st.text.append(ro->open);
                i += ro->open.size();
                continue;
            }
            st.push_frame(ro);
            i += ro->open.size();
            continue;
        }

        // Plain char
        st.text.push_back(c);
        ++i;
    }

    // EOF: flush and unwind
    st.flush_text();

    // Unclosed brackets → literal "[...]" back to parent
    while (!st.brackets.empty()) {
        Bracket b = std::move(st.brackets.back());
        st.brackets.pop_back();
        ParserState::push_text_node(*b.parent, b.is_image ? "![" : "[");
        if (!b.label.empty())
            move_children(*b.parent, b.label);
        ParserState::push_text_node(*b.parent, "]");
        st.cur = b.parent;
    }

    // Unclosed frames: HTML autoclosed, Markdown literalized
    st.flush_text();
    while (!st.frames.empty()) {
        Frame f = std::move(st.frames.back());
        st.frames.pop_back();
        if (f.rule && has(f.rule->flags, RuleFlags::Html)) {
            st.materialize_and_pop(std::move(f));
        } else {
            st.fallback_unclosed(f);
            st.cur = f.parent;
        }
    }

    ParseResult res{};
    res.ptr = last;
    return res;
}

} // mrdocs::doc

#endif // MRDOCS_LIB_METADATA_FINALIZERS_DOCCOMMENT_PARSEINLINES_HPP
