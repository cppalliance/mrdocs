//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom.hpp>
#include <test_suite/test_suite.hpp>
#include <format>


namespace mrdocs {
namespace dom {

struct String_test
{
    void
    string_test()
    {
        // String()
        {
            String s;
            BOOST_TEST(s.empty());
        }

        // String(String&&)
        {
            String s1("hello");
            String s2(std::move(s1));
            BOOST_TEST(s2 == "hello");
        }

        // String(String const&)
        {
            String s1("hello");
            String s2(s1);
            BOOST_TEST(s2 == "hello");
            BOOST_TEST(s1 == "hello");
        }

        // String(std::string_view s)
        {
            String s(std::string_view("hello"));
            BOOST_TEST(s == "hello");
        }

        // String(StringLike const& s)
        {
            String s(std::string("hello"));
            BOOST_TEST(s == "hello");
        }

        // String(char const(&psz)[N])
        {
            String s("hello");
            BOOST_TEST(s == "hello");
        }

        // operator=(String&&)
        {
            String s1("hello");
            String s2;
            s2 = std::move(s1);
            BOOST_TEST(s2 == "hello");
        }

        // operator=(String const&)
        {
            String s1("hello");
            String s2;
            s2 = s1;
            BOOST_TEST(s2 == "hello");
            BOOST_TEST(s1 == "hello");
        }

        // empty()
        {
            String s;
            BOOST_TEST(s.empty());
            s = "hello";
            BOOST_TEST(! s.empty());
        }

        // std::string_view get() const
        {
            String s("hello");
            BOOST_TEST(s.get() == "hello");
        }

        // operator std::string_view()
        {
            String s("hello");
            std::string_view sv(s);
            BOOST_TEST(sv == "hello");
        }

        // std::string str()
        {
            String s("hello");
            BOOST_TEST(s.str() == "hello");
        }

        // std::size_t size() const
        {
            String s("hello");
            BOOST_TEST(s.size() == 5);
        }

        // char const* data() const
        {
            String s("hello");
            BOOST_TEST(s.data() == s.get().data());
        }

        // char const* c_str() const
        {
            String s("hello");
            BOOST_TEST(s.c_str() == s.get().data());
        }

        // swap(String&)
        {
            String s1("hello");
            String s2("world");
            s1.swap(s2);
            BOOST_TEST(s1 == "world");
            BOOST_TEST(s2 == "hello");
        }

        // swap(String&, String&)
        {
            String s1("hello");
            String s2("world");
            swap(s1, s2);
            BOOST_TEST(s1 == "world");
            BOOST_TEST(s2 == "hello");
        }

        // operator==(String const& lhs, StringLike const& rhs)
        {
            String s1("hello");
            std::string s2("hello");
            BOOST_TEST(s1 == s2);
            BOOST_TEST(s2 == s1);
        }

        // operator!=(String const& lhs, StringLike const& rhs)
        {
            String s1("hello");
            std::string s2("hello");
            BOOST_TEST(!(s1 != s2));
            BOOST_TEST(!(s2 != s1));
        }

        // auto operator<=>(String const& lhs, StringLike const& rhs)
        {
            String s1("hello");
            std::string s2("hello");
            BOOST_TEST((s1 <=> s2) == std::strong_ordering::equal);
            BOOST_TEST((s2 <=> s1) == std::strong_ordering::equal);
        }

        // operator==(String const& lhs, String const& rhs)
        {
            String s1("hello");
            String s2("hello");
            BOOST_TEST(s1 == s2);
            BOOST_TEST(s2 == s1);
        }

        // operator!=(String const& lhs, String const& rhs)
        {
            String s1("hello");
            String s2("hello");
            BOOST_TEST(!(s1 != s2));
            BOOST_TEST(!(s2 != s1));
        }

        // auto operator<=>(String const& lhs, String const& rhs)
        {
            String s1("hello");
            String s2("hello");
            BOOST_TEST((s1 <=> s2) == std::strong_ordering::equal);
            BOOST_TEST((s2 <=> s1) == std::strong_ordering::equal);
        }

        // auto operator+(String const& lhs, String const& rhs)
        {
            String s1("hello");
            String s2("world");
            String s3 = s1 + s2;
            BOOST_TEST(s3 == "helloworld");
        }

        // operator+(S const& lhs, String const& rhs)
        {
            String s1("hello");
            std::string s2("world");
            String s3 = s2 + s1;
            BOOST_TEST(s3 == "worldhello");
        }

        // operator+(String const& lhs, S const& rhs)
        {
            String s1("hello");
            std::string s2("world");
            String s3 = s1 + s2;
            BOOST_TEST(s3 == "helloworld");
        }

        // std::formatter<mrdocs::dom::String>
        {
            String s("hello");
            BOOST_TEST(std::format("{}", s) == "hello");
        }
    }

    void run()
    {
        string_test();
    }
};

TEST_SUITE(
    String_test,
    "clang.mrdocs.dom.String");

} // dom
} // mrdocs
