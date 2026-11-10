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


namespace mrdocs {
namespace dom {

struct Array_test
{
    void
    array_test()
    {
        // Array()
        {
            Array a;
            BOOST_TEST(a.empty());
        }

        // Array(Array&&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2(std::move(a1));
            BOOST_TEST(a2.size() == 1);
        }

        // Array(Array const&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2(a1);
            BOOST_TEST(a2.size() == 1);
            BOOST_TEST(a1.size() == 1);
        }

        // Array(impl_type impl)
        {
            auto impl = std::make_shared<DefaultArrayImpl>();
            impl->emplace_back("hello");
            Array a(impl);
            a.emplace_back("world");
            BOOST_TEST(a.size() == 2);
        }

        // Array(storage_type)
        {
            Array::storage_type v;
            v.emplace_back("hello");
            Array a(v);
            a.emplace_back("world");
            BOOST_TEST(a.size() == 2);
        }

        // operator=(Array&&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2 = std::move(a1);
            BOOST_TEST(a2.size() == 1);
        }

        // operator=(Array const&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2 = a1;
            BOOST_TEST(a2.size() == 1);
            BOOST_TEST(a1.size() == 1);
        }

        // auto impl() const
        {
            {
                Array a;
                a.emplace_back("hello");
                BOOST_TEST(a.impl()->size() == 1);
            }

            {
                auto impl = std::make_shared<DefaultArrayImpl>();
                impl->emplace_back("hello");
                Array a(impl);
                a.emplace_back("world");
                BOOST_TEST(a.impl()->size() == 2);
                BOOST_TEST(impl->size() == 2);
            }
        }

        // char const* typeKey()
        {
            Array a;
            BOOST_TEST(std::string_view(a.typeKey()) == "Array");
        }

        // bool empty()
        {
            Array a;
            BOOST_TEST(a.empty());
            a.emplace_back("hello");
            BOOST_TEST(! a.empty());
        }

        // size_type size()
        {
            Array a;
            BOOST_TEST(a.empty());
            a.emplace_back("hello");
            BOOST_TEST(a.size() == 1);
        }

        // void set(size_type i, Value v);
        {
            Array a;
            a.emplace_back("hello");
            a.set(0, "world");
            BOOST_TEST(a.get(0) == "world");
        }

        // value_type get(size_type i)
        // value_type at(size_type i)
        {
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(a.get(0) == "hello");
            BOOST_TEST(a.at(0) == "hello");
        }

        // value_type front()
        {
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(a.front() == "hello");
        }

        // value_type back()
        {
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(a.back() == "hello");
        }

        // iterator begin()
        // iterator end()
        {
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(*a.begin() == "hello");
            BOOST_TEST(a.begin() != a.end());
        }

        // void push_back(value_type value)
        {
            Array a;
            a.push_back("hello");
            BOOST_TEST(a.size() == 1);
            BOOST_TEST(a.at(0) == "hello");
        }

        // template< class... Args >
        // void emplace_back(Args&&... args)
        {
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(a.size() == 1);
            BOOST_TEST(a.at(0) == "hello");
        }

        // friend Array operator+(Array const& lhs, Array const& rhs);
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2.emplace_back("world");
            Array a3 = a1 + a2;
            BOOST_TEST(a3.size() == 2);
            BOOST_TEST(a3.get(0) == "hello");
            BOOST_TEST(a3.get(1) == "world");
        }

        // template <std::convertible_to<Array> S>
        // friend auto operator+(S const& lhs, Array const& rhs)
        {
            Array a1;
            a1.emplace_back("hello");
            std::vector<Value> a2;
            a2.emplace_back("world");
            Array a3 = a2 + a1;
            BOOST_TEST(a3.size() == 2);
            BOOST_TEST(a3.get(0) == "world");
            BOOST_TEST(a3.get(1) == "hello");
            Array a4 = a1 + a2;
            BOOST_TEST(a4.size() == 2);
            BOOST_TEST(a4.get(0) == "hello");
            BOOST_TEST(a4.get(1) == "world");
        }

        // void swap(Array& other) noexcept
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2.emplace_back("world");
            a1.swap(a2);
            BOOST_TEST(a1.size() == 1);
            BOOST_TEST(a2.size() == 1);
            BOOST_TEST(a1.get(0) == "world");
            BOOST_TEST(a2.get(0) == "hello");
        }

        // friend void swap(Array& lhs, Array& rhs) noexcept
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2.emplace_back("world");
            swap(a1, a2);
            BOOST_TEST(a1.size() == 1);
            BOOST_TEST(a2.size() == 1);
            BOOST_TEST(a1.get(0) == "world");
            BOOST_TEST(a2.get(0) == "hello");
        }

        // operator==(Array const&, Array const&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2.emplace_back("hello");
            BOOST_TEST(a1 == a2);
            BOOST_TEST(a2 == a1);
            a1 = a2;
            BOOST_TEST(a1 == a2);
            BOOST_TEST(a2 == a1);
        }

        // operator<=>(Array const&, Array const&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2.emplace_back("hello");
            BOOST_TEST((a1 <=> a2) == std::strong_ordering::equal);
            BOOST_TEST((a2 <=> a1) == std::strong_ordering::equal);
        }

        // toString(Array const&);
        {
            // Behave same as JS:
            // x = ['hello']
            // x.toString() == 'hello'
            Array a;
            BOOST_TEST(toString(a).empty());
            a.emplace_back("hello");
            BOOST_TEST(toString(a) == "hello");
            a.emplace_back("world");
            BOOST_TEST(toString(a) == "hello,world");
        }
    }

    void run()
    {
        array_test();
    }
};

TEST_SUITE(
    Array_test,
    "clang.mrdocs.dom.Array");

} // dom
} // mrdocs
