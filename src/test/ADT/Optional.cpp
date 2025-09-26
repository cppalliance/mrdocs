//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Metadata/Info/Source.hpp>
#include <optional>
#include <type_traits>
#include <test_suite/test_suite.hpp>

namespace clang::mrdocs {

struct OptionalTest {
    // A fallback-only payload type (no sentinel, no empty()/clear()).
    struct NoTraits {
        int x = 0;
        int
        forty_two() const
        {
            return 42;
        }
        auto
        operator<=>(NoTraits const&) const
            = default;
    };

    // An enum that should pick up sentinel via Unknown
    enum class Color {
        Red,
        Green,
        Unknown
    };

    void
    test_nullable_string()
    {
        static_assert(
            has_nullable_traits_v<std::string>,
            "string should use nullable_traits");
        Optional<std::string> o;
        BOOST_TEST(!o.has_value()); // default-null via empty()

        o.emplace("hi");
        BOOST_TEST(o.has_value());
        BOOST_TEST(*o == "hi");
        BOOST_TEST(o->size() == 2u);

        // value() overloads
        {
            std::string& lref = o.value();
            BOOST_TEST(&lref == &*o);

            std::string const& clref = std::as_const(o).value();
            BOOST_TEST(&clref == &*std::as_const(o));

            std::string moved = std::move(o).value(); // value() &&
            BOOST_TEST(moved == "hi");
        }

        // assign null
        o = nullptr;
        BOOST_TEST(!o.has_value());

        // reset()
        o.reset();
        BOOST_TEST(!o.has_value());

        // construct from value
        Optional<std::string> o2{ "abc" };
        BOOST_TEST(o2.has_value());
        BOOST_TEST(o2.value() == "abc");

        // comparisons (spaceship defaults)
        Optional<std::string> a{ "abc" }, b{ "abc" }, c{ "abd" };
        BOOST_TEST(a == b);
        BOOST_TEST(a < c);
    }

    void
    test_nullable_unsigned()
    {
        static_assert(
            has_nullable_traits_v<unsigned>,
            "unsigned should use sentinel -1");
        Optional<unsigned> id; // sentinel == max
        BOOST_TEST(!id.has_value());

        id = 7u;
        BOOST_TEST(id.has_value());
        BOOST_TEST(*id == 7u);

        id = static_cast<unsigned>(-1); // assign sentinel -> null
        BOOST_TEST(!id.has_value());

        id.reset();
        BOOST_TEST(!id.has_value());

        id.emplace(42u);
        BOOST_TEST(id.has_value());
        BOOST_TEST(id.value() == 42u);
    }

    void
    test_nullable_double()
    {
        static_assert(
            has_nullable_traits_v<double>,
            "double should use NaN sentinel");
        Optional<double> dop; // NaN -> null
        BOOST_TEST(!dop.has_value());

        dop = 0.0;
        BOOST_TEST(dop.has_value());
        BOOST_TEST(*dop == 0.0);

        dop = nullptr; // back to null
        BOOST_TEST(!dop.has_value());
    }

    void
    test_nullable_enum()
    {
        static_assert(
            has_nullable_traits_v<Color>,
            "enum with Unknown should have sentinel");
        Optional<Color> c;
        BOOST_TEST(!c.has_value()); // default to Unknown sentinel

        c = Color::Red;
        BOOST_TEST(c.has_value());
        BOOST_TEST(*c == Color::Red);

        c.reset();
        BOOST_TEST(!c.has_value());
    }

    void
    test_nullable_location()
    {
        // Location is nullable when ShortPath is empty (trait specialization
        // provided by user)
        static_assert(
            has_nullable_traits_v<Location>,
            "Location should use nullable_traits");
        Optional<Location> loc;
        BOOST_TEST(!loc.has_value()); // default has empty ShortPath

        Location L{ "full.cpp", "short.cpp", "src.cpp", 10u, true };
        Optional<Location> a{ L };
        BOOST_TEST(a.has_value());
        BOOST_TEST(a->ShortPath == "short.cpp");
        BOOST_TEST(a->LineNumber == 10u);
        BOOST_TEST((*a).Documented == true);

        a = nullptr;
        BOOST_TEST(!a.has_value());
        // Invalid: BOOST_TEST(a.value().ShortPath.empty());
    }

    void
    test_fallback_notraits()
    {
        static_assert(
            !has_nullable_traits_v<NoTraits>,
            "NoTraits must fall back to Optional");
        Optional<NoTraits> o;
        BOOST_TEST(!o.has_value()); // default disengaged

        o.emplace(NoTraits{ 7 });
        BOOST_TEST(o.has_value());
        BOOST_TEST((*o).x == 7);
        BOOST_TEST(o->forty_two() == 42);

        // copy / move
        Optional<NoTraits> copy = o;
        BOOST_TEST(copy.has_value());
        BOOST_TEST(copy->x == 7);

        Optional<NoTraits> moved = std::move(o);
        BOOST_TEST(moved.has_value());
        BOOST_TEST(moved->x == 7);

        // assign value
        o = NoTraits{ 9 };
        BOOST_TEST(o.has_value());
        BOOST_TEST(o->x == 9);

        // value() overloads
        {
            NoTraits& lref = o.value();
            BOOST_TEST(&lref == &*o);

            NoTraits const& clref = std::as_const(o).value();
            BOOST_TEST(&clref == &*std::as_const(o));

            NoTraits moved_val = std::move(o).value();
            BOOST_TEST(moved_val.x == 9);
        }

        // reset and null-assign
        o.reset();
        BOOST_TEST(!o.has_value());
        o = nullptr;
        BOOST_TEST(!o.has_value());

        // comparisons
        Optional<NoTraits> a, b;
        BOOST_TEST(a == b); // both disengaged
        a.emplace(NoTraits{ 1 });
        b.emplace(NoTraits{ 2 });
        BOOST_TEST(a != b);
        BOOST_TEST(a < b);
    }

    void
    test_reference_optional()
    {
        static_assert(detail::isOptionalV<Optional<int&>>);

        int a = 1;
        int b = 2;
        int c = 3;

        Optional<int&> r = a;
        BOOST_TEST(r.has_value());
        BOOST_TEST(&*r == &a);
        BOOST_TEST(r.value() == 1);

        struct S {
            int v{ 0 };

            int
            inc()
            {
                return ++v;
            }
        };
        S s{ 7 };
        Optional<S&> rs = s;
        BOOST_TEST(rs->inc() == 8);
        BOOST_TEST((*rs).v == 8);

        r = b;
        BOOST_TEST(&*r == &b);
        BOOST_TEST(*r == 2);

        // assign from Optional<int&>
        Optional<int&> rr = a;
        rr = r;
        BOOST_TEST(&*rr == &b);

        // construct from Optional<int&>
        Optional<int&> rr2{ r };
        BOOST_TEST(&*rr2 == &b);

        // Rebinding from Optional<int>
        Optional<int> vo = 42;
        rr = vo; // rebind to the referent inside vo
        BOOST_TEST(&*rr == &*vo);
        BOOST_TEST(*rr == *vo);

        // rebind to different lvalue
        rr2 = c;
        BOOST_TEST(&*rr2 == &c);
        BOOST_TEST(*rr2 == 3);

        rr2.reset();
        BOOST_TEST(!rr2.has_value());
        rr2 = a;
        BOOST_TEST(&*rr2 == &a);

        Optional<int&> copy = rr2;
        BOOST_TEST(&*copy == &a);
        Optional<int&> moved = std::move(rr2);
        BOOST_TEST(&*moved == &a);

        Optional<int&> rx = b;
        Optional<int&> ry = c;
        rx.swap(ry);
        BOOST_TEST(&*rx == &c);
        BOOST_TEST(&*ry == &b);

        Optional<int&> r1 = a;
        Optional<int&> r2 = b;
        BOOST_TEST(r1 != r2);
        BOOST_TEST((r1 < r2) == (a < b));

        Optional<int&> rn;
        BOOST_TEST(rn == std::nullopt);
        BOOST_TEST((r1 <=> std::nullopt) == std::strong_ordering::greater);
        BOOST_TEST((rn <=> std::nullopt) == std::strong_ordering::equal);

        int z = 9;
        Optional<int&> rz = z;
        int& zref = std::move(rz).value();
        BOOST_TEST(&zref == &z);

        static_assert(!std::is_constructible_v<Optional<int&>, int&&>);
        static_assert(!std::is_constructible_v<Optional<int&>, int const&&>);
        static_assert(!std::is_constructible_v<Optional<int&>, Optional<int>&&>);
        static_assert(!std::is_constructible_v<Optional<int&>, Optional<int> const&&>);
        static_assert(!std::is_assignable_v<Optional<int&>, int&&>);
        static_assert(!std::is_assignable_v<Optional<int&>, int const&&>);

        static_assert(std::is_constructible_v<Optional<int&>, int&>);
        static_assert(std::is_assignable_v<Optional<int&>, int&>);
        static_assert(std::is_constructible_v<Optional<int&>, Optional<int&>&>);
        static_assert(std::is_constructible_v<Optional<int&>, Optional<int&> const&>);
        static_assert(std::is_assignable_v<Optional<int&>, Optional<int&>&>);
        static_assert(std::is_assignable_v<Optional<int&>, Optional<int&> const&>);

        Optional<int> on = 1;
        Optional<int&> orf = a;
        BOOST_TEST(orf == on);
        a = 5;
        BOOST_TEST(orf != on);
    }

    void
    run()
    {
        test_nullable_string();
        test_nullable_unsigned();
        test_nullable_double();
        test_nullable_enum();
        test_nullable_location();
        test_fallback_notraits();
        test_reference_optional();
    }
};

TEST_SUITE(OptionalTest, "clang.mrdocs.ADT.Optional");

} // namespace clang::mrdocs
