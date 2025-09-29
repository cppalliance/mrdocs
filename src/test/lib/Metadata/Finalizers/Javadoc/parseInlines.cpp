//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Metadata/Finalizers/DocComment/parseInlines.hpp>
#include <test_suite/test_suite.hpp>
#include <cassert>
#include <string>
#include <string_view>

namespace mrdocs {
namespace {

using namespace doc;

// ---------- helpers to drive the public parser and assert results

std::string
dumpInline(Inline const& n);

// Dump all children of a container, concatenated
std::string
dumpContainer(InlineContainer const& c)
{
    std::string r;
    for (auto const& p: c.children)
    {
        r += dumpInline(*p);
    }
    return r;
}

std::string
esc(std::string const& s)
{
    std::string out;
    out.reserve(s.size());
    for (char ch: s)
    {
        if (ch == '\\')
        {
            out += "\\\\";
        }
        else if (ch == '"')
        {
            out += "\\\"";
        }
        else
        {
            out += ch;
        }
    }
    return out;
}

// Dump a single Inline node as a string with its type and key data
std::string
dumpInline(Inline const& n)
{
    switch (n.Kind)
    {
    case InlineKind::Text:
        return "T(\"" + esc(n.asText().literal) + "\")";
    case InlineKind::Emph:
        return "Em{" + dumpContainer(n.asEmph()) + "}";
    case InlineKind::Strong:
        return "Str{" + dumpContainer(n.asStrong()) + "}";
    case InlineKind::Strikethrough:
        return "Del{" + dumpContainer(n.asStrikethrough()) + "}";
    case InlineKind::Highlight:
        return "Mark{" + dumpContainer(n.asHighlight()) + "}";
    case InlineKind::Superscript:
        return "Sup{" + dumpContainer(n.asSuperscript()) + "}";
    case InlineKind::Subscript:
        return "Sub{" + dumpContainer(n.asSubscript()) + "}";
    case InlineKind::Code:
        return "Code{" + dumpContainer(n.asCode()) + "}";
    case InlineKind::LineBreak:
        return "BR";
    case InlineKind::SoftBreak:
        return "SBR";
    case InlineKind::Link:
        return "A(href=\"" + esc(n.asLink().href) + "\"){"
               + dumpContainer(n.asLink()) + "}";
    case InlineKind::Image:
        return "IMG(src=\"" + esc(n.asImage().src) + "\",alt=\""
               + esc(n.asImage().alt) + "\")";
    default:
        // If new kinds appear, fail loudly so tests get updated
        assert(false && "Unhandled InlineKind in dumpInline");
        return {};
    }
}

std::string
parse_to_dump(std::string_view in)
{
    InlineContainer root;
    auto res = parse(in.data(), in.data() + in.size(), root);
    (void) res;
    return dumpContainer(root);
}

void
expect_dump(std::string_view input, std::string_view expect)
{
    auto got = parse_to_dump(input);
    BOOST_TEST_EQ(expect, got);
}

} // namespace

// ---------------------------------- Tests ------------------------------------

struct parseInlinesTest {
    void
    test_plain_and_merge()
    {
        // Plain text + merge of consecutive text nodes
        // Adjacent text from literal degradation should merge too.
        // This is exercised later.
        expect_dump("hello world", R"(T("hello world"))");
    }

    void
    test_escaping()
    {
        // Backslash escapes next char
        expect_dump(R"(foo\*bar)", R"(T("foo*bar"))");
        // Two backslashes before special -> one escapes the other
        expect_dump(R"(foo\\*bar)", R"(T("foo\\*bar"))");
        expect_dump(R"(*foo)", R"(T("*foo"))");
        expect_dump(R"(__x)", R"(T("__x"))");
        expect_dump(R"(~~x)", R"(T("~~x"))");
    }

    void
    test_markdown_basic()
    {
        expect_dump("*em*", R"(Em{T("em")})");
        expect_dump("**strong**", R"(Str{T("strong")})");
        expect_dump("~~strike~~", R"(Del{T("strike")})");
        expect_dump("==mark==", R"(Mark{T("mark")})");
        expect_dump("^sup^", R"(Sup{T("sup")})");
        expect_dump("~sub~", R"(Sub{T("sub")})");
        expect_dump("`code`", R"(Code{T("code")})");

        // mixed and interleaved with text
        expect_dump("pre *em* mid **st** end",
                    R"(T("pre ")Em{T("em")}T(" mid ")Str{T("st")}T(" end"))");
        expect_dump("a ~~del~~ b ==mark== c ^sup^ d ~sub~ e",
                    R"(T("a ")Del{T("del")}T(" b ")Mark{T("mark")}T(" c ")Sup{T("sup")}T(" d ")Sub{T("sub")}T(" e"))");

        // nested inline elements
        expect_dump("**a *b* c**",
                    R"(Str{T("a ")Em{T("b")}T(" c")})");
        expect_dump("*em **strong** em*",
                    R"(Em{T("em ")Str{T("strong")}T(" em")})");
        expect_dump("~~x ==y== z~~",
                    R"(Del{T("x ")Mark{T("y")}T(" z")})");
        expect_dump("**x ~y~ z**",
                    R"(Str{T("x ")Sub{T("y")}T(" z")})");
        expect_dump("==m ^s^ n==",
                    R"(Mark{T("m ")Sup{T("s")}T(" n")})");

        // nested with a barrier inside (markdown code should not parse inner markup)
        expect_dump("**a `b * c` d**",
                    R"(Str{T("a ")Code{T("b * c")}T(" d")})");
    }

    void
    test_flanking_intraword_rules()
    {
        // '_' and '__' are NoIntraWord; shouldn't trigger inside identifiers
        expect_dump("foo_bar_baz", R"(T("foo_bar_baz"))");
        expect_dump("foo__bar__baz", R"(T("foo__bar__baz"))");

        // '*' is allowed intraword; still needs flanking
        expect_dump("foo*bar*", R"(T("foo")Em{T("bar")})");

        // Leading/trailing spaces affect flank
        // not left-flanking (space after token)
        expect_dump("* a*", R"(T("* a*"))");
        // not right-flanking (space before close)
        expect_dump("*a *", R"(T("*a *"))");
    }

    void
    test_barrier_code_span()
    {
        // Everything inside backticks is literal text children of Code
        expect_dump("`a*b_[c]`", R"(Code{T("a*b_[c]")})");

        // Ensure barrier close path: multiple text pushes until matching '`'
        expect_dump("x`y`z", R"(T("x")Code{T("y")}T("z"))");

        // Unclosed barrier at EOF -> literal fallback of opening + contents
        expect_dump("pre `code", R"(T("pre `code"))");
    }

    void
    test_html_phrasing_and_breaks()
    {
        // Simple phrasing tags open/close
        expect_dump("<em>x</em>", R"(Em{T("x")})");
        expect_dump("<strong>x</strong>", R"(Str{T("x")})");
        expect_dump("<code>x</code>", R"(Code{T("x")})");
        expect_dump("<sub>x</sub>", R"(Sub{T("x")})");
        expect_dump("<sup>x</sup>", R"(Sup{T("x")})");
        expect_dump("<del>x</del>", R"(Del{T("x")})");
        expect_dump("<mark>x</mark>", R"(Mark{T("x")})");

        // <br> becomes hard break
        expect_dump("a<br>b", R"(T("a")BRT("b"))");

        // Unknown tag -> literal
        expect_dump("<foo>bar</foo>", R"(T("<foo>bar</foo>"))");
    }

    void
    test_html_a_img_attrs()
    {
        // <a href>…</a> with content
        expect_dump("<a href=\"/x\">y</a>", R"(A(href="/x"){T("y")})");

        // Attribute permutations and spaces
        expect_dump(
            "<a    href='/p?q=1'   >t</a>",
            R"(A(href="/p?q=1"){T("t")})");

        // <img> with src/alt, self-contained
        expect_dump(
            R"(<img src="/i.png" alt="pic">)",
            R"(IMG(src="/i.png",alt="pic"))");

        // Missing attributes tolerated → empty strings
        expect_dump("<img>", R"(IMG(src="",alt=""))");
    }

    void
    test_markdown_links_and_images()
    {
        // Link: [label](dest)
        expect_dump("[x](y)", R"(A(href="y"){T("x")})");

        // Quoted destination and title (title ignored in current model, but
        // path still consumed)
        expect_dump(R"([x]("/p?q" "t"))", R"(A(href="/p?q"){T("x")})");

        // Image: ![alt](src)
        expect_dump("![pic](/i.png)", R"(IMG(src="/i.png",alt="pic"))");

        // Invalid: missing closing paren → degrade to literal "[label]"
        expect_dump("[x](y", R"(T("[")T("x")T("]"))");

        // Unmatched '[' at EOF → literal
        expect_dump("[unclosed", R"(T("[")T("unclosed")T("]"))");

        // Unmatched '!' '[' at EOF → literal
        expect_dump("![alt", R"(T("![")T("alt")T("]"))");
    }

    void
    test_implicit_close_crossing()
    {
        // Strong opened, then emph opened, emph closes, then strong closes
        // (proper nesting)
        expect_dump("**a *b* c**", R"(Str{T("a ")Em{T("b")}T(" c")})");

        // Crossing with implicit-close allowed: ~~ can close past * if needed
        // '*' left unclosed → literal later
        expect_dump(
            "~~a *b~~ c*",
            R"(Del{T("a *b")}T(" c*"))");
    }

    void
    test_closing_without_open()
    {
        // Lone closing tokens should be literal text because close_to_kind fails
        // first '**' treated as literal, then '*' parsed
        expect_dump("**a*b", R"(T("**a*b"))");
        expect_dump(") stray", R"(T(") stray"))");
    }

    void
    test_unclosed_frames_fallback_literal()
    {
        // Open emph but never close → literal '*' + contents
        expect_dump("*x", R"(T("*x"))");

        // Open HTML <em> but never close → literal open + contents
        // open_tok for HTML containers is "<tag>"
        expect_dump("<em>x", R"(Em{T("x")})");
    }

    void
    run()
    {
        test_plain_and_merge();
        test_escaping();
        test_markdown_basic();
        test_flanking_intraword_rules();
        test_barrier_code_span();
        test_html_phrasing_and_breaks();
        test_html_a_img_attrs();
        test_markdown_links_and_images();
        test_implicit_close_crossing();
        test_closing_without_open();
        test_unclosed_frames_fallback_literal();
    }
};

TEST_SUITE(parseInlinesTest, "clang.mrdocs.Metadata.Finalizers.Javadoc.parseInlines");

} // namespace mrdocs
