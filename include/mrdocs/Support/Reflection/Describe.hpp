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

#ifndef MRDOCS_API_SUPPORT_REFLECTION_DESCRIBE_HPP
#define MRDOCS_API_SUPPORT_REFLECTION_DESCRIBE_HPP

#include <mrdocs/Support/String/String.hpp>
#include <mrdocs/Support/Demangle.hpp>
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

// Descriptor for one concrete kind of a polymorphic base: it exposes the
// derived type as its `type` alias. `D` may be incomplete (only named),
// so a kind list can be registered from forward declarations.
template<class C, class D>
struct kind_descriptor
{
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

namespace detail {
// Invoke `f(args...)`; when it returns void the call is treated as
// "keep going" (true), otherwise its result is contextually converted
// to bool. This lets a callback opt into early-exit just by returning a
// bool instead of void.
template<class F, class... Args>
constexpr bool
invokeContinue(F&& f, Args&&... args)
{
    if constexpr (std::is_void_v<std::invoke_result_t<F, Args...>>)
    {
        std::forward<F>(f)(std::forward<Args>(args)...);
        return true;
    }
    else
    {
        return static_cast<bool>(
            std::forward<F>(f)(std::forward<Args>(args)...));
    }
}
} // namespace detail

/** Invoke `f` with the name and value of each reflected member of `obj`.

    A value-yielding companion to `for_each_member<T>(f)`: instead of
    descriptors it hands the callback each member's source name (a
    `std::string_view`) and a reference to that member taken from `obj`.
    The reference keeps `obj`'s cv-qualification, so a `const` object
    yields `const` members. Inherited members are included; each
    descriptor's pointer applies to `obj` directly, so bases need no
    separate handling by the caller.

    `f` may be invoked as `f(name, value)` or as `f(value)`. If `f`
    returns `void` the walk always continues; if it returns something
    convertible to `bool`, a `false` result stops the walk early.

    @param obj The (possibly `const`) described object to walk.
    @param f   The callback; see above for the accepted forms.
    @return `true` if every member was visited, `false` if `f` stopped
            the walk early.
*/
template<class U, class F>
    requires described<std::remove_cvref_t<U>>
constexpr bool
for_each_member(U&& obj, F&& f)
{
    using T = std::remove_cvref_t<U>;
    bool keepGoing = true;
    for_each_member<T>(
        [&](auto d)
        {
            if (!keepGoing)
            {
                return;
            }
            auto& value = obj.*d.pointer;
            if constexpr (std::is_invocable_v<
                    F&, std::string_view, decltype(value)>)
            {
                keepGoing = detail::invokeContinue(
                    f, std::string_view(d.name), value);
            }
            else
            {
                keepGoing = detail::invokeContinue(f, value);
            }
        });
    return keepGoing;
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

/** Whether a described type collapses to a single string.

    `true` when `T` is a described struct whose only reflected member
    (inherited ones included) is a plain string. Such a type is
    represented as that one string rather than as a nested object, both
    in the XML output and in the reflection DOM. This is the single
    source of truth for that rule, shared by the XML writer and the
    `describedToDom` proxy.

    Examples:
    @li `ExprInfo` (its only member is the written expression) ->
        `true`; reflects as the written string.
    @li `SourceInfo`, `FunctionSymbol` (many members) -> `false`.
    @li a described struct with one non-string member -> `false`.
*/
template<class T>
concept isSingleStringObject =
    has_describe_members<T>::value &&
    describedMemberCount<T>() == 1 &&
    describedMembersAllText<T>();

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
        [&](auto const& D) { if constexpr (std::remove_cvref_t<decltype(D)>::value == V) { name = D.name; } });
    return toKebabCase(name).size();
}

template <auto V>
consteval auto
enumeratorKebabArray()
{
    std::array<char, enumeratorKebabSize<V>()> arr{};
    std::string_view name;
    for_each(describe_enumerators<decltype(V)>{},
        [&](auto const& D) { if constexpr (std::remove_cvref_t<decltype(D)>::value == V) { name = D.name; } });
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
#define MRDOCS_PP_FOR_EACH_51(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_50(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_52(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_51(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_53(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_52(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_54(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_53(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_55(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_54(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_56(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_55(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_57(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_56(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_58(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_57(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_59(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_58(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_60(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_59(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_61(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_60(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_62(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_61(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_63(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_62(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_64(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_63(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_65(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_64(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_66(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_65(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_67(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_66(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_68(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_67(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_69(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_68(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_70(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_69(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_71(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_70(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_72(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_71(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_73(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_72(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_74(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_73(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_75(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_74(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_76(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_75(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_77(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_76(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_78(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_77(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_79(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_78(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_80(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_79(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_81(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_80(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_82(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_81(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_83(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_82(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_84(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_83(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_85(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_84(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_86(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_85(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_87(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_86(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_88(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_87(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_89(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_88(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_90(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_89(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_91(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_90(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_92(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_91(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_93(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_92(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_94(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_93(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_95(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_94(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_96(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_95(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_97(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_96(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_98(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_97(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_99(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_98(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_100(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_99(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_101(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_100(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_102(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_101(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_103(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_102(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_104(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_103(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_105(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_104(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_106(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_105(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_107(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_106(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_108(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_107(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_109(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_108(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_110(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_109(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_111(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_110(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_112(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_111(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_113(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_112(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_114(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_113(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_115(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_114(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_116(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_115(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_117(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_116(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_118(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_117(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_119(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_118(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_120(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_119(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_121(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_120(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_122(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_121(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_123(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_122(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_124(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_123(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_125(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_124(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_126(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_125(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_127(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_126(F, a, __VA_ARGS__))
#define MRDOCS_PP_FOR_EACH_128(F, a, x, ...) MRDOCS_PP_EXPAND(MRDOCS_PP_CALL(F, a, x) MRDOCS_PP_FOR_EACH_127(F, a, __VA_ARGS__))

// This per-arity ladder is the standard preprocessor technique for member
// enumeration. Boost.Describe does the same: BOOST_DESCRIBE_PP_FOR_EACH is
// a fixed-maximum FOR_EACH generated the same way. It is verbose but not
// fragile, since adding arities only extends the ladder. The alternative,
// structured-binding reflection (Boost.PFR), avoids the macro but only
// works for aggregates and carries its own generated arity limit.

// --- Count arguments -----------------------------------------------

#define MRDOCS_PP_FE_EXTRACT(                                       \
    _0,  _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9,                \
    _10, _11, _12, _13, _14, _15, _16, _17, _18, _19,               \
    _20, _21, _22, _23, _24, _25, _26, _27, _28, _29,               \
    _30, _31, _32, _33, _34, _35, _36, _37, _38, _39,               \
    _40, _41, _42, _43, _44, _45, _46, _47, _48, _49,               \
    _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, \
    _60, _61, _62, _63, _64, _65, _66, _67, _68, _69, \
    _70, _71, _72, _73, _74, _75, _76, _77, _78, _79, \
    _80, _81, _82, _83, _84, _85, _86, _87, _88, _89, \
    _90, _91, _92, _93, _94, _95, _96, _97, _98, _99, \
    _100, _101, _102, _103, _104, _105, _106, _107, _108, _109, \
    _110, _111, _112, _113, _114, _115, _116, _117, _118, _119, \
    _120, _121, _122, _123, _124, _125, _126, _127, _128, V, ...) V

#define MRDOCS_PP_FOR_EACH(F, ...)                                  \
    MRDOCS_PP_EXPAND(MRDOCS_PP_EXPAND(                              \
        MRDOCS_PP_FE_EXTRACT(__VA_ARGS__,                           \
            MRDOCS_PP_FOR_EACH_128,                                 \
            MRDOCS_PP_FOR_EACH_127,                                 \
            MRDOCS_PP_FOR_EACH_126,                                 \
            MRDOCS_PP_FOR_EACH_125,                                 \
            MRDOCS_PP_FOR_EACH_124,                                 \
            MRDOCS_PP_FOR_EACH_123,                                 \
            MRDOCS_PP_FOR_EACH_122,                                 \
            MRDOCS_PP_FOR_EACH_121,                                 \
            MRDOCS_PP_FOR_EACH_120,                                 \
            MRDOCS_PP_FOR_EACH_119,                                 \
            MRDOCS_PP_FOR_EACH_118,                                 \
            MRDOCS_PP_FOR_EACH_117,                                 \
            MRDOCS_PP_FOR_EACH_116,                                 \
            MRDOCS_PP_FOR_EACH_115,                                 \
            MRDOCS_PP_FOR_EACH_114,                                 \
            MRDOCS_PP_FOR_EACH_113,                                 \
            MRDOCS_PP_FOR_EACH_112,                                 \
            MRDOCS_PP_FOR_EACH_111,                                 \
            MRDOCS_PP_FOR_EACH_110,                                 \
            MRDOCS_PP_FOR_EACH_109,                                 \
            MRDOCS_PP_FOR_EACH_108,                                 \
            MRDOCS_PP_FOR_EACH_107,                                 \
            MRDOCS_PP_FOR_EACH_106,                                 \
            MRDOCS_PP_FOR_EACH_105,                                 \
            MRDOCS_PP_FOR_EACH_104,                                 \
            MRDOCS_PP_FOR_EACH_103,                                 \
            MRDOCS_PP_FOR_EACH_102,                                 \
            MRDOCS_PP_FOR_EACH_101,                                 \
            MRDOCS_PP_FOR_EACH_100,                                 \
            MRDOCS_PP_FOR_EACH_99,                                 \
            MRDOCS_PP_FOR_EACH_98,                                 \
            MRDOCS_PP_FOR_EACH_97,                                 \
            MRDOCS_PP_FOR_EACH_96,                                 \
            MRDOCS_PP_FOR_EACH_95,                                 \
            MRDOCS_PP_FOR_EACH_94,                                 \
            MRDOCS_PP_FOR_EACH_93,                                 \
            MRDOCS_PP_FOR_EACH_92,                                 \
            MRDOCS_PP_FOR_EACH_91,                                 \
            MRDOCS_PP_FOR_EACH_90,                                 \
            MRDOCS_PP_FOR_EACH_89,                                 \
            MRDOCS_PP_FOR_EACH_88,                                 \
            MRDOCS_PP_FOR_EACH_87,                                 \
            MRDOCS_PP_FOR_EACH_86,                                 \
            MRDOCS_PP_FOR_EACH_85,                                 \
            MRDOCS_PP_FOR_EACH_84,                                 \
            MRDOCS_PP_FOR_EACH_83,                                 \
            MRDOCS_PP_FOR_EACH_82,                                 \
            MRDOCS_PP_FOR_EACH_81,                                 \
            MRDOCS_PP_FOR_EACH_80,                                 \
            MRDOCS_PP_FOR_EACH_79,                                 \
            MRDOCS_PP_FOR_EACH_78,                                 \
            MRDOCS_PP_FOR_EACH_77,                                 \
            MRDOCS_PP_FOR_EACH_76,                                 \
            MRDOCS_PP_FOR_EACH_75,                                 \
            MRDOCS_PP_FOR_EACH_74,                                 \
            MRDOCS_PP_FOR_EACH_73,                                 \
            MRDOCS_PP_FOR_EACH_72,                                 \
            MRDOCS_PP_FOR_EACH_71,                                 \
            MRDOCS_PP_FOR_EACH_70,                                 \
            MRDOCS_PP_FOR_EACH_69,                                 \
            MRDOCS_PP_FOR_EACH_68,                                 \
            MRDOCS_PP_FOR_EACH_67,                                 \
            MRDOCS_PP_FOR_EACH_66,                                 \
            MRDOCS_PP_FOR_EACH_65,                                 \
            MRDOCS_PP_FOR_EACH_64,                                 \
            MRDOCS_PP_FOR_EACH_63,                                 \
            MRDOCS_PP_FOR_EACH_62,                                 \
            MRDOCS_PP_FOR_EACH_61,                                 \
            MRDOCS_PP_FOR_EACH_60,                                 \
            MRDOCS_PP_FOR_EACH_59,                                 \
            MRDOCS_PP_FOR_EACH_58,                                 \
            MRDOCS_PP_FOR_EACH_57,                                 \
            MRDOCS_PP_FOR_EACH_56,                                 \
            MRDOCS_PP_FOR_EACH_55,                                 \
            MRDOCS_PP_FOR_EACH_54,                                 \
            MRDOCS_PP_FOR_EACH_53,                                 \
            MRDOCS_PP_FOR_EACH_52,                                 \
            MRDOCS_PP_FOR_EACH_51,                                 \
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

#define MRDOCS_DETAIL_MEMBER(C, m) \
    , ::mrdocs::describe::detail::member_descriptor<                \
        &C::m, []{ return #m; }>{}

#define MRDOCS_PP_UNPACK(...) __VA_ARGS__

#define MRDOCS_DETAIL_DESCRIBE_BASES(C, ...)                        \
    [[maybe_unused]]                                                \
    typename ::mrdocs::describe::detail::bases_descriptor_impl<             \
        C, ::mrdocs::describe::list<__VA_ARGS__>>::type             \
    mrdocs_base_descriptor_fn(C**);

#define MRDOCS_DETAIL_DESCRIBE_MEMBERS(C, ...)                      \
    [[maybe_unused]]                                                \
    decltype(                                                       \
        ::mrdocs::describe::detail::member_descriptor_fn_impl(              \
            0 __VA_OPT__(MRDOCS_PP_FOR_EACH(                        \
                MRDOCS_DETAIL_MEMBER, C, __VA_ARGS__))))              \
    mrdocs_member_descriptor_fn(C**);

/** Describe the bases and members of a class, outside its definition.

    Place it at namespace scope, in the namespace of the class, after the
    class definition. `Bases` and `Members` are parenthesized,
    comma-separated lists, and either one may be empty:

    @code
    namespace geo {

    struct Shape
    {
        std::string name;
    };

    struct Point : Shape
    {
        int x = 0;
        int y = 0;
    };

    struct Tag {};

    MRDOCS_DESCRIBE_STRUCT(Shape, (), (name))
    MRDOCS_DESCRIBE_STRUCT(Point, (Shape), (x, y))
    MRDOCS_DESCRIBE_STRUCT(Tag, (), ())

    } // namespace geo
    @endcode

    Several bases are listed like several members, e.g.
    `MRDOCS_DESCRIBE_STRUCT(AB, (A, B), (c))`. The three lines expand to
    roughly the following (simplified: the `::mrdocs::describe::detail::`
    and `::mrdocs::describe::` qualifications are dropped and the
    assertion message is shortened):

    @code
    // MRDOCS_DESCRIBE_STRUCT(Shape, (), (name))
    static_assert(std::is_class_v<Shape> || std::is_union_v<Shape>, "...");
    [[maybe_unused]]
    typename bases_descriptor_impl<Shape, list<>>::type
    mrdocs_base_descriptor_fn(Shape**);
    [[maybe_unused]]
    decltype(member_descriptor_fn_impl(
        0,
        member_descriptor<&Shape::name, []{ return "name"; }>{}))
    mrdocs_member_descriptor_fn(Shape**);

    // MRDOCS_DESCRIBE_STRUCT(Point, (Shape), (x, y))
    // Rejects anything that isn't a class or a union.
    static_assert(std::is_class_v<Point> || std::is_union_v<Point>, "...");

    // Declared, never defined or called: only the return type matters.
    // ADL finds it from a `Point**`, and the return type lists the bases.
    [[maybe_unused]]
    typename bases_descriptor_impl<Point, list<Shape>>::type
    mrdocs_base_descriptor_fn(Point**);

    // Same trick for the members: one descriptor per name, holding the
    // member pointer and the name as a string.
    [[maybe_unused]]
    decltype(member_descriptor_fn_impl(
        0,
        member_descriptor<&Point::x, []{ return "x"; }>{},
        member_descriptor<&Point::y, []{ return "y"; }>{}))
    mrdocs_member_descriptor_fn(Point**);

    // MRDOCS_DESCRIBE_STRUCT(Tag, (), ())
    // Empty lists give an empty base list and no member descriptors.
    static_assert(std::is_class_v<Tag> || std::is_union_v<Tag>, "...");
    [[maybe_unused]]
    typename bases_descriptor_impl<Tag, list<>>::type
    mrdocs_base_descriptor_fn(Tag**);
    [[maybe_unused]]
    decltype(member_descriptor_fn_impl(0))
    mrdocs_member_descriptor_fn(Tag**);
    @endcode

    Afterwards the `describe` queries work on the type. Each member
    descriptor has a static `pointer` and `name`, and each base
    descriptor has a `type` alias:

    @code
    namespace describe = mrdocs::describe;

    static_assert(describe::described<geo::Point>);
    static_assert(describe::describedMemberCount<geo::Point>() == 3);

    void
    print(geo::Point const& p)
    {
        // Own members only: x, y
        describe::for_each(
            describe::describe_members<geo::Point>{},
            [&](auto d) {
                std::cout << d.name << " = " << p.*d.pointer << '\n';
            });

        // Direct bases only: Shape
        describe::for_each(
            describe::describe_bases<geo::Point>{},
            [](auto d) {
                using Base = typename decltype(d)::type;
                static_assert(std::is_same_v<Base, geo::Shape>);
            });

        // Inherited members too: name, x, y
        describe::for_each_member(
            p,
            [](std::string_view name, auto const& value) {
                std::cout << name << " = " << value << '\n';
            });
    }
    @endcode

    Things to keep in mind:

    @li The macro ends with its own `;`, so don't add another one after
        it. An extra `;` is an empty declaration that `-Wextra-semi`
        flags.
    @li It has to be in the class's own namespace. The queries find the
        declarations by ADL, so they don't see them anywhere else.
    @li List non-static data members by their unqualified names, up to
        128 of them. `&C::m` is formed at namespace scope, so private and
        protected members fail to compile. For those, and for class
        templates, use `MRDOCS_DESCRIBE_CLASS` inside the class instead.
    @li List direct bases only. `for_each_member` and
        `describedMemberCount` walk up the hierarchy themselves, which
        requires each listed base to be described too. Listing a type
        that isn't a base fails a `static_assert` once the bases are
        used.

    @param C The class type.
    @param Bases The parenthesized list of direct base classes.
    @param Members The parenthesized list of data member names.
*/
#define MRDOCS_DESCRIBE_STRUCT(C, Bases, Members)                   \
    static_assert(                                                  \
        std::is_class_v<C> || std::is_union_v<C>,                   \
        "MRDOCS_DESCRIBE_STRUCT should only be used with "          \
        "class types");                                             \
    MRDOCS_DETAIL_DESCRIBE_BASES(C, MRDOCS_PP_UNPACK Bases)                \
    MRDOCS_DETAIL_DESCRIBE_MEMBERS(C, MRDOCS_PP_UNPACK Members)

// --- MRDOCS_DESCRIBE_CLASS ------------------------------------------
//
// The friends are only ever read through `decltype`; no caller invokes them.
// They carry an inline `{ return {}; }` body rather than staying pure
// declarations because a hidden friend of a class-template instantiation can
// end up with internal linkage, and GCC then reports it as "declared static
// but never defined" (-Wunused-function). Defining it silences that; the body
// is never ODR-used. This matches MRDOCS_DESCRIBE_KINDS below.

#define MRDOCS_DETAIL_DESCRIBE_FRIEND_BASES(C, ...)                 \
    friend                                                          \
    typename ::mrdocs::describe::detail::bases_descriptor_impl<             \
        C, ::mrdocs::describe::list<__VA_ARGS__>>::type             \
    mrdocs_base_descriptor_fn(C**) { return {}; }

#define MRDOCS_DETAIL_DESCRIBE_FRIEND_MEMBERS(C, ...)               \
    friend                                                          \
    decltype(                                                       \
        ::mrdocs::describe::detail::member_descriptor_fn_impl(              \
            0 __VA_OPT__(MRDOCS_PP_FOR_EACH(                        \
                MRDOCS_DETAIL_MEMBER, C, __VA_ARGS__))))              \
    mrdocs_member_descriptor_fn(C**) { return {}; }

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns that each template instantiation declares a separate
// non-template friend function (-Wnon-template-friend). That is
// exactly the intended behaviour: every instantiation of the
// enclosing class template gets its own descriptor overload.
/** Describe the bases and members of a class, inside its definition.

    Works like `MRDOCS_DESCRIBE_STRUCT`, but goes inside the class
    definition, after the members it names. It declares the descriptor
    functions as hidden friends, so it also works for class templates
    and for private members:

    @code
    namespace app {

    struct Named
    {
        std::string name;
    };
    MRDOCS_DESCRIBE_STRUCT(Named, (), (name))

    template <class T>
    class Counter : public Named
    {
        T value_{};
        int hits_ = 0;

    public:
        explicit Counter(T v) : Named{"counter"}, value_(v) {}

        MRDOCS_DESCRIBE_CLASS(Counter, (Named), (value_, hits_))
    };

    class Empty
    {
    public:
        MRDOCS_DESCRIBE_CLASS(Empty, (), ())
    };

    } // namespace app
    @endcode

    The two calls expand to roughly the following (simplified: the
    `::mrdocs::describe::detail::` and `::mrdocs::describe::`
    qualifications are dropped):

    @code
    // MRDOCS_DESCRIBE_CLASS(Counter, (Named), (value_, hits_))
    // Hidden friends: only ADL on a `Counter<T>**` finds them, and each
    // instantiation of the template gets its own pair. Nothing calls
    // them; the `{ return {}; }` bodies only keep GCC from warning about
    // undefined internal functions.
    friend
    typename bases_descriptor_impl<Counter, list<Named>>::type
    mrdocs_base_descriptor_fn(Counter**) { return {}; }

    friend
    decltype(member_descriptor_fn_impl(
        0,
        member_descriptor<&Counter::value_, []{ return "value_"; }>{},
        member_descriptor<&Counter::hits_, []{ return "hits_"; }>{}))
    mrdocs_member_descriptor_fn(Counter**) { return {}; }

    // MRDOCS_DESCRIBE_CLASS(Empty, (), ())
    friend
    typename bases_descriptor_impl<Empty, list<>>::type
    mrdocs_base_descriptor_fn(Empty**) { return {}; }

    friend
    decltype(member_descriptor_fn_impl(0))
    mrdocs_member_descriptor_fn(Empty**) { return {}; }
    @endcode

    With GCC, the two friends are also wrapped in `_Pragma`s that silence
    `-Wnon-template-friend` for them.

    Afterwards the same `describe` queries as for `MRDOCS_DESCRIBE_STRUCT`
    work on every instantiation, private members included:

    @code
    namespace describe = mrdocs::describe;

    static_assert(describe::described<app::Counter<double>>);
    static_assert(describe::describedMemberCount<app::Counter<int>>() == 3);

    void
    dump()
    {
        app::Counter<int> c(42);

        // Prints name = counter, value_ = 42, hits_ = 0
        describe::for_each_member(
            c,
            [](std::string_view name, auto const& v) {
                std::cout << name << " = " << v << '\n';
            });
    }
    @endcode

    Things to keep in mind:

    @li Put it after the member declarations. The friend return types
        aren't a complete-class context, so a member declared below the
        macro isn't found ("no member named ...").
    @li `C` names the enclosing class. In a template, the injected name
        (`Counter`) is enough; you don't need to spell `Counter<T>`.
    @li The access section it sits in doesn't matter, since friend
        declarations ignore access.
    @li It ends with a function body, so it needs no trailing `;`.
    @li The rules for `Bases` and `Members` are the same as for
        `MRDOCS_DESCRIBE_STRUCT`: direct bases only, each described too
        if you walk inherited members, and up to 128 data members.

    @param C The class type.
    @param Bases The parenthesized list of direct base classes.
    @param Members The parenthesized list of data member names.
*/
#define MRDOCS_DESCRIBE_CLASS(C, Bases, Members)                    \
    _Pragma("GCC diagnostic push")                                  \
    _Pragma("GCC diagnostic ignored \"-Wnon-template-friend\"")     \
    MRDOCS_DETAIL_DESCRIBE_FRIEND_BASES(C, MRDOCS_PP_UNPACK Bases)         \
    MRDOCS_DETAIL_DESCRIBE_FRIEND_MEMBERS(C, MRDOCS_PP_UNPACK Members)     \
    _Pragma("GCC diagnostic pop")
#else
/** Describe the bases and members of a class, inside its definition.

    Works like `MRDOCS_DESCRIBE_STRUCT`, but goes inside the class
    definition, after the members it names. It declares the descriptor
    functions as hidden friends, so it also works for class templates
    and for private members:

    @code
    namespace app {

    struct Named
    {
        std::string name;
    };
    MRDOCS_DESCRIBE_STRUCT(Named, (), (name))

    template <class T>
    class Counter : public Named
    {
        T value_{};
        int hits_ = 0;

    public:
        explicit Counter(T v) : Named{"counter"}, value_(v) {}

        MRDOCS_DESCRIBE_CLASS(Counter, (Named), (value_, hits_))
    };

    class Empty
    {
    public:
        MRDOCS_DESCRIBE_CLASS(Empty, (), ())
    };

    } // namespace app
    @endcode

    The two calls expand to roughly the following (simplified: the
    `::mrdocs::describe::detail::` and `::mrdocs::describe::`
    qualifications are dropped):

    @code
    // MRDOCS_DESCRIBE_CLASS(Counter, (Named), (value_, hits_))
    // Hidden friends: only ADL on a `Counter<T>**` finds them, and each
    // instantiation of the template gets its own pair. Nothing calls
    // them; the `{ return {}; }` bodies only keep GCC from warning about
    // undefined internal functions.
    friend
    typename bases_descriptor_impl<Counter, list<Named>>::type
    mrdocs_base_descriptor_fn(Counter**) { return {}; }

    friend
    decltype(member_descriptor_fn_impl(
        0,
        member_descriptor<&Counter::value_, []{ return "value_"; }>{},
        member_descriptor<&Counter::hits_, []{ return "hits_"; }>{}))
    mrdocs_member_descriptor_fn(Counter**) { return {}; }

    // MRDOCS_DESCRIBE_CLASS(Empty, (), ())
    friend
    typename bases_descriptor_impl<Empty, list<>>::type
    mrdocs_base_descriptor_fn(Empty**) { return {}; }

    friend
    decltype(member_descriptor_fn_impl(0))
    mrdocs_member_descriptor_fn(Empty**) { return {}; }
    @endcode

    With GCC, the two friends are also wrapped in `_Pragma`s that silence
    `-Wnon-template-friend` for them.

    Afterwards the same `describe` queries as for `MRDOCS_DESCRIBE_STRUCT`
    work on every instantiation, private members included:

    @code
    namespace describe = mrdocs::describe;

    static_assert(describe::described<app::Counter<double>>);
    static_assert(describe::describedMemberCount<app::Counter<int>>() == 3);

    void
    dump()
    {
        app::Counter<int> c(42);

        // Prints name = counter, value_ = 42, hits_ = 0
        describe::for_each_member(
            c,
            [](std::string_view name, auto const& v) {
                std::cout << name << " = " << v << '\n';
            });
    }
    @endcode

    Things to keep in mind:

    @li Put it after the member declarations. The friend return types
        aren't a complete-class context, so a member declared below the
        macro isn't found ("no member named ...").
    @li `C` names the enclosing class. In a template, the injected name
        (`Counter`) is enough; you don't need to spell `Counter<T>`.
    @li The access section it sits in doesn't matter, since friend
        declarations ignore access.
    @li It ends with a function body, so it needs no trailing `;`.
    @li The rules for `Bases` and `Members` are the same as for
        `MRDOCS_DESCRIBE_STRUCT`: direct bases only, each described too
        if you walk inherited members, and up to 128 data members.

    @param C The class type.
    @param Bases The parenthesized list of direct base classes.
    @param Members The parenthesized list of data member names.
*/
#define MRDOCS_DESCRIBE_CLASS(C, Bases, Members)                    \
    MRDOCS_DETAIL_DESCRIBE_FRIEND_BASES(C, MRDOCS_PP_UNPACK Bases)         \
    MRDOCS_DETAIL_DESCRIBE_FRIEND_MEMBERS(C, MRDOCS_PP_UNPACK Members)
#endif

// --- MRDOCS_DESCRIBE_ENUM ------------------------------------------

/** Emit the describe entry for one enumerator.

    Use it between `MRDOCS_DESCRIBE_ENUM_BEGIN` and
    `MRDOCS_DESCRIBE_ENUM_END`, usually by pointing the X-macro `INFO` at
    it before including the `.inc` file that lists the enumerators:

    @code
    MRDOCS_DESCRIBE_ENUM_BEGIN(ShapeKind)
    #define INFO(Name) MRDOCS_ENUM_ENTRY(ShapeKind, Name)
    #include "ShapeNodes.inc"
    MRDOCS_DESCRIBE_ENUM_END(ShapeKind)
    @endcode

    You can also write the entries by hand:

    @code
    enum class Dir { Up, Down };

    MRDOCS_DESCRIBE_ENUM_BEGIN(Dir)
    MRDOCS_ENUM_ENTRY(Dir, Up)
    MRDOCS_ENUM_ENTRY(Dir, Down)
    MRDOCS_DESCRIBE_ENUM_END(Dir)
    @endcode

    Each entry is one comma-prefixed descriptor argument:

    @code
    // MRDOCS_ENUM_ENTRY(Dir, Up)
    , ::mrdocs::describe::detail::enum_descriptor<
        Dir::Up, []{ return "Up"; }>{}
    // The descriptor's `value` is Dir::Up and its `name` is "Up".
    @endcode

    The leading comma is what lets entries follow the `0` that
    `MRDOCS_DESCRIBE_ENUM_BEGIN` leaves open, so don't put commas or
    semicolons between entries. It only makes sense inside that
    bracket; anywhere else the stray comma is a syntax error.

    @param E The enum type.
    @param e The enumerator name, written without the `E::` prefix.
*/
#define MRDOCS_ENUM_ENTRY(E, e)                                     \
    , ::mrdocs::describe::detail::enum_descriptor<                  \
        E::e, []{ return #e; }>{}

/** Describe the enumerators of an enum.

    Pass the enum type followed by the enumerators you want to describe,
    up to 128 of them. Place it at namespace scope, in the same
    namespace as the enum, after the enum definition:

    @code
    namespace shapes {

    enum class Shape { Circle, RoundedRect, Triangle };

    MRDOCS_DESCRIBE_ENUM(Shape, Circle, RoundedRect, Triangle)

    } // namespace shapes
    @endcode

    The macro declares a function that is found by argument-dependent
    lookup and whose return type lists one descriptor per enumerator:

    @code
    // Simplified: whitespace changed; each lambda is a distinct closure
    // type that only returns the stringized enumerator name.

    // MRDOCS_DESCRIBE_ENUM(Shape, Circle, RoundedRect, Triangle)
    static_assert(std::is_enum_v<Shape>,
        "MRDOCS_DESCRIBE_ENUM should only be used with enums");
    [[maybe_unused]]
    decltype(::mrdocs::describe::detail::enum_descriptor_fn_impl(0
        , ::mrdocs::describe::detail::enum_descriptor<
            Shape::Circle, []{ return "Circle"; }>{}
        , ::mrdocs::describe::detail::enum_descriptor<
            Shape::RoundedRect, []{ return "RoundedRect"; }>{}
        , ::mrdocs::describe::detail::enum_descriptor<
            Shape::Triangle, []{ return "Triangle"; }>{}
    )) mrdocs_enum_descriptor_fn(Shape**);
    // The return type is describe::list<D1, D2, D3>, where each Di has
    // `static constexpr Shape value` and `static constexpr char const*
    // name`. The function is declared only; it's never called.
    @endcode

    Afterwards the enum works with the describe queries and the string
    helpers, all usable in constant expressions:

    @code
    namespace describe = mrdocs::describe;
    using shapes::Shape;

    static_assert(describe::has_describe_enumerators<Shape>::value);

    // Kebab-case name, as used in the generated output
    static_assert(mrdocs::toString(Shape::RoundedRect) == "rounded-rect");

    // Declared name
    static_assert(
        describe::enum_to_string(Shape::RoundedRect) == "RoundedRect");

    void
    demo()
    {
        // Declared name back to the enumerator
        Shape s{};
        bool ok = describe::enum_from_string("Triangle", s);
        // ok == true, s == Shape::Triangle

        // Visit every described enumerator
        describe::for_each(
            describe::describe_enumerators<Shape>{},
            [](auto d) {
                // d.value: Shape::Circle, then RoundedRect, then Triangle
                // d.name:  "Circle", then "RoundedRect", then "Triangle"
            });
    }
    @endcode

    Things to keep in mind:

    @li Don't put it inside a class, even for a nested enum. There it
        declares a member function that lookup never finds, so the enum
        silently stays undescribed. For a nested enum, write it at
        namespace scope after the class and qualify the type:
        `MRDOCS_DESCRIBE_ENUM(Canvas::Layer, background, foreground)`.
    @li Writing it in a different namespace also leaves the enum
        undescribed, for the same reason.
    @li It already ends with a semicolon, so don't add one.
    @li Enumerators you leave out are unknown to the helpers: `toString`
        and `describe::enum_to_string` return an empty string for them,
        and `describe::enum_from_string` never produces them.
    @li Works for scoped and unscoped enums, since each entry is spelled
        `E::name`.
    @li When the enumerators already live in an X-macro `.inc` file, use
        `MRDOCS_DESCRIBE_ENUM_BEGIN` and `MRDOCS_DESCRIBE_ENUM_END`
        instead. They also have no limit on the number of enumerators.
    @li To mark one enumerator as the empty state, follow it with
        `MRDOCS_DESCRIBE_ENUM_UNDEFINED`.

    @param E The enum type.
*/
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

/** Open an enum description driven by an X-macro `.inc` file.

    Use it when the enumerators already live in an X-macro `.inc` file,
    one `INFO(Name)` line per enumerator, that also defines the enum. You
    then describe the enum from the same list instead of repeating every
    name in `MRDOCS_DESCRIBE_ENUM`, so the two can't drift apart.

    Given this `ShapeNodes.inc`:

    @code
    #ifndef INFO
    #define INFO(Name)
    #endif

    INFO(Circle)
    INFO(RoundedRect)
    INFO(Triangle)

    #undef INFO
    @endcode

    include it once to define the enum and once more between
    `MRDOCS_DESCRIBE_ENUM_BEGIN` and `MRDOCS_DESCRIBE_ENUM_END`, with
    `INFO` pointed at `MRDOCS_ENUM_ENTRY`:

    @code
    namespace shapes {

    enum class ShapeKind
    {
        None = 0,
    #define INFO(Name) Name,
    #include "ShapeNodes.inc"
    };

    MRDOCS_DESCRIBE_ENUM_BEGIN(ShapeKind)
    #define INFO(Name) MRDOCS_ENUM_ENTRY(ShapeKind, Name)
    #include "ShapeNodes.inc"
    MRDOCS_DESCRIBE_ENUM_END(ShapeKind)

    } // namespace shapes
    @endcode

    The `.inc` file `#undef`s `INFO` itself, so no cleanup line is
    needed. The three pieces together produce the same declaration as
    `MRDOCS_DESCRIBE_ENUM(ShapeKind, Circle, RoundedRect, Triangle)`:

    @code
    // Simplified: each lambda is a distinct closure type that returns the
    // stringized name.

    // MRDOCS_DESCRIBE_ENUM_BEGIN(ShapeKind)
    static_assert(std::is_enum_v<ShapeKind>,
        "MRDOCS_DESCRIBE_ENUM should only be used with enums");
    [[maybe_unused]]
    decltype(::mrdocs::describe::detail::enum_descriptor_fn_impl(0

    // #include "ShapeNodes.inc": one MRDOCS_ENUM_ENTRY per INFO line
        , ::mrdocs::describe::detail::enum_descriptor<
            ShapeKind::Circle, []{ return "Circle"; }>{}
        , ::mrdocs::describe::detail::enum_descriptor<
            ShapeKind::RoundedRect, []{ return "RoundedRect"; }>{}
        , ::mrdocs::describe::detail::enum_descriptor<
            ShapeKind::Triangle, []{ return "Triangle"; }>{}

    // MRDOCS_DESCRIBE_ENUM_END(ShapeKind)
    )) mrdocs_enum_descriptor_fn(ShapeKind**);
    @endcode

    Here `None` isn't in the `.inc` file, so it isn't described:
    `toString(ShapeKind::None)` is empty, while
    `toString(ShapeKind::RoundedRect)` is `"rounded-rect"`.

    Things to keep in mind:

    @li The macro opens a parenthesized expression that
        `MRDOCS_DESCRIBE_ENUM_END` closes, so everything in between must
        expand to nothing but `MRDOCS_ENUM_ENTRY` calls (and
        preprocessor directives). Each entry starts with its own comma,
        so don't add separators.
    @li Pass the same enum type to both macros.
    @li The placement rules of `MRDOCS_DESCRIBE_ENUM` apply: namespace
        scope, in the namespace of the enum, never inside a class.
    @li Don't put a `;` after either macro. After this one it's a
        syntax error, and `MRDOCS_DESCRIBE_ENUM_END` already ends with
        one.
    @li Unlike `MRDOCS_DESCRIBE_ENUM`, there's no limit on the number of
        enumerators.

    @param E The enum type.
*/
#define MRDOCS_DESCRIBE_ENUM_BEGIN(E)                               \
    static_assert(std::is_enum_v<E>,                                \
        "MRDOCS_DESCRIBE_ENUM should only be used with enums");     \
    [[maybe_unused]]                                                \
    decltype(::mrdocs::describe::detail::enum_descriptor_fn_impl(0

/** Close an enum description opened with `MRDOCS_DESCRIBE_ENUM_BEGIN`.

    It closes the expression opened by `MRDOCS_DESCRIBE_ENUM_BEGIN` and
    names the descriptor function, which completes the description:

    @code
    MRDOCS_DESCRIBE_ENUM_BEGIN(ShapeKind)
    #define INFO(Name) MRDOCS_ENUM_ENTRY(ShapeKind, Name)
    #include "ShapeNodes.inc"
    MRDOCS_DESCRIBE_ENUM_END(ShapeKind)
    @endcode

    On its own it expands to the closing tokens below; see
    `MRDOCS_DESCRIBE_ENUM_BEGIN` for the full expansion of the three
    pieces together:

    @code
    // MRDOCS_DESCRIBE_ENUM_END(ShapeKind)
    )) mrdocs_enum_descriptor_fn(ShapeKind**);
    @endcode

    Pass the same enum type given to `MRDOCS_DESCRIBE_ENUM_BEGIN`. The
    expansion ends with a semicolon, so don't add one. Without a matching
    `MRDOCS_DESCRIBE_ENUM_BEGIN` before it, it's a syntax error.

    @param E The enum type.
*/
#define MRDOCS_DESCRIBE_ENUM_END(E)                                 \
    )) mrdocs_enum_descriptor_fn(E**);

// --- MRDOCS_DESCRIBE_ENUM_UNDEFINED --------------------------------

/** Mark one enumerator as the enum's undefined (empty) state.

    Some enums have a value that means "not set", like `None`. Marking it
    makes the string helpers treat it as empty, and makes generators treat
    a field holding it as absent (an empty optional) instead of printing
    it. Place it at namespace scope, in the namespace of the enum, after
    the enum's `MRDOCS_DESCRIBE_ENUM`:

    @code
    namespace shapes {

    enum class Fill { None, Solid, Hatched };

    MRDOCS_DESCRIBE_ENUM(Fill, None, Solid, Hatched)
    MRDOCS_DESCRIBE_ENUM_UNDEFINED(Fill, None)

    } // namespace shapes
    @endcode

    It expands to a small function that ADL finds from the enum type:

    @code
    // MRDOCS_DESCRIBE_ENUM_UNDEFINED(Fill, None)
    [[maybe_unused]]
    inline constexpr Fill
    mrdocs_undefined_descriptor_fn(Fill**) noexcept { return Fill::None; }
    @endcode

    Afterwards:

    @code
    namespace describe = mrdocs::describe;
    using shapes::Fill;

    static_assert(describe::has_undefined_enumerator<Fill>);
    static_assert(describe::undefined_enumerator<Fill> == Fill::None);

    // The undefined state renders as the empty string
    static_assert(mrdocs::toString(Fill::None).empty());
    static_assert(describe::enum_to_string(Fill::None).empty());
    static_assert(mrdocs::toString(Fill::Solid) == "solid");

    void
    demo()
    {
        // and the empty string parses back to it
        Fill f = Fill::Solid;
        describe::enum_from_string("", f); // returns true, f == Fill::None
    }
    @endcode

    Things to keep in mind:

    @li Use it at most once per enum; a second use is a redefinition
        error.
    @li Like `MRDOCS_DESCRIBE_ENUM`, it must be in the namespace of the
        enum and not inside a class, or lookup won't find it.
    @li The function is `inline`, so it's safe in headers.
    @li It ends with a closing brace, so don't add a semicolon.

    @param E The enum type.
    @param U The enumerator that represents the undefined state, written
        without the `E::` prefix.
*/
#define MRDOCS_DESCRIBE_ENUM_UNDEFINED(E, U)                         \
    [[maybe_unused]]                                                 \
    inline constexpr E                                              \
    mrdocs_undefined_descriptor_fn(E**) noexcept { return E::U; }

// --- MRDOCS_DESCRIBE_KINDS -----------------------------------------
//
// The emitted mrdocs_kind_descriptor_fn is only ever read through decltype; the
// inline `{ return {}; }` body (rather than a pure declaration) silences GCC's
// -Wunused-function when the macro is used in an anonymous namespace, and inline
// keeps it ODR-safe in headers.

/** Emit the describe entry for one derived kind.

    Use it between `MRDOCS_DESCRIBE_KINDS_BEGIN` and
    `MRDOCS_DESCRIBE_KINDS_END`, one per derived class. Usually the
    X-macro `INFO` points at it, so an `.inc` file with one `INFO(Name)`
    per kind supplies the entries:

    @code
    #define INFO(Name) MRDOCS_KIND_ENTRY(Shape, Name##Shape)
    MRDOCS_DESCRIBE_KINDS_BEGIN(Shape)
    #include "ShapeNodes.inc"
    MRDOCS_DESCRIBE_KINDS_END(Shape)
    @endcode

    You can also write the entries by hand:

    @code
    struct Node { int Kind; };
    struct Leaf : Node {};
    struct Branch : Node {};

    MRDOCS_DESCRIBE_KINDS_BEGIN(Node)
        MRDOCS_KIND_ENTRY(Node, Leaf)
        MRDOCS_KIND_ENTRY(Node, Branch)
    MRDOCS_DESCRIBE_KINDS_END(Node)
    @endcode

    Each entry is a leading comma plus a descriptor object, which becomes
    one more argument of the call that `MRDOCS_DESCRIBE_KINDS_BEGIN`
    opens:

    @code
    // MRDOCS_KIND_ENTRY(Node, Leaf) expands to exactly:
    , ::mrdocs::describe::detail::kind_descriptor<Node, Leaf>{}
    @endcode

    Because of that leading comma it only makes sense in that spot. Don't
    put a `;` after it, and don't use it with `MRDOCS_DESCRIBE_KINDS`,
    which writes its own entries. `D` may still be a forward declaration
    here.

    @param C The polymorphic base class, the same one passed to
        `MRDOCS_DESCRIBE_KINDS_BEGIN`.
    @param D The derived class.
*/
#define MRDOCS_KIND_ENTRY(C, D)                                     \
    , ::mrdocs::describe::detail::kind_descriptor<C, D>{}

/** Describe a polymorphic base and the closed set of its derived classes.

    Place it at namespace scope, in the namespace of the base. The first
    argument is the base; the rest are its concrete derived classes, from
    none up to 128:

    @code
    namespace shapes {

    enum class ShapeKind { Circle, Square };

    struct Shape
    {
        ShapeKind Kind;
    };

    struct Circle : Shape
    {
        static constexpr ShapeKind kind_id = ShapeKind::Circle;
        double radius = 1;
        Circle() : Shape{kind_id} {}
    };

    struct Square : Shape
    {
        static constexpr ShapeKind kind_id = ShapeKind::Square;
        double side = 2;
        Square() : Shape{kind_id} {}
    };

    MRDOCS_DESCRIBE_KINDS(Shape, Circle, Square)  // several kinds

    struct One { int Kind; };
    struct OnlyChild : One {};
    MRDOCS_DESCRIBE_KINDS(One, OnlyChild)         // a single kind

    struct Leaf {};
    MRDOCS_DESCRIBE_KINDS(Leaf)                   // no kinds at all

    } // namespace shapes
    @endcode

    The three calls expand to roughly the following (simplified: the
    `::mrdocs::describe::detail::` qualification is dropped and the
    assertion message is shortened):

    @code
    // MRDOCS_DESCRIBE_KINDS(Shape, Circle, Square)
    static_assert(std::is_class_v<Shape>, "...");

    // Defined inline but never called: only the return type matters.
    // ADL finds it from a `Shape**`.
    [[maybe_unused]]
    inline decltype(kind_descriptor_fn_impl(
        0,
        kind_descriptor<Shape, Circle>{},
        kind_descriptor<Shape, Square>{}))
    mrdocs_kind_descriptor_fn(Shape**) { return {}; }

    // So describe_kinds<Shape> is
    //   list<kind_descriptor<Shape, Circle>,
    //        kind_descriptor<Shape, Square>>
    // and each descriptor's `type` alias names one derived class.

    // MRDOCS_DESCRIBE_KINDS(One, OnlyChild)
    static_assert(std::is_class_v<One>, "...");
    [[maybe_unused]]
    inline decltype(kind_descriptor_fn_impl(
        0,
        kind_descriptor<One, OnlyChild>{}))
    mrdocs_kind_descriptor_fn(One**) { return {}; }

    // MRDOCS_DESCRIBE_KINDS(Leaf): an empty kind list
    static_assert(std::is_class_v<Leaf>, "...");
    [[maybe_unused]]
    inline decltype(kind_descriptor_fn_impl(0))
    mrdocs_kind_descriptor_fn(Leaf**) { return {}; }
    @endcode

    Afterwards `describe::has_describe_kinds` is true for the base (even
    with no kinds, where `describe::describe_kinds` is an empty list),
    `describe::for_each` iterates the kinds in the listed order, and
    `mrdocs::visit` from `<mrdocs/Support/TypeTraits/Visitor.hpp>` can
    downcast a base reference by comparing its `Kind` member with each
    kind's static `kind_id`:

    @code
    namespace describe = mrdocs::describe;

    static_assert(describe::has_describe_kinds<shapes::Shape>::value);

    double
    area(shapes::Shape const& s)
    {
        return mrdocs::visit(s, []<class T>(T const& shape) -> double {
            if constexpr (std::is_same_v<T, shapes::Circle>)
                return 3.14159 * shape.radius * shape.radius;
            else
                return shape.side * shape.side;
        });
    }

    int
    countKinds()
    {
        int n = 0;
        describe::for_each(
            describe::describe_kinds<shapes::Shape>{},
            [&](auto d) {
                using D = typename decltype(d)::type;
                static_assert(std::is_base_of_v<shapes::Shape, D>);
                ++n;
            });
        return n; // 2
    }
    @endcode

    Things to keep in mind:

    @li The derived classes may be forward declarations where the macro
        expands. They need to be complete wherever you use them, e.g.
        in `mrdocs::visit` or in a `for_each` body that touches `D`, so a
        header that includes every kind's header is still the natural
        home for the macro.
    @li The macro doesn't check that each kind derives from the base.
    @li `mrdocs::visit` needs at least one kind, so a base described
        with no kinds can be queried but not visited.
    @li It ends with a function body, so it needs no trailing `;`. The
        function is `inline`, so the macro is safe in a header, but each
        base can only be described once.
    @li When the kinds already live in an X-macro `.inc` file, use
        `MRDOCS_DESCRIBE_KINDS_BEGIN` and `MRDOCS_DESCRIBE_KINDS_END`
        instead.

    @param C The class type whose kinds follow.
*/
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

/** Open a kinds description driven by an X-macro `.inc` file.

    When the derived kinds already live in an X-macro `.inc` file (one
    `INFO(Name)` per kind, often the same file that builds the kind
    enum), this registers them without repeating the list in
    `MRDOCS_DESCRIBE_KINDS`. Point `INFO` at `MRDOCS_KIND_ENTRY`, then
    bracket the include with this macro and `MRDOCS_DESCRIBE_KINDS_END`.
    Given this `ShapeNodes.inc`:

    @code
    #ifndef INFO
    #define INFO(Name)
    #endif

    INFO(Circle)
    INFO(Square)

    #undef INFO
    @endcode

    a header can build both the enum and the kind list from it:

    @code
    namespace shapes {

    enum class ShapeKind
    {
    #define INFO(Name) Name,
    #include "ShapeNodes.inc"
    };

    struct Shape { ShapeKind Kind; };

    struct CircleShape : Shape
    {
        static constexpr ShapeKind kind_id = ShapeKind::Circle;
        CircleShape() : Shape{kind_id} {}
    };

    struct SquareShape : Shape
    {
        static constexpr ShapeKind kind_id = ShapeKind::Square;
        SquareShape() : Shape{kind_id} {}
    };

    #define INFO(Name) MRDOCS_KIND_ENTRY(Shape, Name##Shape)
    MRDOCS_DESCRIBE_KINDS_BEGIN(Shape)
    #include "ShapeNodes.inc"
    MRDOCS_DESCRIBE_KINDS_END(Shape)

    } // namespace shapes
    @endcode

    The entries can also be written by hand, and an empty pair registers
    a base with no kinds:

    @code
    MRDOCS_DESCRIBE_KINDS_BEGIN(Node)
        MRDOCS_KIND_ENTRY(Node, Leaf)
        MRDOCS_KIND_ENTRY(Node, Branch)
    MRDOCS_DESCRIBE_KINDS_END(Node)

    MRDOCS_DESCRIBE_KINDS_BEGIN(Empty)
    MRDOCS_DESCRIBE_KINDS_END(Empty)
    @endcode

    After preprocessing, the `Shape` block becomes the following
    (simplified: the `::mrdocs::describe::detail::` qualification is
    dropped and the assertion message is shortened):

    @code
    // MRDOCS_DESCRIBE_KINDS_BEGIN(Shape): leaves the call open
    static_assert(std::is_class_v<Shape>, "...");
    [[maybe_unused]]
    inline decltype(kind_descriptor_fn_impl(0

    // #include "ShapeNodes.inc": one MRDOCS_KIND_ENTRY per INFO line
        , kind_descriptor<Shape, CircleShape>{}   // INFO(Circle)
        , kind_descriptor<Shape, SquareShape>{}   // INFO(Square)

    // MRDOCS_DESCRIBE_KINDS_END(Shape): closes the call
    )) mrdocs_kind_descriptor_fn(Shape**) { return {}; }
    @endcode

    That's the same declaration
    `MRDOCS_DESCRIBE_KINDS(Shape, CircleShape, SquareShape)` produces,
    so the same queries (`describe::describe_kinds`,
    `describe::has_describe_kinds`, `mrdocs::visit`) work afterwards.

    Things to keep in mind:

    @li This macro leaves a call open, so only `MRDOCS_KIND_ENTRY`
        expansions and preprocessor directives may come before the
        matching `MRDOCS_DESCRIBE_KINDS_END`. Anything else, including a
        stray `;`, is a syntax error.
    @li Pass the same base to this macro, to every entry, and to
        `MRDOCS_DESCRIBE_KINDS_END`.
    @li Place the block at namespace scope, in the namespace of the base.
    @li Define `INFO` before the include. The `.inc` file is expected to
        `#undef` it at the end; if yours doesn't, add the `#undef`
        yourself.

    @param C The polymorphic base class.
*/
#define MRDOCS_DESCRIBE_KINDS_BEGIN(C)                              \
    static_assert(std::is_class_v<C>,                               \
        "MRDOCS_DESCRIBE_KINDS_BEGIN should only be used "          \
        "with class types");                                        \
    [[maybe_unused]]                                                \
    inline decltype(                                                \
        ::mrdocs::describe::detail::kind_descriptor_fn_impl(0

/** Close a kinds description opened with `MRDOCS_DESCRIBE_KINDS_BEGIN`.

    It closes the call the opening macro left open and names the
    descriptor function it declares, which registers the kinds for `C`:

    @code
    #define INFO(Name) MRDOCS_KIND_ENTRY(Shape, Name##Shape)
    MRDOCS_DESCRIBE_KINDS_BEGIN(Shape)
    #include "ShapeNodes.inc"
    MRDOCS_DESCRIBE_KINDS_END(Shape)
    @endcode

    @code
    // MRDOCS_DESCRIBE_KINDS_END(Shape) expands to exactly:
    )) mrdocs_kind_descriptor_fn(Shape**) { return {}; }
    @endcode

    `C` has to match the one passed to `MRDOCS_DESCRIBE_KINDS_BEGIN`,
    since this is where the function's parameter type, and so the base
    the kinds get registered for, comes from. It ends with a function
    body, so it needs no trailing `;`. See `MRDOCS_DESCRIBE_KINDS_BEGIN`
    for a complete example.

    @param C The polymorphic base class.
*/
#define MRDOCS_DESCRIBE_KINDS_END(C)                                \
        )) mrdocs_kind_descriptor_fn(C**) { return {}; }

#endif // MRDOCS_API_SUPPORT_REFLECTION_DESCRIBE_HPP
