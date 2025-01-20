//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/Glob.hpp>
#include <test_suite/test_suite.hpp>

namespace clang::mrdocs {

struct Glob_test
{
    void
    run()
    {
        // empty
        {
            // default constructed
            {
                PathGlobPattern glob;
                BOOST_TEST(glob.pattern().empty());
                BOOST_TEST(glob.match(""));
                BOOST_TEST_NOT(glob.match("a"));
            }

            // empty string
            {
                auto globExp = PathGlobPattern::create("");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.pattern().empty());
            }
        }

        // literal
        {
            auto globExp = PathGlobPattern::create("abc");
            BOOST_TEST(globExp);
            PathGlobPattern const& glob = *globExp;
            BOOST_TEST_NOT(glob.pattern().empty());
            BOOST_TEST(glob.match("abc"));
            BOOST_TEST_NOT(glob.match("abcd"));
            BOOST_TEST_NOT(glob.match("a/b/c"));
        }

        // "*"
        {
            // surrounded "*"
            {
                // Path
                {
                    auto globExp = PathGlobPattern::create("abc*ghi");
                    BOOST_TEST(globExp);
                    PathGlobPattern const& glob = *globExp;
                    BOOST_TEST(glob.match("abcdefghi"));
                    BOOST_TEST(glob.match("abcdefghghi"));
                    BOOST_TEST_NOT(glob.match("abcdefg/ghi"));
                    BOOST_TEST(glob.match("abcdefg::ghi"));
                }

                // Symbol
                {
                    auto globExp = SymbolGlobPattern::create("abc*ghi");
                    BOOST_TEST(globExp);
                    SymbolGlobPattern const& glob = *globExp;
                    BOOST_TEST(glob.match("abcdefghi"));
                    BOOST_TEST(glob.match("abcdefghghi"));
                    BOOST_TEST(glob.match("abcdefg/ghi"));
                    BOOST_TEST_NOT(glob.match("abcdefg::ghi"));
                }
            }

            // single "*"
            {
                // Path
                {
                    auto globExp = PathGlobPattern::create("*");
                    BOOST_TEST(globExp);
                    PathGlobPattern const& glob = *globExp;
                    BOOST_TEST(glob.match(""));
                    BOOST_TEST(glob.match("abc"));
                    BOOST_TEST_NOT(glob.match("a/b/c"));
                    BOOST_TEST(glob.match("a::b::c"));
                }

                // Symbol
                {
                    auto globExp = SymbolGlobPattern::create("*");
                    BOOST_TEST(globExp);
                    SymbolGlobPattern const& glob = *globExp;
                    BOOST_TEST(glob.match(""));
                    BOOST_TEST(glob.match("abc"));
                    BOOST_TEST(glob.match("a/b/c"));
                    BOOST_TEST_NOT(glob.match("a::b::c"));
                }
            }

            // multiple "*"
            {
                // Path
                {
                    auto globExp = PathGlobPattern::create("a*b*c");
                    BOOST_TEST(globExp);
                    PathGlobPattern const& glob = *globExp;
                    BOOST_TEST(glob.match("abc"));
                    BOOST_TEST_NOT(glob.match("a/b/c"));
                    BOOST_TEST(glob.match("a::b::c"));
                    BOOST_TEST(glob.match("aabbc"));
                }

                // Symbol
                {
                    auto globExp = SymbolGlobPattern::create("a*b*c");
                    BOOST_TEST(globExp);
                    SymbolGlobPattern const& glob = *globExp;
                    BOOST_TEST(glob.match("abc"));
                    BOOST_TEST(glob.match("a/b/c"));
                    BOOST_TEST_NOT(glob.match("a::b::c"));
                    BOOST_TEST(glob.match("aabbc"));
                }
            }

            // escaped "*"
            {
                auto globExp = PathGlobPattern::create("a\\*b");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("a*b"));
                BOOST_TEST_NOT(glob.match("aab"));
            }
        }


        // "**"
        {
            // surrounded "**"
            {
                auto globExp = PathGlobPattern::create("abc**ghi");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abcdefghi"));
                BOOST_TEST(glob.match("abcdefghghi"));
                BOOST_TEST(glob.match("abcdefg/ghi"));
            }

            // single "**"
            {
                auto globExp = PathGlobPattern::create("**");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match(""));
                BOOST_TEST(glob.match("abc"));
                BOOST_TEST(glob.match("a/b/c"));
            }
        }

        // single "?"
        {
            auto globExp = PathGlobPattern::create("a?c");
            BOOST_TEST(globExp);
            PathGlobPattern const& glob = *globExp;
            BOOST_TEST(glob.match("abc"));
            BOOST_TEST(glob.match("a/c"));
        }

        // character sets "[...]"
        {
            // charset single char [<char>]
            {
                auto globExp = PathGlobPattern::create("a[b]c");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abc"));
                BOOST_TEST_NOT(glob.match("acc"));
            }

            // charset two chars [<chars>]
            {
                auto globExp = PathGlobPattern::create("a[bc]d");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abd"));
                BOOST_TEST(glob.match("acd"));
                BOOST_TEST_NOT(glob.match("aad"));
            }

            // charset multiple chars [<chars>]
            {
                auto globExp = PathGlobPattern::create("a[bcdef]g");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abg"));
                BOOST_TEST(glob.match("acg"));
                BOOST_TEST(glob.match("adg"));
                BOOST_TEST(glob.match("aeg"));
                BOOST_TEST(glob.match("afg"));
                BOOST_TEST_NOT(glob.match("agg"));
            }

            // single char range <start>-<end>
            {
                auto globExp = PathGlobPattern::create("a[b-d]e");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abe"));
                BOOST_TEST(glob.match("ace"));
                BOOST_TEST(glob.match("ade"));
                BOOST_TEST_NOT(glob.match("aae"));
            }

            // double char range [<start>-<end><start>-<end>]
            {
                auto globExp = PathGlobPattern::create("a[b-df-h]g");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abg"));
                BOOST_TEST(glob.match("acg"));
                BOOST_TEST(glob.match("adg"));
                BOOST_TEST_NOT(glob.match("aeg"));
                BOOST_TEST(glob.match("afg"));
                BOOST_TEST(glob.match("agg"));
                BOOST_TEST(glob.match("ahg"));
                BOOST_TEST_NOT(glob.match("aig"));
            }

            // escaped range
            {
                // escaping with backslash
                {
                    auto globExp = PathGlobPattern::create("a\\[b]c");
                    BOOST_TEST(globExp);
                    PathGlobPattern const& glob = *globExp;
                    BOOST_TEST(glob.match("a[b]c"));
                }

                // escaping with set containing only "["
                {
                    auto globExp = PathGlobPattern::create("a[[]b]c");
                    BOOST_TEST(globExp);
                    PathGlobPattern const& glob = *globExp;
                    BOOST_TEST(glob.match("a[b]c"));
                }
            }

            // escaped empty range
            {
                // escaping with backslash
                {
                    auto globExp = PathGlobPattern::create("a\\[]b");
                    BOOST_TEST(globExp);
                    PathGlobPattern const& glob = *globExp;
                    BOOST_TEST(glob.match("a[]b"));
                }

                // escaping with set containing only "["
                {
                    auto globExp = PathGlobPattern::create("a[[]]b");
                    BOOST_TEST(globExp);
                    PathGlobPattern const& glob = *globExp;
                    BOOST_TEST(glob.match("a[]b"));
                }
            }

            // - at the end as part of the set
            {
                auto globExp = PathGlobPattern::create("a[b-]c");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abc"));
                BOOST_TEST(glob.match("a-c"));
                BOOST_TEST_NOT(glob.match("a]c"));
            }

            // - at the beginning as part of the set
            {
                auto globExp = PathGlobPattern::create("a[-b]c");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abc"));
                BOOST_TEST(glob.match("a-c"));
                BOOST_TEST_NOT(glob.match("a]c"));
            }

            // range with surrounding set
            {
                auto globExp = PathGlobPattern::create("a[bc-de]f");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abf"));
                BOOST_TEST(glob.match("acf"));
                BOOST_TEST(glob.match("adf"));
                BOOST_TEST(glob.match("aef"));
                BOOST_TEST_NOT(glob.match("aff"));
            }

            // invalid cases
            {
                // empty range
                BOOST_TEST_NOT(PathGlobPattern::create("a[]b"));
                // unmatched '['
                BOOST_TEST_NOT(PathGlobPattern::create("a[b"));
                // range with end lower than start
                BOOST_TEST_NOT(PathGlobPattern::create("a[b-a]c"));
            }
        }

        // negated charset
        {
            // negated with ^
            {
                auto globExp = PathGlobPattern::create("a[^bc]d");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("aad"));
                BOOST_TEST(glob.match("aed"));
                BOOST_TEST_NOT(glob.match("abd"));
                BOOST_TEST_NOT(glob.match("acd"));
            }

            // negated with !
            {
                auto globExp = PathGlobPattern::create("a[!bc]d");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("aad"));
                BOOST_TEST(glob.match("aed"));
                BOOST_TEST_NOT(glob.match("abd"));
                BOOST_TEST_NOT(glob.match("acd"));
            }

            // negated char range
            {
                auto globExp = PathGlobPattern::create("a[^b-d]e");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("aae"));
                BOOST_TEST_NOT(glob.match("abe"));
                BOOST_TEST_NOT(glob.match("ace"));
                BOOST_TEST_NOT(glob.match("ade"));
                BOOST_TEST(glob.match("aee"));
            }
        }

        // escaping
        {
            // escaping *
            {
                auto globExp = PathGlobPattern::create("a\\*b");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("a*b"));
                BOOST_TEST_NOT(glob.match("aab"));
            }

            // escaping literal
            {
                auto globExp = PathGlobPattern::create("a\\bc");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abc"));
                BOOST_TEST_NOT(glob.match("a\\bc"));
            }

            // escaping ?
            {
                auto globExp = PathGlobPattern::create("a\\?c");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("a?c"));
                BOOST_TEST_NOT(glob.match("aac"));
            }

            // unescaping
            {
                auto globExp = PathGlobPattern::create("a\\\\b");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("a\\b"));
                BOOST_TEST_NOT(glob.match("aab"));
            }

            // stray \ becomes part of the literal prefix
            {
                BOOST_TEST(PathGlobPattern::create("a\\"));
            }
        }

        // brace expansion "{<glob>,...}"
        {
            // simple expansion
            {
                auto globExp = PathGlobPattern::create("a{b,c}d");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abd"));
                BOOST_TEST(glob.match("acd"));
                BOOST_TEST_NOT(glob.match("aad"));
            }

            // escaped {
            {
                auto globExp = PathGlobPattern::create("a\\{b,c}d");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("a{b,c}d"));
                BOOST_TEST_NOT(glob.match("abd"));
            }

            // expansion with charsets
            {
                auto globExp = PathGlobPattern::create("a{b[cd],e}f");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abcf"));
                BOOST_TEST(glob.match("abdf"));
                BOOST_TEST(glob.match("aef"));
                BOOST_TEST_NOT(glob.match("aacf"));
            }

            // "," after literal prefix but outside brace expression
            {
                auto globExp = PathGlobPattern::create("ab{c,d}e,f");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abce,f"));
                BOOST_TEST(glob.match("abde,f"));
            }

            // "}" after literal prefix but outside brace expression
            {
                auto globExp = PathGlobPattern::create("ab{c,d}e}f");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abce}f"));
                BOOST_TEST(glob.match("abde}f"));
            }

            // invalid
            {
                // unmatched '[' in expansion
                BOOST_TEST_NOT(PathGlobPattern::create("a{b[cd,e}f"));
                // nested brace expansions
                BOOST_TEST_NOT(PathGlobPattern::create("a{b{c,d}}e"));
                // brace expansion with no terms
                BOOST_TEST_NOT(PathGlobPattern::create("a{}b"));
                // brace expansion with single term
                BOOST_TEST_NOT(PathGlobPattern::create("a{b}c"));
                // stray \\ in glob pattern
                BOOST_TEST_NOT(PathGlobPattern::create("ab{c,d}\\"));
                // incomplete brace expansion
                BOOST_TEST_NOT(PathGlobPattern::create("a{b,c"));
            }
        }

        // max sub patterns
        {
            // max sub patterns exceeded with single brace expansion
            {
                BOOST_TEST_NOT(PathGlobPattern::create("a{b,c,d}z", 2));
            }

            // max sub patterns exceeded with multiple brace expansions
            {
                BOOST_TEST_NOT(PathGlobPattern::create("a{b,c,d}{e,f,g}z", 5));
            }

            // max sub patterns not exceeded with single brance expansion
            {
                auto globExp = PathGlobPattern::create("a{b,c,d}z", 4);
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abz"));
                BOOST_TEST(glob.match("acz"));
                BOOST_TEST(glob.match("adz"));
                BOOST_TEST_NOT(glob.match("aez"));
            }

            // max sub patterns not exceeded with multiple brace expansions
            {
                auto globExp = PathGlobPattern::create("a{b,c,d}{e,f,g}z", 9);
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.match("abez"));
                BOOST_TEST(glob.match("abfz"));
                BOOST_TEST(glob.match("abgz"));
                BOOST_TEST(glob.match("acez"));
                BOOST_TEST(glob.match("acfz"));
                BOOST_TEST(glob.match("acgz"));
                BOOST_TEST(glob.match("adez"));
                BOOST_TEST(glob.match("adfz"));
                BOOST_TEST(glob.match("adgz"));
                BOOST_TEST_NOT(glob.match("aehz"));
            }
        }

        // matchPrefix
        {
            // empty
            {
                auto globExp = PathGlobPattern::create("");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.matchPatternPrefix(""));
            }

            // literal
            {
                auto globExp = PathGlobPattern::create("abc");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.matchPatternPrefix(""));
                BOOST_TEST(glob.matchPatternPrefix("a"));
                BOOST_TEST(glob.matchPatternPrefix("ab"));
                BOOST_TEST(glob.matchPatternPrefix("abc"));
                BOOST_TEST_NOT(glob.matchPatternPrefix("c"));
                BOOST_TEST_NOT(glob.matchPatternPrefix("abcd"));
            }

            // star
            {
                auto globExp = PathGlobPattern::create("a*c");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.matchPatternPrefix(""));
                BOOST_TEST(glob.matchPatternPrefix("a"));
                BOOST_TEST(glob.matchPatternPrefix("ab"));
                BOOST_TEST(glob.matchPatternPrefix("abc"));
                BOOST_TEST(glob.matchPatternPrefix("ac"));
                BOOST_TEST(glob.matchPatternPrefix("ad"));
                BOOST_TEST(glob.matchPatternPrefix("adc"));
                BOOST_TEST_NOT(glob.matchPatternPrefix("b"));
            }

            // star with delimiters
            {
                auto globExp = SymbolGlobPattern::create("ns::c::*");
                BOOST_TEST(globExp);
                SymbolGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.matchPatternPrefix(""));
                BOOST_TEST(glob.matchPatternPrefix("ns"));
                BOOST_TEST(glob.matchPatternPrefix("ns::"));
                BOOST_TEST(glob.matchPatternPrefix("ns::c"));
                BOOST_TEST(glob.matchPatternPrefix("ns::c::"));
                BOOST_TEST(glob.matchPatternPrefix("ns::c::d"));
                BOOST_TEST_NOT(glob.matchPatternPrefix("std"));
            }
        }

        // isLiteral
        {
            // default constructed to empty string
            {
                PathGlobPattern glob;
                BOOST_TEST(glob.isLiteral());
                BOOST_TEST(glob.match(""));
                BOOST_TEST_NOT(glob.match("a"));
            }

            // empty string
            {
                auto globExp = PathGlobPattern::create("");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.isLiteral());
                BOOST_TEST(glob.match(""));
                BOOST_TEST_NOT(glob.match("a"));
            }

            // literal string
            {
                auto globExp = PathGlobPattern::create("abc");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.isLiteral());
                BOOST_TEST(glob.match("abc"));
                BOOST_TEST_NOT(glob.match("abcd"));
                BOOST_TEST_NOT(glob.match("a/b/c"));
            }

            // literal string with escaped characters
            {
                auto globExp = PathGlobPattern::create("a\\*b");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.isLiteral());
                BOOST_TEST(glob.match("a*b"));
                BOOST_TEST_NOT(glob.match("aab"));
            }

            // literal string with all special characters escaped
            {
                auto globExp = PathGlobPattern::create("a\\*\\?\\[\\{\\}\\^\\!\\-\\]\\c");
                BOOST_TEST(globExp);
                PathGlobPattern const& glob = *globExp;
                BOOST_TEST(glob.isLiteral());
                BOOST_TEST(glob.match("a*?[{}^!-]c"));
                BOOST_TEST_NOT(glob.match("a"));
            }
        }
    }
};

TEST_SUITE(
    Glob_test,
    "clang.mrdocs.Glob");

} // clang::mrdocs

