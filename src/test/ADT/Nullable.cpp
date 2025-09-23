//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/ADT/Nullable.hpp>
#include <cmath>
#include <optional>
#include <string>
#include <vector>
#include <test_suite/test_suite.hpp>

namespace clang::mrdocs {

struct NullableTest {
    // Types for testing
    struct NoTraits {
        int v{};
    };

    enum class EUnknown {
        A,
        B,
        Unknown
    };
    enum class EUNKNOWN {
        A,
        B,
        UNKNOWN
    };
    enum class ENone {
        A,
        B,
        None
    };
    enum class ENONE {
        A,
        B,
        NONE
    };

    void
    test_concepts_and_detection()
    {
        // HasSentinel: pointers, nullptr_t, unsigned, floating, enums with
        // known nuller
        static_assert(HasSentinel<int*>);
        static_assert(HasSentinel<std::nullptr_t>);
        static_assert(HasSentinel<unsigned>);
        static_assert(HasSentinel<double>);
        static_assert(HasSentinel<EUnknown>);
        static_assert(HasSentinel<EUNKNOWN>);
        static_assert(HasSentinel<ENone>);
        static_assert(HasSentinel<ENONE>);

        // HasSentinel should NOT hold for std::string or NoTraits
        static_assert(!HasSentinel<std::string>);
        static_assert(!HasSentinel<NoTraits>);

        // ClearableEmpty: string, vector qualify
        static_assert(ClearableEmpty<std::string>);
        static_assert(ClearableEmpty<std::vector<int>>);

        // ClearableEmpty should NOT hold for primitive or pointer
        static_assert(!ClearableEmpty<int>);
        static_assert(!ClearableEmpty<int*>);

        // has_nullable_traits_v (the concept) should be true when either
        // sentinel or clearable-empty applies
        static_assert(has_nullable_traits_v<int*>);
        static_assert(has_nullable_traits_v<std::nullptr_t>);
        static_assert(has_nullable_traits_v<unsigned>);
        static_assert(has_nullable_traits_v<double>);
        static_assert(has_nullable_traits_v<EUnknown>);
        static_assert(has_nullable_traits_v<std::string>);
        static_assert(has_nullable_traits_v<std::vector<int>>);
        static_assert(!has_nullable_traits_v<NoTraits>);
    }

    void
    test_sentinel_traits_pointers()
    {
        int* p = nullptr;
        BOOST_TEST(sentinel_traits<int*>::is_sentinel(p));
        BOOST_TEST(sentinel_traits<int*>::sentinel() == nullptr);

        int x = 0;
        p = &x;
        BOOST_TEST(!sentinel_traits<int*>::is_sentinel(p));

        // nullable_traits uses sentinel for pointers
        BOOST_TEST(is_null<int*>(nullptr));
        int* q = &x;
        BOOST_TEST(!is_null(q));
        make_null(q);
        BOOST_TEST(is_null(q));
        auto n = null_of<int*>();
        BOOST_TEST(n == nullptr);
    }

    void
    test_sentinel_traits_nullptr_t()
    {
        std::nullptr_t z = nullptr;
        BOOST_TEST(sentinel_traits<std::nullptr_t>::is_sentinel(z));
        BOOST_TEST(sentinel_traits<std::nullptr_t>::sentinel() == nullptr);

        // nullable_traits via sentinel
        BOOST_TEST(is_null(z));
        auto n = null_of<std::nullptr_t>();
        (void) n; // always null
        std::nullptr_t t = nullptr;
        make_null(t);
        BOOST_TEST(is_null(t));
    }

    void
    test_sentinel_traits_unsigned()
    {
        using U = unsigned;
        U s = sentinel_traits<U>::sentinel();
        BOOST_TEST(s == static_cast<U>(-1));
        BOOST_TEST(sentinel_traits<U>::is_sentinel(s));
        BOOST_TEST(!sentinel_traits<U>::is_sentinel(static_cast<U>(0)));

        // nullable_traits via sentinel
        U v = 7u;
        BOOST_TEST(!is_null(v));
        make_null(v);
        BOOST_TEST(is_null(v));
        BOOST_TEST(sentinel_traits<U>::is_sentinel(v));
        BOOST_TEST(is_null(null_of<U>()));
    }

    void
    test_sentinel_traits_floating()
    {
        using F = double;
        F s = sentinel_traits<F>::sentinel();
        BOOST_TEST(std::isnan(s));
        BOOST_TEST(sentinel_traits<F>::is_sentinel(s));
        BOOST_TEST(!sentinel_traits<F>::is_sentinel(0.0));

        // nullable_traits via sentinel (NaN)
        F v = 0.5;
        BOOST_TEST(!is_null(v));
        make_null(v);
        BOOST_TEST(is_null(v));
        BOOST_TEST(std::isnan(null_of<F>()));
    }

    void
    test_sentinel_traits_enums_all_variants()
    {
        // EUnknown
        BOOST_TEST(sentinel_traits<EUnknown>::is_sentinel(EUnknown::Unknown));
        BOOST_TEST(sentinel_traits<EUnknown>::sentinel() == EUnknown::Unknown);
        EUnknown eu = EUnknown::A;
        BOOST_TEST(!is_null(eu));
        make_null(eu);
        BOOST_TEST(is_null(eu));
        BOOST_TEST(null_of<EUnknown>() == EUnknown::Unknown);

        // EUNKNOWN
        BOOST_TEST(sentinel_traits<EUNKNOWN>::is_sentinel(EUNKNOWN::UNKNOWN));
        BOOST_TEST(sentinel_traits<EUNKNOWN>::sentinel() == EUNKNOWN::UNKNOWN);

        // ENone
        BOOST_TEST(sentinel_traits<ENone>::is_sentinel(ENone::None));
        BOOST_TEST(sentinel_traits<ENone>::sentinel() == ENone::None);

        // ENONE
        BOOST_TEST(sentinel_traits<ENONE>::is_sentinel(ENONE::NONE));
        BOOST_TEST(sentinel_traits<ENONE>::sentinel() == ENONE::NONE);
    }

    void
    test_nullable_traits_clearable_empty_string()
    {
        std::string s;
        BOOST_TEST(has_nullable_traits_v<std::string>);
        BOOST_TEST(is_null(s)); // empty
        s = "abc";
        BOOST_TEST(!is_null(s));
        make_null(s); // clear()
        BOOST_TEST(is_null(s));

        auto z = null_of<std::string>(); // default empty
        BOOST_TEST(is_null(z));
    }

    void
    test_nullable_traits_clearable_empty_vector()
    {
        std::vector<int> v;
        BOOST_TEST(has_nullable_traits_v<std::vector<int>>);
        BOOST_TEST(is_null(v));
        v.push_back(1);
        BOOST_TEST(!is_null(v));
        make_null(v);
        BOOST_TEST(is_null(v));
        auto z = null_of<std::vector<int>>();
        BOOST_TEST(is_null(z));
    }

    void
    test_negative_no_traits()
    {
        // No traits ⇒ helpers are not available by constraints; we only assert
        // detection.
        static_assert(!has_nullable_traits_v<NoTraits>);
        BOOST_TEST(true);
    }

    void
    run()
    {
        test_concepts_and_detection();
        test_sentinel_traits_pointers();
        test_sentinel_traits_nullptr_t();
        test_sentinel_traits_unsigned();
        test_sentinel_traits_floating();
        test_sentinel_traits_enums_all_variants();
        test_nullable_traits_clearable_empty_string();
        test_nullable_traits_clearable_empty_vector();
        test_negative_no_traits();

        BOOST_TEST(true);
    }
};

TEST_SUITE(NullableTest, "clang.mrdocs.ADT.Nullable");

} // namespace clang::mrdocs
