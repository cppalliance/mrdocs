//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/String.hpp>
#include <mrdocs/Metadata/Specifiers/NoexceptInfo.hpp>
#include <test_suite/test_suite.hpp>

namespace mrdocs {
namespace {

// Helpers to build the specs in one line per test.

NoexceptInfo
makeNoexcept(
    bool const implicit,
    NoexceptKind const kind,
    std::string operand = {})
{
    NoexceptInfo info;
    info.Implicit = implicit;
    info.Kind = kind;
    info.Operand = std::move(operand);
    return info;
}

// ------------------------------------------------------------------

struct NoexceptInfoTest
{
    // -- toString(NoexceptInfo) ----------------------------------

    void test_noexcept_implicit_is_skipped()
    {
        NoexceptInfo const info = makeNoexcept(true, NoexceptKind::True);
        BOOST_TEST(toString(info) == "");
    }

    void test_noexcept_implicit_shown_when_requested()
    {
        NoexceptInfo const info = makeNoexcept(true, NoexceptKind::True);
        BOOST_TEST(toString(info, false, true) == "noexcept");
    }

    void test_noexcept_dependent_empty_operand()
    {
        NoexceptInfo const info = makeNoexcept(false, NoexceptKind::Dependent);
        BOOST_TEST(toString(info) == "");
    }

    void test_noexcept_dependent_with_operand()
    {
        NoexceptInfo const info = makeNoexcept(
            false, NoexceptKind::Dependent, "sizeof(T) > 4");
        BOOST_TEST(toString(info) == "noexcept(sizeof(T) > 4)");
    }

    void test_noexcept_false_resolved_drops_operand()
    {
        NoexceptInfo const info = makeNoexcept(
            false, NoexceptKind::False, "false");
        BOOST_TEST(toString(info, true) == "");
    }

    void test_noexcept_false_with_operand()
    {
        NoexceptInfo const info = makeNoexcept(
            false, NoexceptKind::False, "false");
        BOOST_TEST(toString(info) == "noexcept(false)");
    }

    void test_noexcept_true_resolved_drops_operand()
    {
        NoexceptInfo const info = makeNoexcept(
            false, NoexceptKind::True, "true");
        BOOST_TEST(toString(info, true) == "noexcept");
    }

    void test_noexcept_true_empty_operand()
    {
        NoexceptInfo const info = makeNoexcept(false, NoexceptKind::True);
        BOOST_TEST(toString(info) == "noexcept");
    }

    void test_noexcept_true_with_operand()
    {
        NoexceptInfo const info = makeNoexcept(
            false, NoexceptKind::True, "true");
        BOOST_TEST(toString(info) == "noexcept(true)");
    }

    // -- runner --------------------------------------------------

    void run()
    {
        test_noexcept_implicit_is_skipped();
        test_noexcept_implicit_shown_when_requested();
        test_noexcept_dependent_empty_operand();
        test_noexcept_dependent_with_operand();
        test_noexcept_false_resolved_drops_operand();
        test_noexcept_false_with_operand();
        test_noexcept_true_resolved_drops_operand();
        test_noexcept_true_empty_operand();
        test_noexcept_true_with_operand();
    }
};

} // (unnamed)

TEST_SUITE(NoexceptInfoTest, "clang.mrdocs.Metadata.NoexceptInfo");

} // mrdocs
