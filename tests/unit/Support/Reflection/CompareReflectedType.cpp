//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/DocComment/Block/HeadingBlock.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Symbol/Location.hpp>
#include <mrdocs/Metadata/TArg.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/Reflection/CompareReflectedType.hpp>
#include <test_suite/test_suite.hpp>

namespace mrdocs {
namespace {

// ------------------------------------------------------------------
// Location: no bases, no enum members.
// ------------------------------------------------------------------

struct LocationCompare
{
    void test_equal()
    {
        Location a("f", "s", "p", 10, 5, false);
        Location b("f", "s", "p", 10, 5, false);
        BOOST_TEST(std::is_eq(a <=> b));
        BOOST_TEST(a == b);
    }

    void test_less()
    {
        Location a("f", "s", "p", 10, 5, false);
        Location b("f", "s", "p", 20, 5, false);
        BOOST_TEST(std::is_lt(a <=> b));
        BOOST_TEST(!(a == b));
    }

    void test_greater()
    {
        Location a("f", "s", "p", 20, 5, false);
        Location b("f", "s", "p", 10, 5, false);
        BOOST_TEST(std::is_gt(a <=> b));
    }

    void test_first_member_wins()
    {
        // ShortPath is the first described member;
        // a difference there must outweigh any later member.
        Location a("f", "a", "z", 99, 99, true);
        Location b("f", "b", "a",  1,  1, false);
        BOOST_TEST(std::is_lt(a <=> b));
    }
};

// ------------------------------------------------------------------
// TArg / TypeTArg: has bases (TArgCommonBase<>->TArg), has enum
// member (TArg::Kind).
// ------------------------------------------------------------------

struct TArgCompare
{
    void test_equal()
    {
        TypeTArg a;
        TypeTArg b;
        BOOST_TEST(std::is_eq(a.asTArg() <=> b.asTArg()));
        BOOST_TEST(a.asTArg() == b.asTArg());
    }

    void test_enum_difference()
    {
        // Directly tweak Kind to exercise the enum comparison path.
        TypeTArg a;
        TypeTArg b;
        const_cast<TArgKind&>(b.asTArg().Kind) = TArgKind::Template;
        BOOST_TEST(std::is_neq(a.asTArg() <=> b.asTArg()));
    }

    void test_base_short_circuits()
    {
        TypeTArg a;
        TypeTArg b;
        // Make the base (TArg) differ so the member loop is skipped.
        const_cast<TArgKind&>(a.asTArg().Kind) = TArgKind::Constant;
        // Even though derived fields are equal, the base difference
        // must determine the result.
        BOOST_TEST(std::is_neq(a <=> b));
    }
};

// ------------------------------------------------------------------
// doc::HeadingBlock: has two bases (BlockCommonBase, InlineContainer)
// and an own unsigned member (level).
// ------------------------------------------------------------------

struct HeadingCompare
{
    void test_equal()
    {
        doc::HeadingBlock a;
        doc::HeadingBlock b;
        BOOST_TEST(std::is_eq(a <=> b));
        BOOST_TEST(a == b);
    }

    void test_level_difference()
    {
        doc::HeadingBlock a;
        doc::HeadingBlock b;
        a.level = 2;
        b.level = 4;
        BOOST_TEST(std::is_lt(a <=> b));
        BOOST_TEST(!(a == b));
    }
};

// ------------------------------------------------------------------
// Polymorphic<TArg>, Polymorphic<Name>, Polymorphic<Type>.
// ------------------------------------------------------------------

struct PolymorphicCompare
{
    void test_polymorphic_targ_equal()
    {
        Polymorphic<TArg> a{TypeTArg{}};
        Polymorphic<TArg> b{TypeTArg{}};
        BOOST_TEST(std::is_eq(a <=> b));
        BOOST_TEST(a == b);
    }

    void test_polymorphic_targ_not_equal()
    {
        Polymorphic<TArg> a{TypeTArg{}};
        Polymorphic<TArg> b{ConstantTArg{}};
        BOOST_TEST(!(a == b));
    }

    void test_polymorphic_name_equal()
    {
        Polymorphic<Name> a{IdentifierName{}};
        Polymorphic<Name> b{IdentifierName{}};
        BOOST_TEST(std::is_eq(a <=> b));
        BOOST_TEST(a == b);
    }

    void test_polymorphic_name_not_equal()
    {
        IdentifierName n1;
        n1.Identifier = "a";
        IdentifierName n2;
        n2.Identifier = "b";
        Polymorphic<Name> a{std::move(n1)};
        Polymorphic<Name> b{std::move(n2)};
        BOOST_TEST(!(a == b));
    }

    void test_polymorphic_type_equal()
    {
        Polymorphic<Type> a{NamedType{}};
        Polymorphic<Type> b{NamedType{}};
        BOOST_TEST(std::is_eq(a <=> b));
        BOOST_TEST(a == b);
    }

    void test_polymorphic_type_not_equal()
    {
        Polymorphic<Type> a{NamedType{}};
        Polymorphic<Type> b{PointerType{}};
        BOOST_TEST(!(a == b));
    }
};

// ------------------------------------------------------------------

struct CompareReflectedTypeTest
{
    void run()
    {
        LocationCompare loc;
        loc.test_equal();
        loc.test_less();
        loc.test_greater();
        loc.test_first_member_wins();

        TArgCompare targ;
        targ.test_equal();
        targ.test_enum_difference();
        targ.test_base_short_circuits();

        HeadingCompare heading;
        heading.test_equal();
        heading.test_level_difference();

        PolymorphicCompare poly;
        poly.test_polymorphic_targ_equal();
        poly.test_polymorphic_targ_not_equal();
        poly.test_polymorphic_name_equal();
        poly.test_polymorphic_name_not_equal();
        poly.test_polymorphic_type_equal();
        poly.test_polymorphic_type_not_equal();
    }
};

} // (anon)

TEST_SUITE(
    CompareReflectedTypeTest,
    "clang.mrdocs.Support.CompareReflectedType");

} // mrdocs
