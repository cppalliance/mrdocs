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
        // default constructor
        {
            Polymorphic<X> constexpr v;
            BOOST_TEST_NOT(v);
        }

        // nullptr constructor
        {
            Polymorphic<X> constexpr v(nullptr);
            BOOST_TEST_NOT(v);
        }

        // from derived object
        {
            Polymorphic<X> x(Y{});
            BOOST_TEST(x);
            BOOST_TEST(x->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*x)->b == 43);
        }

        // from pointer
        {
            Polymorphic<X> x(new Y);
            BOOST_TEST(x);
            BOOST_TEST(x->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*x)->b == 43);
        }

        // from nullptr pointer
        {
            X* p = nullptr;
            Polymorphic<X> const x(p);
            BOOST_TEST_NOT(x);
        }

        // from pointer different from Derived
        {
            Y* p = new Y{};
            X* p2 = p;
            BOOST_TEST_THROWS(Polymorphic<X>(p2), BadPolymorphicConstruction);
        }

        // from pointer and copier
        {
            auto copier = [](Y const& y) -> Y* { auto el = new Y(y); el->b = 44; return el; };
            Polymorphic<X> x(new Y, copier);
            BOOST_TEST(x);
            BOOST_TEST(x->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*x)->b == 43);

            Polymorphic<X> x2(x);
            BOOST_TEST(x2);
            BOOST_TEST(x2->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*x2)->b == 44);
        }

        // from pointer, copier and deleter
        {
            auto copier = [](Y const& y) -> Y* { auto el = new Y(y); el->b = 44; return el; };
            auto deleter = [](Y const* y) { delete y; };
            Polymorphic<X> x(new Y, copier, deleter);
            BOOST_TEST(x);
            BOOST_TEST(x->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*x)->b == 43);

            Polymorphic<X> x2(x);
            BOOST_TEST(x2);
            BOOST_TEST(x2->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*x2)->b == 44);
        }

        // Copy constructor
        {
            // from empty
            {
                Polymorphic<X> x;
                Polymorphic<X> y(x);
                BOOST_TEST_NOT(y);
            }

            // from valid
            {
                Polymorphic<X> x(new Y);
                x->a = 45;
                Polymorphic<X> y(x);
                BOOST_TEST(y);
                BOOST_TEST(y->a == 45);
                BOOST_TEST(dynamic_cast<Y*>(&*y)->b == 43);
            }
        }

        // Move constructor
        {
            Polymorphic<X> x(new Y);
            x->a = 45;
            Polymorphic<X> y(std::move(x));
            BOOST_TEST_NOT(x);
            BOOST_TEST(y);
            BOOST_TEST(y->a == 45);
            BOOST_TEST(dynamic_cast<Y*>(&*y)->b == 43);
        }

        // Copy from derived value constructor
        {
            // from empty
            {
                Polymorphic<Y> x;
                Polymorphic<X> y(x);
                BOOST_TEST_NOT(y);
            }

            // from valid
            {
                Polymorphic<Y> x(new Y);
                x->a = 45;
                Polymorphic<X> y(x);
                BOOST_TEST(y);
                BOOST_TEST(y->a == 45);
                BOOST_TEST(dynamic_cast<Y*>(&*y)->b == 43);
            }
        }

        // Move from derived constructor
        {
            // from empty
            {
                Polymorphic<Y> x;
                Polymorphic<X> y(std::move(x));
                BOOST_TEST_NOT(y);
            }

            // from valid
            {
                Polymorphic<Y> x(new Y);
                x->a = 45;
                Polymorphic<X> y(std::move(x));
                BOOST_TEST_NOT(x);
                BOOST_TEST(y);
                BOOST_TEST(y->a == 45);
                BOOST_TEST(dynamic_cast<Y*>(&*y)->b == 43);
            }
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
                Polymorphic<X> lhs(new Y);
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
                Polymorphic<X> rhs;
                lhs = rhs;
                BOOST_TEST_NOT(lhs);
            }

            // from valid
            {
                Polymorphic<X> lhs(new Y);
                lhs->a = 45;
                Polymorphic<X> rhs(new Y);
                rhs->a = 46;
                BOOST_TEST(lhs->a == 45);
                BOOST_TEST(rhs->a == 46);
                lhs = rhs;
                BOOST_TEST(lhs);
                BOOST_TEST(lhs->a == 46);
                BOOST_TEST(rhs->a == 46);
                BOOST_TEST(get<Y>(lhs).b == 43);
            }
        }

        // copy assignment
        {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#pragma clang diagnostic ignored "-Wself-move"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
            // from same
            {
                Polymorphic<X> lhs(new Y);
                lhs = std::move(lhs);
                BOOST_TEST(lhs);
                BOOST_TEST(lhs->a == 42);
            }
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

            // from empty
            {
                Polymorphic<X> lhs;
                Polymorphic<X> rhs;
                lhs = std::move(rhs);
                BOOST_TEST_NOT(lhs);
                BOOST_TEST_NOT(rhs);
            }

            // from valid
            {
                Polymorphic<X> lhs(new Y);
                lhs->a = 45;
                Polymorphic<X> rhs(new Y);
                rhs->a = 46;
                BOOST_TEST(lhs->a == 45);
                BOOST_TEST(rhs->a == 46);
                lhs = std::move(rhs);
                BOOST_TEST(lhs->a == 46);
                BOOST_TEST_NOT(rhs);
                BOOST_TEST(get<Y>(lhs).b == 43);
            }
        }

        // copy from derived
        {
            Polymorphic<X> lhs(new Y);
            lhs->a = 45;
            Y rhs;
            rhs.a = 46;
            BOOST_TEST(lhs->a == 45);
            BOOST_TEST(rhs.a == 46);
            lhs = rhs;
            BOOST_TEST(lhs);
            BOOST_TEST(lhs->a == 46);
            BOOST_TEST(get<Y>(lhs).b == 43);
        }

        // copy from derived
        {
            Polymorphic<X> lhs(new Y);
            lhs->a = 45;
            Y rhs;
            rhs.a = 46;
            BOOST_TEST(lhs->a == 45);
            BOOST_TEST(rhs.a == 46);
            lhs = std::move(rhs);
            BOOST_TEST(lhs);
            BOOST_TEST(lhs->a == 46);
            BOOST_TEST(get<Y>(lhs).b == 43);
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
            Polymorphic<X> x(new Y);
            BOOST_TEST((*x).a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*x)->b == 43);
        }
    }

    static
    void
    testMake()
    {
        // MakePolymorphic<Base, Derived>(args...)
        {
            auto x = MakePolymorphic<X, Y>();
            BOOST_TEST(x);
            BOOST_TEST(x->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*x)->b == 43);
        }
    }

    static
    void
    testDynamicCast()
    {
        // from valid
        {
            Polymorphic<X> x(new Y);
            auto y = DynamicCast<Y>(std::move(x));
            BOOST_TEST(y);
            BOOST_TEST(y->a == 42);
            BOOST_TEST(y->b == 43);
        }

        // from empty
        {
            Polymorphic<X> x;
            auto y = DynamicCast<Y>(std::move(x));
            BOOST_TEST_NOT(y);
        }

        // from invalid derived type
        {
            Polymorphic<X> x(new Z);
            auto y = DynamicCast<Y>(std::move(x));
            BOOST_TEST_NOT(y);
        }
    }

    static
    void
    testSwap()
    {
        // default constructor
        {
            Polymorphic<X> lhs;
            Polymorphic<X> rhs(new Y);
            swap(lhs, rhs);
            BOOST_TEST(lhs);
            BOOST_TEST(lhs->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*lhs)->b == 43);
            BOOST_TEST_NOT(rhs);
        }

        // rhs: default constructor
        {
            Polymorphic<X> lhs(new Y);
            Polymorphic<X> rhs;
            swap(lhs, rhs);
            BOOST_TEST_NOT(lhs);
            BOOST_TEST(rhs);
            BOOST_TEST(rhs->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*rhs)->b == 43);
        }

        // lhs: from derived object
        {
            Polymorphic<X> lhs(Y{});
            Polymorphic<X> rhs(new Y);
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
            Polymorphic<X> lhs(new Y);
            Polymorphic<X> rhs(Y{});
            swap(lhs, rhs);
            BOOST_TEST(lhs);
            BOOST_TEST(lhs->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*lhs)->b == 43);
            BOOST_TEST(rhs);
            BOOST_TEST(rhs->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*rhs)->b == 43);
        }

        // lhs: from pointer
        {
            Polymorphic<X> lhs(new Y);
            Polymorphic<X> rhs(new Y);
            swap(lhs, rhs);
            BOOST_TEST(rhs);
            BOOST_TEST(rhs->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*rhs)->b == 43);
            BOOST_TEST(lhs);
            BOOST_TEST(lhs->a == 42);
            BOOST_TEST(dynamic_cast<Y*>(&*lhs)->b == 43);
        }
    }

    static
    void
    testIsA()
    {
        // IsA<Derived>(x)
        {
            Polymorphic<X> const x(new Y);
            BOOST_TEST(IsA<X>(x));
            BOOST_TEST(IsA<Y>(x));
        }

        // Empty state
        {
            Polymorphic<X> const x;
            BOOST_TEST_NOT(IsA<X>(x));
            BOOST_TEST_NOT(IsA<Y>(x));
        }
    }

    static
    void
    testGet()
    {
        // from mutable
        {
            Polymorphic<X> x(new Y);

            // to mutable derived
            {
                // get<Derived>(x)
                {
                    get<X>(x).a = 30;
                    BOOST_TEST(get<X>(x).a == 30);
                    get<Y>(x).b = 31;
                    BOOST_TEST(get<Y>(x).b == 31);
                }

                // get<Derived&>(x)
                {
                    get<X&>(x).a = 32;
                    BOOST_TEST(get<X&>(x).a == 32);
                    get<Y&>(x).b = 33;
                    BOOST_TEST(get<Y&>(x).b == 33);
                }

                // get<Derived*>(x)
                {
                    get<X*>(x)->a = 34;
                    BOOST_TEST(get<X*>(x)->a == 34);
                    get<Y*>(x)->b = 35;
                    BOOST_TEST(get<Y*>(x)->b == 35);
                }
            }

            // to const derived
            {
                // get<Derived const>(x)
                {
                    BOOST_TEST(get<X const>(x).a == 34);
                    BOOST_TEST(get<Y const>(x).b == 35);
                }

                // get<Derived const&>(x)
                {
                    BOOST_TEST(get<X const&>(x).a == 34);
                    BOOST_TEST(get<Y const&>(x).b == 35);
                }

                // get<Derived const*>(x)
                {
                    BOOST_TEST(get<X const*>(x)->a == 34);
                    BOOST_TEST(get<Y const*>(x)->b == 35);
                }
            }
        }

        // from const
        {
            Polymorphic<X> const x(new Y);

            // to mutable derived (returns const anyway)
            {
                // get<Derived>(x)
                {
                    BOOST_TEST(get<X>(x).a == 42);
                    BOOST_TEST(get<Y>(x).b == 43);
                }

                // get<Derived&>(x)
                {
                    BOOST_TEST(get<X&>(x).a == 42);
                    BOOST_TEST(get<Y&>(x).b == 43);
                }

                // get<Derived*>(x)
                {
                    BOOST_TEST(get<X*>(x)->a == 42);
                    BOOST_TEST(get<Y*>(x)->b == 43);
                }
            }

            // to const derived
            {
                // get<Derived const>(x)
                {
                    BOOST_TEST(get<X const>(x).a == 42);
                    BOOST_TEST(get<Y const>(x).b == 43);
                }

                // get<Derived const&>(x)
                {
                    BOOST_TEST(get<X const&>(x).a == 42);
                    BOOST_TEST(get<Y const&>(x).b == 43);
                }

                // get<Derived const*>(x)
                {
                    BOOST_TEST(get<X const*>(x)->a == 42);
                    BOOST_TEST(get<Y const*>(x)->b == 43);
                }
            }
        }
    }

    void
    run()
    {
        testConstructors();
        testAssignment();
        testDereference();
        testMake();
        testDynamicCast();
        testSwap();
        testIsA();
        testGet();
    }
};

TEST_SUITE(
    Polymorphic_test,
    "clang.mrdocs.ADT.Polymorphic");

} // clang::mrdocs

