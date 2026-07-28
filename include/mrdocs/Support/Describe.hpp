//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SUPPORT_DESCRIBE_HPP
#define MRDOCS_SUPPORT_DESCRIBE_HPP

#include <mrdocs/Support/String.hpp>
#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

/** Compile-time reflection over structs, enums, and their bases.

    A small, self-contained reflection library inspired by Boost.Describe.
    Besides the MRDOCS_DESCRIBE_* macros, the public API includes a number
    of traits and queries we need for documenting types in Mr.Docs.
*/
namespace mrdocs::describe {

/** A compile-time list of descriptor types.

    This is the container returned by the `describe_members`, `describe_bases`
    and `describe_enumerators` queries and consumed by `for_each`. The concrete
    descriptor element types are unspecified; access them through `for_each`.
*/
template<class... T>
struct list {};

/** The reflected data members of `T`, as a `list` of member descriptors.

    Well-formed only when `T` was annotated with MRDOCS_DESCRIBE_STRUCT or
    MRDOCS_DESCRIBE_CLASS; use `has_describe_members` to test first.
*/
template<class T>
using describe_members =
    decltype(mrdocs_member_descriptor_fn(static_cast<T**>(nullptr)));

/** The reflected direct base classes of `T`, as a `list` of base descriptors.

    Well-formed only when `T` was annotated with MRDOCS_DESCRIBE_STRUCT or
    MRDOCS_DESCRIBE_CLASS; use `has_describe_bases` to test first.
*/
template<class T>
using describe_bases =
    decltype(mrdocs_base_descriptor_fn(static_cast<T**>(nullptr)));

/** The reflected enumerators of `E`, as a `list` of enumerator descriptors.

    Well-formed only when `E` was annotated with MRDOCS_DESCRIBE_ENUM; use
    `has_describe_enumerators` to test first.
*/
template<class E>
using describe_enumerators =
    decltype(mrdocs_enum_descriptor_fn(static_cast<E**>(nullptr)));

/** The concrete kinds registered for a polymorphic base `T`, as a `list` of
    kind descriptors (each exposing the derived type as its `type` alias).

    Well-formed only when `T` was annotated with MRDOCS_DESCRIBE_KINDS; use
    `has_describe_kinds` to test first. Use with `for_each` to dispatch over
    every concrete kind of `T`.
*/
template<class T>
using describe_kinds =
    decltype(mrdocs_kind_descriptor_fn(static_cast<T**>(nullptr)));

namespace detail {

// The MRDOCS_DESCRIBE_* macros make a type's members, bases, and enumerators
// discoverable by injecting an ADL function (mrdocs_member_descriptor_fn(T**),
// mrdocs_base_descriptor_fn(T**), etc) whose return type is the
// descriptor list. The *_fn_impl helpers below only build those list<...>
// return types.
//
// ADL is used, rather than specializing a trait in this detail namespace,
// because the descriptor has to be found from T alone, and a function in T's
// own namespace works for both ways of describing a type: a namespace-scope
// MRDOCS_DESCRIBE_STRUCT written after the type, and an in-class hidden friend
// from MRDOCS_DESCRIBE_CLASS (the only way to describe a class template in
// place). A trait specialization would instead have to be declared from a
// namespace enclosing this one, which a described type in a sibling namespace
// (e.g. mrdocs::doc) cannot do.
//
// The trade-off is that the injected functions live in the described type's
// own namespace, not here in detail, so a project documenting its own API must
// exclude them by name (the `**mrdocs_*_descriptor_fn` rule in docs/mrdocs.yml);
// a detail-namespace rule cannot reach them.

template<class... T>
list<T...> member_descriptor_fn_impl(int, T...);

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

template<class... T>
list<T...> enum_descriptor_fn_impl(int, T...);

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

// Descriptor for one concrete kind of a polymorphic base: it checks that `D`
// derives from `C` and exposes the derived type as its `type` alias.
template<class C, class D>
struct kind_descriptor
{
    static_assert(std::is_base_of_v<C, D>,
        "A type listed as a kind is not actually derived from C");
    using type = D;
};

template<class... T>
list<T...> kind_descriptor_fn_impl(int, T...);

template<class, class = void>
struct has_describe_kinds_impl : std::false_type {};

template<class T>
struct has_describe_kinds_impl<T,
    std::void_t<describe_kinds<T>>> : std::true_type {};

} // namespace detail

/** A trait that is true when `T` has reflected members. */
template<class T>
using has_describe_members = detail::has_describe_members_impl<T>;

/** A trait that is true when `T` has reflected base classes. */
template<class T>
using has_describe_bases = detail::has_describe_bases_impl<T>;

/** A trait that is true when `E` has reflected enumerators. */
template<class E>
using has_describe_enumerators = detail::has_describe_enumerators_impl<E>;

/** A trait that is true when polymorphic base `T` has its kinds registered
    (via MRDOCS_DESCRIBE_KINDS). */
template<class T>
using has_describe_kinds = detail::has_describe_kinds_impl<T>;

/** Whether `E` designates an enumerator as its undefined (empty) state. */
template<class E>
concept has_undefined_enumerator =
requires { mrdocs_undefined_descriptor_fn(static_cast<E**>(nullptr)); };

/** The enumerator of `E` that stands for the undefined (empty) state.

    Valid only when `has_undefined_enumerator<E>`. `toString` renders this value
    as the empty string, and generators treat a field holding it as absent (an
    empty optional). Declared with MRDOCS_DESCRIBE_ENUM_UNDEFINED.
*/
template<class E>
requires has_undefined_enumerator<E>
inline constexpr E undefined_enumerator =
    mrdocs_undefined_descriptor_fn(static_cast<E**>(nullptr));

/** Satisfied when `T` has reflected members.

    True for any type annotated with MRDOCS_DESCRIBE_STRUCT or
    MRDOCS_DESCRIBE_CLASS. Reads better than, and constrains templates on,
    `has_describe_members<T>::value`.
*/
template<class T>
concept described = has_describe_members<T>::value || has_describe_enumerators<T>::value;

/** Invoke `f` with each descriptor in a descriptor list.

    @param f A callable invoked once per element, as `f(D{})`, where `D` is the
        descriptor type. Member descriptors expose `pointer` and `name`,
        enumerator descriptors expose `value` and `name`, and base descriptors
        expose a `type` alias.
*/
template<class... T, class F>
constexpr void
for_each(list<T...>, F&& f)
{
    (static_cast<void>(f(T{})), ...);
}

/** Invoke `f` with each reflected member descriptor of `T`, inherited ones
    included: base-class members first (recursively), then `T`'s own members.

    Each descriptor exposes `pointer` and `name`. A descriptor for a member
    inherited from base `B` carries a `B::*` pointer, which still applies to a
    `T` object directly (`obj.*d.pointer`), so callers need not walk the base
    hierarchy themselves.

    @param f A callable invoked once per member descriptor, as `f(d)`.
*/
template<described T, class F>
constexpr void
for_each_member(F&& f)
{
    if constexpr (has_describe_bases<T>::value)
    {
        for_each(describe_bases<T>{}, [&](auto d) {
            for_each_member<typename std::decay_t<decltype(d)>::type>(f);
        });
    }
    if constexpr (has_describe_members<T>::value)
    {
        for_each(describe_members<T>{}, [&](auto d) { f(d); });
    }
}

/** The number of reflected members of `T`, counting inherited ones.

    @return The member count across `T` and its reflected base classes.
*/
template<class T>
consteval std::size_t
describedMemberCount()
{
    std::size_t n = 0;
    if constexpr (described<T>)
    {
        for_each_member<T>([&](auto) { ++n; });
    }
    return n;
}

namespace detail {
template<class L> struct describe_front;
template<class D0, class... Ds>
struct describe_front<list<D0, Ds...>> { using type = D0; };
} // namespace detail

/** The member-pointer type shared by `T`'s reflected members.

    Well-formed only when `T` has at least one own reflected member
    and all its members share a single type. That homogeneity is
    what lets the members be addressed by a runtime index (see
    @ref memberPointers).
*/
template<class T>
using member_pointer_t = std::remove_cvref_t<
    decltype(detail::describe_front<describe_members<T>>::type::pointer)>;

/** Pointers to every reflected member of `T`, in reflection order.

    Complements @ref describedMemberCount by giving O(1) access to
    the i-th described member: `t.*memberPointers<T>()[i]`. Requires
    the members to be homogeneous (a single type), so a plain array
    indexable at runtime can hold them. Use it to iterate a struct's
    members generically instead of hand-writing a per-member switch.

    @return An array with one pointer per reflected member of `T`.
*/
template<class T>
consteval std::array<member_pointer_t<T>, describedMemberCount<T>()>
memberPointers()
{
    std::array<member_pointer_t<T>, describedMemberCount<T>()> ptrs{};
    std::size_t i = 0;
    for_each_member<T>([&](auto d) { ptrs[i++] = d.pointer; });
    return ptrs;
}

/** Whether every reflected member of `T` (inherited ones included) is an
    undescribed string, i.e. a value convertible to `std::string_view`.

    @return `true` when every reflected member is such a string, including
        vacuously when `T` has no members.
*/
template<class T>
consteval bool
describedMembersAllText()
{
    bool allText = true;
    if constexpr (described<T>)
    {
        for_each_member<T>([&](auto d) {
            using M = std::remove_cvref_t<decltype(std::declval<T const&>().*d.pointer)>;
            allText = allText && !described<M> &&
                      std::convertible_to<M, std::string_view>;
        });
    }
    return allText;
}

// --- Enumerator <-> string -----------------------------------------

namespace detail {

// The kebab-cased name of a single enumerator value V, materialized into
// static storage so a string_view over it stays valid at run time.

template <auto V>
consteval std::size_t
enumeratorKebabSize()
{
    std::string_view name;
    for_each(describe_enumerators<decltype(V)>{},
        [&](auto const& D) { if (D.value == V) { name = D.name; } });
    return toKebabCase(name).size();
}

template <auto V>
consteval auto
enumeratorKebabArray()
{
    std::array<char, enumeratorKebabSize<V>()> arr{};
    std::string_view name;
    for_each(describe_enumerators<decltype(V)>{},
        [&](auto const& D) { if (D.value == V) { name = D.name; } });
    std::string const kebab = toKebabCase(name);
    for (std::size_t i = 0; i < arr.size(); ++i) { arr[i] = kebab[i]; }
    return arr;
}

template <auto V>
inline constexpr auto enumeratorKebabStorage = enumeratorKebabArray<V>();

template <auto V>
inline constexpr std::string_view enumeratorKebab{
    enumeratorKebabStorage<V>.data(), enumeratorKebabStorage<V>.size()};

// Find the descriptor for enumerator `e` and return its name, recursing over
// the descriptor list. Every branch returns either a view over static storage
// (a string literal for the raw name, a static array for the kebab name) or an
// empty view, never a local or a parameter, so escape analysis has nothing to
// flag -- returning the match directly, rather than assigning a local inside a
// lambda, is what keeps it that way.

template <class E>
constexpr std::string_view
enumRawName(E, list<>) noexcept
{
    return {};
}

template <class E, class D0, class... Ds>
constexpr std::string_view
enumRawName(E e, list<D0, Ds...>) noexcept
{
    if (D0::value == e)
    {
        return D0::name;
    }
    return enumRawName(e, list<Ds...>{});
}

template <class E>
constexpr std::string_view
enumKebabName(E, list<>) noexcept
{
    return {};
}

template <class E, class D0, class... Ds>
constexpr std::string_view
enumKebabName(E e, list<D0, Ds...>) noexcept
{
    if (D0::value == e)
    {
        return enumeratorKebab<D0::value>;
    }
    return enumKebabName(e, list<Ds...>{});
}

} // namespace detail

/** Return the name of enumerator `e` exactly as it was declared.

    No case transformation is applied; the kebab-case form is produced by
    @ref mrdocs::toString instead.

    @param e The enumerator to convert.
    @return The enumerator's declared name; the empty string for the undefined
        state (see MRDOCS_DESCRIBE_ENUM_UNDEFINED) or an unrecognized value.
*/
template <described E>
requires std::is_enum_v<E>
constexpr std::string_view
enum_to_string(E e) noexcept
{
    if constexpr (has_undefined_enumerator<E>)
    {
        if (e == undefined_enumerator<E>)
        {
            return {};
        }
    }
    return detail::enumRawName(e, describe_enumerators<E>{});
}

/** Parse a declared enumerator name into an enumerator.

    Inverse of @ref enum_to_string: `name` is matched against the enumerator
    names exactly as declared.

    @param name The declared name; an empty name selects the undefined state
        when the enum has one (see MRDOCS_DESCRIBE_ENUM_UNDEFINED).
    @param e Set to the matching enumerator on success.
    @return `true` if `name` matched an enumerator, `false` otherwise.
*/
template <described E>
requires std::is_enum_v<E>
constexpr bool
enum_from_string(std::string_view name, E& e) noexcept
{
    if constexpr (has_undefined_enumerator<E>)
    {
        if (name.empty())
        {
            e = undefined_enumerator<E>;
            return true;
        }
    }
    bool found = false;
    for_each(describe_enumerators<E>{}, [&](auto const& D) {
        if (!found && std::string_view(D.name) == name)
        {
            e = D.value;
            found = true;
        }
    });
    return found;
}

} // namespace mrdocs::describe

namespace mrdocs {

/** Convert a described enumerator to its kebab-case string form.

    The kebab-case name of `e`, or the empty string for the enum's undefined
    state (see MRDOCS_DESCRIBE_ENUM_UNDEFINED). The raw declared name is
    available from @ref describe::enum_to_string.

    @param e The enumerator to convert.
    @return A view over static storage holding the enumerator's kebab-case name.
*/
template <class E>
    requires describe::has_describe_enumerators<E>::value
constexpr std::string_view
toString(E e) noexcept
{
    if constexpr (describe::has_undefined_enumerator<E>)
    {
        if (e == describe::undefined_enumerator<E>)
        {
            return {};
        }
    }
    return describe::detail::enumKebabName(
        e, describe::describe_enumerators<E>{});
}

} // namespace mrdocs

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
    typename ::mrdocs::describe::detail::bases_descriptor_impl<             \
        C, ::mrdocs::describe::list<__VA_ARGS__>>::type             \
    mrdocs_base_descriptor_fn(C**);

#define MRDOCS_DESCRIBE_MEMBERS(C, ...)                             \
    [[maybe_unused]]                                                \
    decltype(                                                       \
        ::mrdocs::describe::detail::member_descriptor_fn_impl(              \
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

// --- MRDOCS_DESCRIBE_CLASS ------------------------------------------
//
// Like MRDOCS_DESCRIBE_STRUCT but placed INSIDE a class definition.
// Uses friend declarations so the descriptor functions are declared
// in the enclosing namespace. This variant supports class templates.
//
// The friends are only ever read through `decltype`; no caller invokes them.
// They carry an inline `{ return {}; }` body rather than staying pure
// declarations because a hidden friend of a class-template instantiation can
// end up with internal linkage, and GCC then reports it as "declared static
// but never defined" (-Wunused-function). Defining it silences that; the body
// is never ODR-used. This matches MRDOCS_DESCRIBE_KINDS below.

#define MRDOCS_DESCRIBE_FRIEND_BASES(C, ...)                        \
    friend                                                          \
    typename ::mrdocs::describe::detail::bases_descriptor_impl<             \
        C, ::mrdocs::describe::list<__VA_ARGS__>>::type             \
    mrdocs_base_descriptor_fn(C**) { return {}; }

#define MRDOCS_DESCRIBE_FRIEND_MEMBERS(C, ...)                      \
    friend                                                          \
    decltype(                                                       \
        ::mrdocs::describe::detail::member_descriptor_fn_impl(              \
            0 __VA_OPT__(MRDOCS_PP_FOR_EACH(                        \
                MRDOCS_MEMBER_IMPL, C, __VA_ARGS__))))              \
    mrdocs_member_descriptor_fn(C**) { return {}; }

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns that each template instantiation declares a separate
// non-template friend function (-Wnon-template-friend). That is
// exactly the intended behaviour: every instantiation of the
// enclosing class template gets its own descriptor overload.
#define MRDOCS_DESCRIBE_CLASS(C, Bases, Members)                    \
    _Pragma("GCC diagnostic push")                                  \
    _Pragma("GCC diagnostic ignored \"-Wnon-template-friend\"")     \
    MRDOCS_DESCRIBE_FRIEND_BASES(C, MRDOCS_PP_UNPACK Bases)         \
    MRDOCS_DESCRIBE_FRIEND_MEMBERS(C, MRDOCS_PP_UNPACK Members)     \
    _Pragma("GCC diagnostic pop")
#else
#define MRDOCS_DESCRIBE_CLASS(C, Bases, Members)                    \
    MRDOCS_DESCRIBE_FRIEND_BASES(C, MRDOCS_PP_UNPACK Bases)         \
    MRDOCS_DESCRIBE_FRIEND_MEMBERS(C, MRDOCS_PP_UNPACK Members)
#endif

// --- MRDOCS_DESCRIBE_ENUM ------------------------------------------

#define MRDOCS_ENUM_ENTRY(E, e)                                     \
    , ::mrdocs::describe::detail::enum_descriptor<                  \
        E::e, []{ return #e; }>{}

#define MRDOCS_DESCRIBE_ENUM(E, ...)                                \
    static_assert(std::is_enum_v<E>,                                \
        "MRDOCS_DESCRIBE_ENUM should only be used with enums");     \
    [[maybe_unused]]                                                \
    decltype(                                                       \
        ::mrdocs::describe::detail::enum_descriptor_fn_impl(0               \
            MRDOCS_PP_FOR_EACH(                                     \
                MRDOCS_ENUM_ENTRY, E, __VA_ARGS__)                  \
        )) mrdocs_enum_descriptor_fn(E**);

// --- MRDOCS_DESCRIBE_ENUM from an X-macro (.inc) list --------------
//
// When an enum's enumerators already live in an X-macro `.inc` file (one
// `INFO(Name)` per enumerator, the same file used to define the enum itself),
// the describe metadata can be generated from that single source of truth
// instead of repeating the list in MRDOCS_DESCRIBE_ENUM. Bracket the include
// with these two macros and point `INFO` at MRDOCS_ENUM_ENTRY:
//
//     MRDOCS_DESCRIBE_ENUM_BEGIN(BlockKind)
//     #define INFO(Name) MRDOCS_ENUM_ENTRY(BlockKind, Name)
//     #include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
//     MRDOCS_DESCRIBE_ENUM_END(BlockKind)
//
// The `.inc` file `#undef`s INFO itself, so no cleanup line is needed.

#define MRDOCS_DESCRIBE_ENUM_BEGIN(E)                               \
    static_assert(std::is_enum_v<E>,                                \
        "MRDOCS_DESCRIBE_ENUM should only be used with enums");     \
    [[maybe_unused]]                                                \
    decltype(::mrdocs::describe::detail::enum_descriptor_fn_impl(0

#define MRDOCS_DESCRIBE_ENUM_END(E)                                 \
    )) mrdocs_enum_descriptor_fn(E**);

// --- MRDOCS_DESCRIBE_ENUM_UNDEFINED --------------------------------
//
// Marks E::U as E's undefined (empty) state. See has_undefined_enumerator /
// undefined_enumerator. Place at namespace scope after MRDOCS_DESCRIBE_ENUM(E).

#define MRDOCS_DESCRIBE_ENUM_UNDEFINED(E, U)                         \
    [[maybe_unused]]                                                 \
    inline constexpr E                                              \
    mrdocs_undefined_descriptor_fn(E**) noexcept { return E::U; }

// --- MRDOCS_DESCRIBE_KINDS -----------------------------------------
//
// Register a polymorphic base `C` together with the closed set of its concrete
// derived classes ("kinds"). Generic code iterates the result with for_each:
//
//     MRDOCS_DESCRIBE_KINDS(Type, NamedType, PointerType, ArrayType /* ... */)
//
//     describe::for_each(describe::describe_kinds<Type>{}, [](auto desc) {
//         using D = typename decltype(desc)::type; /* ... */ });
//
// Every listed derived class must be a complete type at the point of expansion,
// so the macro's natural home is a dedicated header that includes each kind's
// header. Query with describe::has_describe_kinds<C> / describe::describe_kinds<C>.
//
// The emitted mrdocs_kind_descriptor_fn is only ever read through decltype; the
// inline `{ return {}; }` body (rather than a pure declaration) silences GCC's
// -Wunused-function when the macro is used in an anonymous namespace, and inline
// keeps it ODR-safe in headers.

#define MRDOCS_KIND_ENTRY(C, D)                                     \
    , ::mrdocs::describe::detail::kind_descriptor<C, D>{}

#define MRDOCS_DESCRIBE_KINDS(C, ...)                               \
    static_assert(std::is_class_v<C>,                               \
        "MRDOCS_DESCRIBE_KINDS should only be used with "           \
        "class types");                                             \
    [[maybe_unused]]                                                \
    inline decltype(                                                \
        ::mrdocs::describe::detail::kind_descriptor_fn_impl(0               \
            __VA_OPT__(MRDOCS_PP_FOR_EACH(                          \
                MRDOCS_KIND_ENTRY, C, __VA_ARGS__))                 \
        )) mrdocs_kind_descriptor_fn(C**) { return {}; }

// The BEGIN/END variant drives the kind list from an X-macro `.inc` file:
//
//     #define INFO(Name) MRDOCS_KIND_ENTRY(Base, Name##Suffix)
//     MRDOCS_DESCRIBE_KINDS_BEGIN(Base)
//     #include <path/to/Nodes.inc>
//     MRDOCS_DESCRIBE_KINDS_END(Base)

#define MRDOCS_DESCRIBE_KINDS_BEGIN(C)                              \
    static_assert(std::is_class_v<C>,                               \
        "MRDOCS_DESCRIBE_KINDS_BEGIN should only be used "          \
        "with class types");                                        \
    [[maybe_unused]]                                                \
    inline decltype(                                                \
        ::mrdocs::describe::detail::kind_descriptor_fn_impl(0

#define MRDOCS_DESCRIBE_KINDS_END(C)                                \
        )) mrdocs_kind_descriptor_fn(C**) { return {}; }

#endif // MRDOCS_SUPPORT_DESCRIBE_HPP
