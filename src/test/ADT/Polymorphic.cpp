//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/ADT/Polymorphic.hpp>
#include <test_suite/test_suite.hpp>

namespace clang::mrdocs {

struct X {
    virtual ~X() = default;

    int a{42};
};

struct Y : X {
    int b{43};
};

struct Z : X {
    int c{44};
};

struct Polymorphic_test
{
    static
    void
    testConstructors()
    {
        // nullopt constructor
        {
          Polymorphic<X> constexpr v(std::nullopt);
          BOOST_TEST_NOT(v);
        }

        // from derived object
        {
            Polymorphic<X> x(Y{});
            BOOST_TEST(x);
            BOOST_TEST(x->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*x)->b == 43);
        }

        // from derived
        {
            auto x = Polymorphic<X>(std::in_place_type<Y>);
            BOOST_TEST(x);
            BOOST_TEST(x->a == 42);
            BOOST_TEST(dynamic_cast<Y *>(&*x)->b == 43);
        }

        // Copy constructor
        {
            // from empty
            {
                Polymorphic<X> x = std::nullopt;
                Polymorphic<X> y(x);
                BOOST_TEST_NOT(y);
            }

            // from valid
            {
                Polymorphic<X> x(std::in_place_type<Y>);
                x->a = 45;
                Polymorphic<X> y(x);
                BOOST_TEST(y);
                BOOST_TEST(y->a == 45);
                BOOST_TEST(dynamic_cast<Y *>(&*y)->b == 43);
            }
        }

        // Move constructor
        {
            auto x = Polymorphic<X>(std::in_place_type<Y>);
            x->a = 45;
            Polymorphic<X> y(std::move(x));
            BOOST_TEST_NOT(x);
            BOOST_TEST(y);
            BOOST_TEST(y->a == 45);
            BOOST_TEST(dynamic_cast<Y *>(&*y)->b == 43);
        }

        // Copy from derived value constructor
        {
            Polymorphic<Y> x;
            x->a = 45;
            Polymorphic<X> y(std::in_place_type<Y>, *x);
            BOOST_TEST(y);
            BOOST_TEST(y->a == 45);
            BOOST_TEST(dynamic_cast<Y *>(&*y)->b == 43);
        }

        // In-place constructor
        {
            Polymorphic<X> x(std::in_place_type<Y>);
            BOOST_TEST(x);
            BOOST_TEST(x->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*x)->b == 43);
        }
    }

    static
    void
    testAssignment()
    {
        // copy assignment
        {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
            // from same
            {
                Polymorphic<X> lhs(std::in_place_type<Y>);
                lhs = lhs;
                BOOST_TEST(lhs);
                BOOST_TEST(lhs->a == 42);
            }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
            // from empty
            {
                Polymorphic<X> lhs;
                Polymorphic<X> rhs = std::nullopt;
                lhs = rhs;
                BOOST_TEST_NOT(lhs);
            }

            // from valid
            {
                Polymorphic<X> lhs(std::in_place_type<Y>);
                lhs->a = 45;
                Polymorphic<X> rhs(std::in_place_type<Y>);
                rhs->a = 46;
                BOOST_TEST(lhs->a == 45);
                BOOST_TEST(rhs->a == 46);
                lhs = rhs;
                BOOST_TEST(lhs);
                BOOST_TEST(lhs->a == 46);
                BOOST_TEST(rhs->a == 46);
                BOOST_TEST(static_cast<Y &>(*lhs).b == 43);
            }
        }

        // copy assignment
        {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
            // from same
            {
                Polymorphic<X> lhs(std::in_place_type<Y>);
                lhs = std::move(lhs);
                BOOST_TEST(lhs);
                BOOST_TEST(lhs->a == 42);
            }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

            // from empty
            {
                Polymorphic<X> lhs;
                Polymorphic<X> rhs = std::nullopt;
                lhs = std::move(rhs);
                BOOST_TEST_NOT(lhs);
                BOOST_TEST_NOT(rhs);
            }

            // from valid
            {
                Polymorphic<X> lhs(std::in_place_type<Y>);
                lhs->a = 45;
                Polymorphic<X> rhs(std::in_place_type<Y>);
                rhs->a = 46;
                BOOST_TEST(lhs->a == 45);
                BOOST_TEST(rhs->a == 46);
                lhs = std::move(rhs);
                BOOST_TEST(lhs->a == 46);
                BOOST_TEST_NOT(rhs);
                BOOST_TEST(static_cast<Y &>(*lhs).b == 43);
            }
        }

        // copy from derived
        {
            Polymorphic<X> lhs(std::in_place_type<Y>);
            lhs->a = 45;
            Y rhs;
            rhs.a = 46;
            BOOST_TEST(lhs->a == 45);
            BOOST_TEST(rhs.a == 46);
            lhs = Polymorphic<X>(std::in_place_type<Y>, rhs);
            BOOST_TEST(lhs);
            BOOST_TEST(lhs->a == 46);
            BOOST_TEST(static_cast<Y &>(*lhs).b == 43);
        }

        // copy from derived
        {
            Polymorphic<X> lhs(std::in_place_type<Y>);
            lhs->a = 45;
            Y rhs;
            rhs.a = 46;
            BOOST_TEST(lhs->a == 45);
            BOOST_TEST(rhs.a == 46);
            lhs = Polymorphic<X>(std::in_place_type<Y>, std::move(rhs));
            BOOST_TEST(lhs);
            BOOST_TEST(lhs->a == 46);
            BOOST_TEST(static_cast<Y &>(*lhs).b == 43);
        }
    }

    static
    void
    testDereference()
    {
        // from derived object
        {
            Polymorphic<X> x(Y{});
            BOOST_TEST((*x).a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*x)->b == 43);
        }

        // from pointer
        {
            Polymorphic<X> x(std::in_place_type<Y>);
            BOOST_TEST((*x).a == 42);
            BOOST_TEST(dynamic_cast<Y *>(&*x)->b == 43);
        }
    }

    static
    void
    testSwap()
    {
        // default constructor
        {
            Polymorphic<X> lhs = std::nullopt;
            Polymorphic<X> rhs(std::in_place_type<Y>);
            swap(lhs, rhs);
            BOOST_TEST(lhs);
            BOOST_TEST(lhs->a == 42);
            BOOST_TEST(dynamic_cast<Y *>(&*lhs)->b == 43);
            BOOST_TEST_NOT(rhs);
        }

        // rhs: default constructor
        {
            Polymorphic<X> lhs(std::in_place_type<Y>);
            Polymorphic<X> rhs = std::nullopt;
            swap(lhs, rhs);
            BOOST_TEST_NOT(lhs);
            BOOST_TEST(rhs);
            BOOST_TEST(rhs->a == 42);
            BOOST_TEST(dynamic_cast<Y *>(&*rhs)->b == 43);
        }

        // lhs: from derived object
        {
            Polymorphic<X> lhs(Y{});
            Polymorphic<X> rhs(std::in_place_type<Y>);
            swap(lhs, rhs);
            BOOST_TEST(rhs);
            BOOST_TEST(rhs->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*rhs)->b == 43);
            BOOST_TEST(lhs);
            BOOST_TEST(lhs->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*lhs)->b == 43);
        }

        // rhs: from derived object
        {
            Polymorphic<X> lhs(std::in_place_type<Y>);
            Polymorphic<X> rhs(Y{});
            swap(lhs, rhs);
            BOOST_TEST(lhs);
            BOOST_TEST(lhs->a == 42);
            BOOST_TEST(dynamic_cast<Y *>(&*lhs)->b == 43);
            BOOST_TEST(rhs);
            BOOST_TEST(rhs->a == 42);
            BOOST_TEST(dynamic_cast<Y *>(&*rhs)->b == 43);
        }

        // lhs: from pointer
        {
            Polymorphic<X> lhs(std::in_place_type<Y>);
            Polymorphic<X> rhs(std::in_place_type<Y>);
            swap(lhs, rhs);
            BOOST_TEST(rhs);
            BOOST_TEST(rhs->a == 42);
            BOOST_TEST(dynamic_cast<Y *>(&*rhs)->b == 43);
            BOOST_TEST(lhs);
            BOOST_TEST(lhs->a == 42);
            BOOST_TEST(dynamic_cast<Y *>(&*lhs)->b == 43);
        }
    }

    void
    run()
    {
        testConstructors();
        testAssignment();
        testDereference();
        testSwap();
    }
};

TEST_SUITE(
    Polymorphic_test,
    "clang.mrdocs.ADT.Polymorphic");

} // clang::mrdocs

