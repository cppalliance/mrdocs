//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Dom/LazyObject.hpp"
#include <test_suite/test_suite.hpp>

namespace clang {
namespace mrdocs {
namespace dom {

struct Y {
    std::string a = "hello";
    std::string b = "world";
};

void
tag_invoke(
    ValueFromTag,
    Value& v,
    Y const& y)
{
    v = y.a + " " + y.b;
}

struct X {
    int i = 123;
    std::string s = "hello";
    Y y;
};

template<>
struct MappingTraits<X>
{
    template <class IO>
    void map(IO &io, X const& x) const
    {
        io.map("i", x.i);
        io.map("s", x.s);
        io.defer("si", [&x]{ return x.s + std::to_string(x.i); });
        io.map("y", x.y);
    }
};

struct LazyObject_test
{
    void
    testConstructor()
    {
        X x;
        LazyObjectImpl<X> obj(x);
    }

    void
    testTypeKey()
    {
        X x;
        LazyObjectImpl<X> obj(x);
        BOOST_TEST(std::string_view(obj.type_key()) == "LazyObject");
    }

    void
    testGet()
    {
        X x;
        LazyObjectImpl<X> obj(x);

        // Convertible to Value
        {
            BOOST_TEST(obj.get("i") == 123);
            BOOST_TEST(obj.get("s") == "hello");
        }

        // Change value through the original object
        {
            x.i = 789;
            BOOST_TEST(obj.get("i") == 789);
        }
    }

    void
    testSet()
    {
        X x;
        LazyObjectImpl<X> obj(x);

        // Change value
        {
            obj.set("i", 456);
            BOOST_TEST(obj.get("i") == 456);

            // Changing value via the original object
            // no longer affects the LazyObject
            x.i = 789;
            BOOST_TEST(obj.get("i") == 456);
        }

        // Make undefined
        {
            obj.set("i", Value{});
            BOOST_TEST(obj.get("i").isUndefined());
        }

        // Add new value
        {
            obj.set("x", 789);
            BOOST_TEST(obj.get("x") == 789);
        }
    }

    void
    testExists()
    {
        X x;
        LazyObjectImpl<X> obj(x);

        // original members
        {
            BOOST_TEST(obj.exists("i"));
            BOOST_TEST(obj.exists("s"));
            BOOST_TEST_NOT(obj.exists("x"));
        }

        // new members
        {
            obj.set("x", 789);
            BOOST_TEST(obj.exists("x"));
        }
    }

    void
    testSize()
    {
        X x;
        LazyObjectImpl<X> obj(x);

        // original object
        {
            BOOST_TEST(obj.size() == 2);
        }

        // new values
        {
            obj.set("x", 789);
            BOOST_TEST(obj.size() == 3);
        }

        // replacing in overlay doesn't increase size
        {
            obj.set("i", 456);
            BOOST_TEST(obj.size() == 3);
        }

        // undefined values don't reduce the size
        {
            obj.set("i", Value{});
            BOOST_TEST(obj.size() == 3);
        }
    }

    void
    testVisit()
    {
        X x;
        LazyObjectImpl<X> obj(x);

        // visit original members
        {
            std::size_t count = 0;
            bool match = true;
            obj.visit([&count, &match](String key, Value value) {
                if (key == "i" && value != 123)
                    match = false;
                if (key == "s" && value != "hello")
                    match = false;
                if (key == "si" && value != "hello123")
                    match = false;
                if (key == "y" && value != "hello world")
                    match = false;
                ++count;
                return true;
            });
            BOOST_TEST(count == 4);
        }

        // visit new members
        {
            obj.set("x", 789);
            std::size_t count = 0;
            bool found = false;
            obj.visit([&count, &found](String key, Value value) {
                if (key == "x" && value == 789)
                    found = true;
                ++count;
                return true;
            });
            BOOST_TEST(count == 5);
            BOOST_TEST(found);
        }

        // stop visiting
        {
            std::size_t count = 0;
            obj.visit([&count](String key, Value value) {
                ++count;
                return false;
            });
            BOOST_TEST(count == 1);
        }

        // replacing in overlay doesn't increase size
        {
            obj.set("i", 456);
            std::size_t count = 0;
            bool match = true;
            obj.visit([&count, &match](String key, Value value) {
                if (key == "i" && value != 456)
                    match = false;
                if (key == "s" && value != "hello")
                    match = false;
                if (key == "x" && value != 789)
                    match = false;
                ++count;
                return true;
            });
            BOOST_TEST(count == 5);
        }
    }

    void run()
    {
        testConstructor();
        testTypeKey();
        testGet();
        testSet();
        testExists();
        testVisit();
    }
};

TEST_SUITE(
    LazyObject_test,
    "clang.mrdocs.dom.LazyObject");

} // dom
} // mrdocs
} // clang

