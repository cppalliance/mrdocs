//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// Minimal compile-time reflection for C++23.
//
// Provides MRDOCS_DESCRIBE_STRUCT and MRDOCS_DESCRIBE_ENUM macros
// to annotate types with compile-time member/base/enumerator
// descriptors. This is a simplified, header-only subset of
// Boost.Describe by Peter Dimov, stripped down to what MrDocs
// needs and adapted to C++23. It replaces both Boost.Describe
// and Boost.Mp11 as dependencies.
//
// Differences from Boost.Describe:
//   - C++23 only (no C++11/14 fallbacks)
//   - Public members only (no access-level modifiers or filtering)
//   - Own members only (no inherited member resolution)
//   - for_each via fold expressions (no Mp11 dependency)
//
// Original Boost.Describe:
//   Copyright 2020, 2021 Peter Dimov
//   Distributed under the Boost Software License, Version 1.0.
//   https://www.boost.org/LICENSE_1_0.txt

#ifndef MRDOCS_SUPPORT_DESCRIBE_HPP
#define MRDOCS_SUPPORT_DESCRIBE_HPP

#include <type_traits>

namespace mrdocs::describe {

// -------------------------------------------------------------------
// Type list
// -------------------------------------------------------------------

template<class... T>
struct list {};

/** Invoke f(T{}) for each type T in a descriptor list. */
template<class... T, class F>
constexpr void
for_each(list<T...>, F&& f)
{
    (static_cast<void>(f(T{})), ...);
}

// -------------------------------------------------------------------
// Member descriptors
// -------------------------------------------------------------------

template<class... T>
list<T...> member_descriptor_fn_impl(int, T...);

// -------------------------------------------------------------------
// Base class descriptors
// -------------------------------------------------------------------

template<class C, class B>
struct base_descriptor
{
    static_assert(std::is_base_of_v<B, C>,
        "A type listed as a base is not one");
    using type = B;
};

template<class C, class... Bs>
struct bases_descriptor_impl;

template<class C, class... Bs>
struct bases_descriptor_impl<C, list<Bs...>>
{
    using type = list<base_descriptor<C, Bs>...>;
};

// -------------------------------------------------------------------
// Enum descriptors
// -------------------------------------------------------------------

template<class... T>
list<T...> enum_descriptor_fn_impl(int, T...);

// -------------------------------------------------------------------
// Query aliases (ADL lookup)
// -------------------------------------------------------------------

template<class T>
using describe_members =
    decltype(mrdocs_member_descriptor_fn(static_cast<T**>(nullptr)));

template<class T>
using describe_bases =
    decltype(mrdocs_base_descriptor_fn(static_cast<T**>(nullptr)));

template<class E>
using describe_enumerators =
    decltype(mrdocs_enum_descriptor_fn(static_cast<E**>(nullptr)));

// -------------------------------------------------------------------
// Type traits
// -------------------------------------------------------------------

// -------------------------------------------------------------------
// Descriptor detail types (used by macros below)
// -------------------------------------------------------------------

namespace detail {

template<auto P, auto N>
struct member_descriptor
{
    static constexpr auto pointer = P;
    static constexpr auto name    = N();
};

template<auto V, auto N>
struct enum_descriptor
{
    static constexpr auto value = V;
    static constexpr auto name  = N();
};

template<class, class = void>
struct has_describe_members_impl : std::false_type {};

template<class T>
struct has_describe_members_impl<T,
    std::void_t<describe_members<T>>> : std::true_type {};

template<class, class = void>
struct has_describe_bases_impl : std::false_type {};

template<class T>
struct has_describe_bases_impl<T,
    std::void_t<describe_bases<T>>> : std::true_type {};

template<class, class = void>
struct has_describe_enumerators_impl : std::false_type {};

template<class E>
struct has_describe_enumerators_impl<E,
    std::void_t<describe_enumerators<E>>> : std::true_type {};

} // namespace detail

template<class T>
using has_describe_members = detail::has_describe_members_impl<T>;

template<class T>
using has_describe_bases = detail::has_describe_bases_impl<T>;

template<class E>
using has_describe_enumerators = detail::has_describe_enumerators_impl<E>;

} // namespace mrdocs::describe

// ===================================================================
// Preprocessing machinery
// ===================================================================

// --- Utilities -----------------------------------------------------

#define MRDOCS_PP_EXPAND(x) x

#define MRDOCS_PP_CAT(x, y) MRDOCS_PP_CAT_I(x, y)
#define MRDOCS_PP_CAT_I(x, ...) x ## __VA_ARGS__

// --- Empty-argument detection --------------------------------------

#define MRDOCS_PP_IS_PAREN_I(x)                                     \
    MRDOCS_PP_CAT(                                                  \
        MRDOCS_PP_IS_PAREN_I_, MRDOCS_PP_IS_PAREN_II x)
#define MRDOCS_PP_IS_PAREN_II(...) 0
#define MRDOCS_PP_IS_PAREN_I_0 1,
#define MRDOCS_PP_IS_PAREN_I_MRDOCS_PP_IS_PAREN_II 0,

#define MRDOCS_PP_FIRST(x)    MRDOCS_PP_FIRST_I(x)
#define MRDOCS_PP_FIRST_I(x, ...) x

#define MRDOCS_PP_IS_PAREN(x) \
    MRDOCS_PP_FIRST(MRDOCS_PP_IS_PAREN_I(x))

#define MRDOCS_PP_EMPTY

#define MRDOCS_PP_IS_EMPTY(x) \
    MRDOCS_PP_IS_EMPTY_I(                                           \
        MRDOCS_PP_IS_PAREN(x),                                      \
        MRDOCS_PP_IS_PAREN(x MRDOCS_PP_EMPTY ()))
#define MRDOCS_PP_IS_EMPTY_I(x, y)     MRDOCS_PP_IS_EMPTY_II(x, y)
#define MRDOCS_PP_IS_EMPTY_II(x, y)    MRDOCS_PP_IS_EMPTY_III(x, y)
#define MRDOCS_PP_IS_EMPTY_III(x, y)   MRDOCS_PP_IS_EMPTY_III_ ## x ## y
#define MRDOCS_PP_IS_EMPTY_III_00 0
#define MRDOCS_PP_IS_EMPTY_III_01 1
#define MRDOCS_PP_IS_EMPTY_III_10 0
#define MRDOCS_PP_IS_EMPTY_III_11 0

#define MRDOCS_PP_CALL(F, a, x) \
    MRDOCS_PP_CAT(MRDOCS_PP_CALL_I_, MRDOCS_PP_IS_EMPTY(x))(F, a, x)
#define MRDOCS_PP_CALL_I_0(F, a, x)  F(a, x)
#define MRDOCS_PP_CALL_I_1(F, a, x)

// --- For-each ------------------------------------------------------

#define MRDOCS_PP_FOR_EACH_0(F, a)
#define MRDOCS_PP_FOR_EACH_1(F, a, x)      MRDOCS_PP_CALL(F, a, x)
#define MRDOCS_PP_FOR_EACH_2(F, a, x, ...)  MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_1(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_3(F, a, x, ...)  MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_2(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_4(F, a, x, ...)  MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_3(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_5(F, a, x, ...)  MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_4(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_6(F, a, x, ...)  MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_5(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_7(F, a, x, ...)  MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_6(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_8(F, a, x, ...)  MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_7(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_9(F, a, x, ...)  MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_8(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_10(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_9(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_11(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_10(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_12(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_11(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_13(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_12(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_14(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_13(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_15(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_14(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_16(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_15(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_17(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_16(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_18(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_17(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_19(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_18(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_20(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_19(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_21(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_20(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_22(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_21(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_23(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_22(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_24(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_23(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_25(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_24(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_26(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_25(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_27(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_26(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_28(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_27(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_29(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_28(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_30(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_29(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_31(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_30(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_32(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_31(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_33(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_32(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_34(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_33(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_35(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_34(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_36(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_35(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_37(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_36(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_38(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_37(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_39(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_38(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_40(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_39(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_41(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_40(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_42(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_41(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_43(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_42(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_44(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_43(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_45(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_44(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_46(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_45(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_47(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_46(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_48(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_47(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_49(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_48(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_50(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_49(F, a, __VA_ARGS__))

// --- Count arguments -----------------------------------------------

#define MRDOCS_PP_FE_EXTRACT(                                       \
    _0,  _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9,                \
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19,               \
    _20, _21, _22, _23, _24, _25, _26, _27, _28, _29,               \
    _30, _31, _32, _33, _34, _35, _36, _37, _38, _39,               \
    _40, _41, _42, _43, _44, _45, _46, _47, _48, _49,               \
    _50, V, ...) V

#define MRDOCS_PP_FOR_EACH(F, ...)                                  \
    MRDOCS_PP_EXPAND(MRDOCS_PP_EXPAND(                              \
        MRDOCS_PP_FE_EXTRACT(__VA_ARGS__,                           \
            MRDOCS_PP_FOR_EACH_50,                                  \
            MRDOCS_PP_FOR_EACH_49,                                  \
            MRDOCS_PP_FOR_EACH_48,                                  \
            MRDOCS_PP_FOR_EACH_47,                                  \
            MRDOCS_PP_FOR_EACH_46,                                  \
            MRDOCS_PP_FOR_EACH_45,                                  \
            MRDOCS_PP_FOR_EACH_44,                                  \
            MRDOCS_PP_FOR_EACH_43,                                  \
            MRDOCS_PP_FOR_EACH_42,                                  \
            MRDOCS_PP_FOR_EACH_41,                                  \
            MRDOCS_PP_FOR_EACH_40,                                  \
            MRDOCS_PP_FOR_EACH_39,                                  \
            MRDOCS_PP_FOR_EACH_38,                                  \
            MRDOCS_PP_FOR_EACH_37,                                  \
            MRDOCS_PP_FOR_EACH_36,                                  \
            MRDOCS_PP_FOR_EACH_35,                                  \
            MRDOCS_PP_FOR_EACH_34,                                  \
            MRDOCS_PP_FOR_EACH_33,                                  \
            MRDOCS_PP_FOR_EACH_32,                                  \
            MRDOCS_PP_FOR_EACH_31,                                  \
            MRDOCS_PP_FOR_EACH_30,                                  \
            MRDOCS_PP_FOR_EACH_29,                                  \
            MRDOCS_PP_FOR_EACH_28,                                  \
            MRDOCS_PP_FOR_EACH_27,                                  \
            MRDOCS_PP_FOR_EACH_26,                                  \
            MRDOCS_PP_FOR_EACH_25,                                  \
            MRDOCS_PP_FOR_EACH_24,                                  \
            MRDOCS_PP_FOR_EACH_23,                                  \
            MRDOCS_PP_FOR_EACH_22,                                  \
            MRDOCS_PP_FOR_EACH_21,                                  \
            MRDOCS_PP_FOR_EACH_20,                                  \
            MRDOCS_PP_FOR_EACH_19,                                  \
            MRDOCS_PP_FOR_EACH_18,                                  \
            MRDOCS_PP_FOR_EACH_17,                                  \
            MRDOCS_PP_FOR_EACH_16,                                  \
            MRDOCS_PP_FOR_EACH_15,                                  \
            MRDOCS_PP_FOR_EACH_14,                                  \
            MRDOCS_PP_FOR_EACH_13,                                  \
            MRDOCS_PP_FOR_EACH_12,                                  \
            MRDOCS_PP_FOR_EACH_11,                                  \
            MRDOCS_PP_FOR_EACH_10,                                  \
            MRDOCS_PP_FOR_EACH_9,                                   \
            MRDOCS_PP_FOR_EACH_8,                                   \
            MRDOCS_PP_FOR_EACH_7,                                   \
            MRDOCS_PP_FOR_EACH_6,                                   \
            MRDOCS_PP_FOR_EACH_5,                                   \
            MRDOCS_PP_FOR_EACH_4,                                   \
            MRDOCS_PP_FOR_EACH_3,                                   \
            MRDOCS_PP_FOR_EACH_2,                                   \
            MRDOCS_PP_FOR_EACH_1,                                   \
            MRDOCS_PP_FOR_EACH_0))(F, __VA_ARGS__))

// ===================================================================
// Public macros
// ===================================================================

// --- MRDOCS_DESCRIBE_STRUCT ----------------------------------------

#define MRDOCS_MEMBER_IMPL(C, m) \
    , ::mrdocs::describe::detail::member_descriptor<                \
        &C::m, []{ return #m; }>{}

#define MRDOCS_PP_UNPACK(...) __VA_ARGS__

#define MRDOCS_DESCRIBE_BASES(C, ...)                               \
    [[maybe_unused]]                                                \
    typename ::mrdocs::describe::bases_descriptor_impl<             \
        C, ::mrdocs::describe::list<__VA_ARGS__>>::type             \
    mrdocs_base_descriptor_fn(C**);

#define MRDOCS_DESCRIBE_MEMBERS(C, ...)                             \
    [[maybe_unused]]                                                \
    decltype(                                                       \
        ::mrdocs::describe::member_descriptor_fn_impl(              \
            0 __VA_OPT__(MRDOCS_PP_FOR_EACH(                        \
                MRDOCS_MEMBER_IMPL, C, __VA_ARGS__))))              \
    mrdocs_member_descriptor_fn(C**);

#define MRDOCS_DESCRIBE_STRUCT(C, Bases, Members)                   \
    static_assert(                                                  \
        std::is_class_v<C> || std::is_union_v<C>,                   \
        "MRDOCS_DESCRIBE_STRUCT should only be used with "          \
        "class types");                                             \
    MRDOCS_DESCRIBE_BASES(C, MRDOCS_PP_UNPACK Bases)                \
    MRDOCS_DESCRIBE_MEMBERS(C, MRDOCS_PP_UNPACK Members)

// --- MRDOCS_DESCRIBE_ENUM ------------------------------------------

#define MRDOCS_ENUM_ENTRY(E, e)                                     \
    , ::mrdocs::describe::detail::enum_descriptor<                  \
        E::e, []{ return #e; }>{}

#define MRDOCS_DESCRIBE_ENUM(E, ...)                                \
    static_assert(std::is_enum_v<E>,                                \
        "MRDOCS_DESCRIBE_ENUM should only be used with enums");     \
    [[maybe_unused]]                                                \
    decltype(                                                       \
        ::mrdocs::describe::enum_descriptor_fn_impl(0               \
            MRDOCS_PP_FOR_EACH(                                     \
                MRDOCS_ENUM_ENTRY, E, __VA_ARGS__)                  \
        )) mrdocs_enum_descriptor_fn(E**);

#endif // MRDOCS_SUPPORT_DESCRIBE_HPP
