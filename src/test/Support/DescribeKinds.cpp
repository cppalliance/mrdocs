//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/Describe.hpp>
#include <test_suite/test_suite.hpp>
#include <string>
#include <type_traits>

namespace mrdocs {
namespace {

// ------------------------------------------------------------------
// Fixture types
// ------------------------------------------------------------------
//
// Toy class hierarchies registered with the variadic and the
// BEGIN/END forms of `MRDOCS_DESCRIBE_KINDS`, plus one base with no
// kinds at all and one type that is never registered.

// Variadic form.
struct VBase {};
struct VFoo : VBase {};
struct VBar : VBase {};
struct VBaz : VBase {};

// BEGIN/END form.
struct BBase {};
struct BAlpha : BBase {};
struct BBeta : BBase {};

// Empty kind list (the base is registered, but no derived classes).
struct EBase {};

// Never registered.
struct NotRegistered {};

// Registrations live in the same (anonymous) namespace as the
// fixtures so ADL on the base type can find the descriptor
// function. The function has internal linkage, which is fine
// here: it's only consumed by `decltype` in this TU.

MRDOCS_DESCRIBE_KINDS(VBase, VFoo, VBar, VBaz)

MRDOCS_DESCRIBE_KINDS(EBase)

#define INFO(Name) MRDOCS_KIND_ENTRY(BBase, B##Name)
MRDOCS_DESCRIBE_KINDS_BEGIN(BBase)
INFO(Alpha)
INFO(Beta)
MRDOCS_DESCRIBE_KINDS_END(BBase)
#undef INFO

// ------------------------------------------------------------------
// has_describe_kinds
// ------------------------------------------------------------------

struct TraitTest
{
    void test_true_for_registered_variadic()
    {
        BOOST_TEST(describe::has_describe_kinds<VBase>::value);
    }

    void test_true_for_registered_begin_end()
    {
        BOOST_TEST(describe::has_describe_kinds<BBase>::value);
    }

    void test_true_for_empty_registration()
    {
        BOOST_TEST(describe::has_describe_kinds<EBase>::value);
    }

    void test_false_for_unregistered_class()
    {
        BOOST_TEST(!describe::has_describe_kinds<NotRegistered>::value);
    }
};

// ------------------------------------------------------------------
// describe_kinds and for_each
// ------------------------------------------------------------------

struct IterationTest
{
    void test_count_variadic()
    {
        int n = 0;
        describe::for_each(
            describe::describe_kinds<VBase>{},
            [&](auto) { ++n; });
        BOOST_TEST(n == 3);
    }

    void test_count_begin_end()
    {
        int n = 0;
        describe::for_each(
            describe::describe_kinds<BBase>{},
            [&](auto) { ++n; });
        BOOST_TEST(n == 2);
    }

    void test_count_empty()
    {
        int n = 0;
        describe::for_each(
            describe::describe_kinds<EBase>{},
            [&](auto) { ++n; });
        BOOST_TEST(n == 0);
    }

    void test_order_preserved_variadic()
    {
        // Each kind contributes a digit; we verify they arrive in
        // the order they were listed in the macro invocation.
        std::string seen;
        describe::for_each(
            describe::describe_kinds<VBase>{},
            [&](auto descriptor)
            {
                using D = typename
                    std::decay_t<decltype(descriptor)>::type;
                if constexpr (std::is_same_v<D, VFoo>)
                {
                    seen += '0';
                }
                else if constexpr (std::is_same_v<D, VBar>)
                {
                    seen += '1';
                }
                else if constexpr (std::is_same_v<D, VBaz>)
                {
                    seen += '2';
                }
            });
        BOOST_TEST(seen == std::string("012"));
    }

    void test_order_preserved_begin_end()
    {
        std::string seen;
        describe::for_each(
            describe::describe_kinds<BBase>{},
            [&](auto descriptor)
            {
                using D = typename
                    std::decay_t<decltype(descriptor)>::type;
                if constexpr (std::is_same_v<D, BAlpha>)
                {
                    seen += '0';
                }
                else if constexpr (std::is_same_v<D, BBeta>)
                {
                    seen += '1';
                }
            });
        BOOST_TEST(seen == std::string("01"));
    }
};

// ------------------------------------------------------------------

struct DescribeKindsTest
{
    void run()
    {
        TraitTest trait;
        trait.test_true_for_registered_variadic();
        trait.test_true_for_registered_begin_end();
        trait.test_true_for_empty_registration();
        trait.test_false_for_unregistered_class();

        IterationTest iter;
        iter.test_count_variadic();
        iter.test_count_begin_end();
        iter.test_count_empty();
        iter.test_order_preserved_variadic();
        iter.test_order_preserved_begin_end();
    }
};

} // namespace

TEST_SUITE(
    DescribeKindsTest,
    "clang.mrdocs.Support.DescribeKinds");

} // namespace mrdocs
