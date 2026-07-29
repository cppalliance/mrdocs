//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/ADT/ArrayView.hpp>
#include <test_suite/test_suite.hpp>

namespace mrdocs {

struct ArrayViewTest {
    template <class T>
    static bool equal(ArrayView<T> v, std::initializer_list<T> il) {
        if (v.size() != il.size())
            return false;
        return std::equal(v.begin(), v.end(), il.begin(), il.end());
    }

    void
    run()
    {
        // default ctor
        {
            ArrayView<int> v;
            BOOST_TEST(v.empty());
            BOOST_TEST_EQ(v.size(), 0u);
            BOOST_TEST(v.begin() == v.end());
            BOOST_TEST(v.data() == nullptr);
        }

        // array ctor + deduction guide
        {
            int a[] = {1,2,3,4};
            ArrayView v(a); // CTAD
            static_assert(std::is_same_v<decltype(v), ArrayView<int>>);
            BOOST_TEST(!v.empty());
            BOOST_TEST_EQ(v.size(), 4u);
            BOOST_TEST_EQ(v.front(), 1);
            BOOST_TEST_EQ(v.back(), 4);
            BOOST_TEST(v.data() == a);
            BOOST_TEST(equal(v, {1,2,3,4}));
        }

        // pointer + size ctor
        {
            int a[] = {10,20,30,40,50};
            ArrayView<int> v(a + 1, 3);
            BOOST_TEST_EQ(v.size(), 3u);
            BOOST_TEST(equal(v, {20,30,40}));
            BOOST_TEST_EQ(v[0], 20);
            BOOST_TEST_EQ(v[2], 40);
        }

        // iterator + count ctor (contiguous iterators)
        {
            int a[] = {7,8,9};
            auto first = a;
            ArrayView<int> v(first, 2);
            BOOST_TEST_EQ(v.size(), 2u);
            BOOST_TEST(equal(v, {7,8}));
        }

        // iterators and reverse iterators
        {
            int a[] = {1,2,3};
            ArrayView<int> v(a);
            BOOST_TEST(std::distance(v.begin(), v.end()) == 3);
            BOOST_TEST(*v.begin() == 1);
            BOOST_TEST(*(v.end() - 1) == 3);

            auto r = v.rbegin();
            BOOST_TEST(*r == 3);
            ++r;
            BOOST_TEST(*r == 2);
        }

        // at(), bounds-checked (via assert in debug); just verify value here
        {
            int a[] = {11,22};
            ArrayView<int> v(a);
            BOOST_TEST_EQ(v.at(0), 11);
            BOOST_TEST_EQ(v.at(1), 22);
        }

        // remove_prefix / remove_suffix adjust the view only
        {
            int a[] = {1,2,3,4,5};
            ArrayView<int> v(a);
            v.remove_prefix(1);
            BOOST_TEST(equal(v, {2,3,4,5}));
            v.remove_suffix(2);
            BOOST_TEST(equal(v, {2,3}));
        }

        // slice / take_* / drop_* helpers
        {
            int a[] = {5,6,7,8,9};
            ArrayView<int> v(a);
            auto s1 = v.slice(1, 3);
            BOOST_TEST(equal(s1, {6,7,8}));

            auto s2 = v.slice(3, ArrayView<int>::npos);
            BOOST_TEST(equal(s2, {8,9}));

            auto tf = v.take_front(2);
            BOOST_TEST(equal(tf, {5,6}));

            auto tb = v.take_back(3);
            BOOST_TEST(equal(tb, {7,8,9}));

            auto df = v.drop_front(4);
            BOOST_TEST(equal(df, {9}));

            auto db = v.drop_back(5);
            BOOST_TEST(db.empty());
        }

        // comparisons: == and <=>
        {
            int a[] = {1,2,3};
            int b[] = {1,2,3};
            int c[] = {1,2,4};
            int d[] = {1,2,3,0};

            ArrayView<int> va(a), vb(b), vc(c), vd(d);

            // equality
            BOOST_TEST(va == vb);
            BOOST_TEST(!(va == vc));

            // three-way: va vs vc (3 < 4)
            {
                auto ord = (va <=> vc);
                BOOST_TEST(ord == std::strong_ordering::less);
            }
            // three-way: vc vs va
            {
                auto ord = (vc <=> va);
                BOOST_TEST(ord == std::strong_ordering::greater);
            }
            // prefix vs longer with same prefix: va (3) vs vd (3,0)
            {
                auto ord = (va <=> vd);
                BOOST_TEST(ord == std::strong_ordering::less);
            }
            // identical content
            {
                auto ord = (va <=> vb);
                BOOST_TEST(ord == std::strong_ordering::equal);
            }
        }

        // data() exposure and aliasing (view must not modify underlying)
        {
            int a[] = {42,43};
            ArrayView<int> v(a);
            // mutate underlying array; view should reflect it
            a[1] = 99;
            BOOST_TEST_EQ(v.back(), 99);
        }
    }
};

TEST_SUITE(ArrayViewTest, "clang.mrdocs.ADT.ArrayView");

} // namespace mrdocs
