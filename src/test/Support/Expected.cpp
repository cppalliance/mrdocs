//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/Expected.hpp>
#include <test_suite/test_suite.hpp>
#include <string>
#include <type_traits>
#include <utility>

namespace clang {
namespace mrdocs {

struct ExpectedTest
{
    struct S {
        int v = 0;
        int
        get() const noexcept
        {
            return v;
        }
        auto
        operator<=>(S const&) const
            = default;
    };

    // Helpers returning Expected for monadic tests
    static
    Expected<int, Error>
    plus_one(int x)
    {
        return Expected<int, Error>(x + 1);
    }

    static
    Expected<int, Error>
    fail_with(std::string msg)
    {
        return Expected<int, Error>(unexpect, Error(std::move(msg)));
    }

    static
    Expected<void, Error>
    ok_void()
    {
        return Expected<void, Error>(std::in_place);
    }

    static
    Expected<void, Error>
    fail_void(std::string msg)
    {
        return Expected<void, Error>(unexpect, Error(std::move(msg)));
    }

    void
    test_value_expected_basic()
    {
        using E = Error;

        // default-construct value (if T is default-constructible)
        {
            Expected<int, E> e(std::in_place, 0);
            BOOST_TEST(e.has_value());
            BOOST_TEST(*e == 0);
            BOOST_TEST(e.value() == 0);
        }

        // construct from value
        {
            Expected<int, E> e(42);
            BOOST_TEST(e);
            BOOST_TEST(*e == 42);

            // copy/move
            Expected<int, E> c = e;
            BOOST_TEST(c);
            BOOST_TEST(*c == 42);

            Expected<int, E> m = std::move(e);
            BOOST_TEST(m);
            BOOST_TEST(*m == 42);

            // assign value
            c = 7;
            BOOST_TEST(c);
            BOOST_TEST(*c == 7);
        }

        // construct unexpected
        {
            Expected<int, E> e(unexpect, E("bang"));
            BOOST_TEST(!e);
            BOOST_TEST(e.error().failed());
            BOOST_TEST(!e.has_value());
            // error_or
            auto er = e.error_or(E("alt"));
            BOOST_TEST(er.failed());
        }

        // assign unexpected
        {
            Expected<int, E> e(3);
            e = Unexpected<E>(E("nope"));
            BOOST_TEST(!e);
            BOOST_TEST(e.error().failed());
        }

        // emplace
        {
            Expected<int, E> e(unexpect, E("x"));
            e.emplace(99);
            BOOST_TEST(e);
            BOOST_TEST(*e == 99);
        }

        // operator-> / operator* with object type
        {
            Expected<S, E> es(S{ 5 });
            BOOST_TEST(es->get() == 5);
            BOOST_TEST((*es).v == 5);
        }

        // value_or
        {
            Expected<int, E> a(10);
            Expected<int, E> b(unexpect, E("err"));
            BOOST_TEST(a.value_or(1) == 10);
            BOOST_TEST(b.value_or(1) == 1);
        }

        // and_then (success)
        {
            Expected<int, E> e(10);
            auto r = e.and_then([](int& x) { return plus_one(x); });
            BOOST_TEST(r);
            BOOST_TEST(*r == 11);
        }

        // and_then (error propagates)
        {
            Expected<int, E> e(unexpect, E("err"));
            auto r = e.and_then([](int& x) { return plus_one(x); });
            BOOST_TEST(!r);
            BOOST_TEST(r.error().failed());
        }

        // or_else (success path returns same value)
        {
            Expected<int, E> e(3);
            auto r = e.or_else([](E&) { return fail_with("should-not-run"); });
            BOOST_TEST(r);
            BOOST_TEST(*r == 3);
        }

        // or_else (error path produces alternate)
        {
            Expected<int, E> e(unexpect, E("oops"));
            auto r = e.or_else([](E&) { return Expected<int, E>(7); });
            BOOST_TEST(r);
            BOOST_TEST(*r == 7);
        }

        // transform maps the value
        {
            Expected<int, E> e(8);
            auto r = e.transform([](int& x) { return x * 2; });
            static_assert(std::is_same_v<decltype(r), Expected<int, E>>);
            BOOST_TEST(r);
            BOOST_TEST(*r == 16);
        }

        // transform_error maps the error
        {
            struct MyErr {
                std::string s;
            };
            Expected<int, E> e(unexpect, E("bad"));
            auto r = e.transform_error([](E& old) {
                return MyErr{ old.message() };
            });
            static_assert(std::is_same_v<decltype(r), Expected<int, MyErr>>);
            BOOST_TEST(!r);
            BOOST_TEST(r.error().s.find("bad") != std::string::npos);
        }

        // swap combinations
        {
            Expected<int, E> a(1);
            Expected<int, E> b(2);
            swap(a, b);
            BOOST_TEST(*a == 2);
            BOOST_TEST(*b == 1);

            Expected<int, E> c(unexpect, E("err"));
            swap(a, c);
            BOOST_TEST(!a);
            BOOST_TEST(c);
            BOOST_TEST(*c == 2);
        }

        // equality (value vs value, error vs error)
        {
            Expected<int, E> a(5), b(5), c(6);
            Expected<int, E> xe(unexpect, E("x")), ye(unexpect, E("x"));
            BOOST_TEST(a == b);
            BOOST_TEST(!(a == c));
            BOOST_TEST(xe == ye);
            BOOST_TEST(!(a == xe));
        }
    }

    void
    test_void_expected_basic()
    {
        using Ev = Error;

        // default engaged
        {
            Expected<void, Ev> e(std::in_place);
            BOOST_TEST(e);
            // operator* is a no-op with an assert, we won't call it here.
            e.value(); // should not throw
        }

        // unexpected
        {
            Expected<void, Ev> e(unexpect, Ev("boom"));
            BOOST_TEST(!e);
            BOOST_TEST(e.error().failed());
            auto er = e.error_or(Ev("alt"));
            BOOST_TEST(er.failed());
        }

        // copy/move assign between states
        {
            Expected<void, Ev> ok(std::in_place);
            Expected<void, Ev> err(unexpect, Ev("x"));

            ok = err; // ok -> err
            BOOST_TEST(!ok);

            err = Expected<void, Ev>(std::in_place); // err -> ok
            BOOST_TEST(err);
        }

        // emplace clears error
        {
            Expected<void, Ev> e(unexpect, Ev("no"));
            e.emplace();
            BOOST_TEST(e);
        }

        // and_then
        {
            Expected<void, Ev> e(std::in_place);
            auto r = e.and_then([] { return ok_void(); });
            BOOST_TEST(r);
        }
        {
            Expected<void, Ev> e(unexpect, Ev("n"));
            auto r = e.and_then([] { return ok_void(); });
            BOOST_TEST(!r);
        }

        // or_else
        {
            Expected<void, Ev> e(unexpect, Ev("nope"));
            auto r = e.or_else([](Ev&) { return ok_void(); });
            BOOST_TEST(r);
        }

        // transform to non-void
        {
            Expected<void, Ev> e(std::in_place);
            auto r = e.transform([] { return 17; });
            static_assert(std::is_same_v<decltype(r), Expected<int, Ev>>);
            BOOST_TEST(r);
            BOOST_TEST(*r == 17);
        }

        // transform_error
        {
            struct MyErr {
                std::string s;
            };
            Expected<void, Ev> e(unexpect, Ev("zzz"));
            auto r = e.transform_error([](Ev& old) {
                return MyErr{ old.message() };
            });
            static_assert(std::is_same_v<decltype(r), Expected<void, MyErr>>);
            BOOST_TEST(!r);
            BOOST_TEST(r.error().s.find("zzz") != std::string::npos);
        }

        // swap
        {
            Expected<void, Ev> a(std::in_place);
            Expected<void, Ev> b(unexpect, Ev("e"));
            swap(a, b);
            BOOST_TEST(!a);
            BOOST_TEST(b);
        }

        // equality
        {
            Expected<void, Ev> ok1(std::in_place), ok2(std::in_place);
            Expected<void, Ev> er1(unexpect, Ev("e1")), er2(unexpect, Ev("e1"));
            BOOST_TEST(ok1 == ok2);
            BOOST_TEST(er1 == er2);
            BOOST_TEST(!(ok1 == er1));
        }
    }

    void
    test_reference_expected_basic()
    {
        using E = Error;

        int x = 10;
        int y = 20;

        // bind to lvalue, arrow/deref/value
        {
            Expected<int&, E> er(std::in_place, x);
            BOOST_TEST(er);
            BOOST_TEST(&er.value() == &x);
            BOOST_TEST(*er == 10);

            er = y; // rebinding
            BOOST_TEST(&er.value() == &y);
            BOOST_TEST(*er == 20);
        }

        // construct from Expected<T,E>& (binds to contained lvalue)
        {
            Expected<int, E> ev(std::in_place, 42);
            Expected<int&, E> er(std::in_place, x);
            er = ev; // rebind to ev.value()
            BOOST_TEST(&er.value() == &ev.value());
            BOOST_TEST(*er == 42);
        }

        // value_or returns by value (copy)
        {
            Expected<int&, E> er(std::in_place, x);
            Expected<int&, E> bad(unexpect, E("err"));
            auto v1 = er.value_or(5);
            auto v2 = bad.value_or(5);
            static_assert(std::is_same_v<decltype(v1), int>);
            BOOST_TEST(v1 == 10);
            BOOST_TEST(v2 == 5);
        }

        // error transitions
        {
            Expected<int&, E> er(unexpect, E("e"));
            BOOST_TEST(!er);
            er = x; // rebind from error -> success
            BOOST_TEST(er);
            BOOST_TEST(&er.value() == &x);
        }

        // transform
        {
            Expected<int&, E> er(std::in_place, x);
            auto r = er.transform([](int& r) { return r * 3; });
            static_assert(std::is_same_v<decltype(r), Expected<int, E>>);
            BOOST_TEST(r);
            BOOST_TEST(*r == 30);
        }

        // and_then (success)
        {
            Expected<int&, E> er(std::in_place, x);
            auto r = er.and_then([](int& r) { return plus_one(r); });
            BOOST_TEST(r);
            BOOST_TEST(*r == 11);
        }

        // and_then (error propagates)
        {
            Expected<int&, E> er(unexpect, E("err"));
            auto r = er.and_then([](int& r) { return plus_one(r); });
            BOOST_TEST(!r);
        }

        // transform_error changes error type, preserves binding semantics on
        // success
        {
            struct MyErr {
                std::string s;
            };

            Expected<int&, E> ok(std::in_place, x);
            auto r1 = ok.transform_error([](E& e) {
                return MyErr{ e.message() };
            });
            static_assert(std::is_same_v<decltype(r1), Expected<int&, MyErr>>);
            BOOST_TEST(r1);
            BOOST_TEST(&r1.value() == &x);

            Expected<int&, E> bad(unexpect, E("xx"));
            auto r2 = bad.transform_error([](E& e) {
                return MyErr{ e.message() };
            });
            BOOST_TEST(!r2);
            BOOST_TEST(r2.error().s.find("xx") != std::string::npos);
        }

        // swap: value<->value and value<->error
        {
            Expected<int&, E> a(std::in_place, x);
            Expected<int&, E> b(std::in_place, y);
            swap(a, b);
            BOOST_TEST(&a.value() == &y);
            BOOST_TEST(&b.value() == &x);

            Expected<int&, E> c(unexpect, E("err"));
            swap(a, c);
            BOOST_TEST(!a);
            BOOST_TEST(c);
            BOOST_TEST(&c.value() == &y);
        }

        // equality (value vs value, error vs error)
        {
            Expected<int&, E> a(std::in_place, x);
            Expected<int&, E> b(std::in_place, x);
            Expected<int&, E> c(std::in_place, y);
            Expected<int&, E> xe(unexpect, E("e")), ye(unexpect, E("e"));

            BOOST_TEST(a == b);
            BOOST_TEST(!(a == c));
            BOOST_TEST(xe == ye);
            BOOST_TEST(!(a == xe));
        }

        // deleted constructs/assignments from temporaries (compile-time checks)
        {
            static_assert(!std::is_constructible_v<Expected<int&, E>, int&&>);
            static_assert(!std::is_assignable_v<Expected<int&, E>, int&&>);
            static_assert(!std::is_constructible_v<Expected<int&, E>, Expected<int, E>&&>);
            static_assert(!std::is_assignable_v<Expected<int&, E>, Expected<int, E>&&>);
        }
    }

    void
    run()
    {
        test_value_expected_basic();
        test_void_expected_basic();
        test_reference_expected_basic();
    }
};

TEST_SUITE(
    ExpectedTest,
    "clang.mrdocs.Expected");

} // mrdocs
} // clang
