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

#ifndef MRDOCS_API_SUPPORT_REFLECTION_MERGEREFLECTEDTYPE_HPP
#define MRDOCS_API_SUPPORT_REFLECTION_MERGEREFLECTEDTYPE_HPP

#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <mrdocs/Support/TypeTraits/TypeTraits.hpp>
#include <algorithm>
#include <concepts>
#include <string>
#include <type_traits>
#include <vector>

namespace mrdocs {

// Forward declarations — full definitions are provided by the
// translation units that instantiate merge().
class SymbolID;
struct Type;
struct Name;

namespace detail {

/** Determine if an enum value is at its default (i.e., zero-initialized).

    Most enums in MrDocs use 0 as their "unset" value (e.g., `None`,
    `Normal`, `Struct`). This template detects that uniformly.
*/
template <typename E>
    requires std::is_enum_v<E>
constexpr bool
isDefaultEnum(E value)
{
    return static_cast<std::underlying_type_t<E>>(value) == 0;
}

// Type trait: is this Polymorphic<U> for a given U?
//
// Built alongside (not on top of) `is_specialization_of_v<T, Polymorphic>`
// from Support/TypeTraits.hpp, which asks the related question
// "is `T` some Polymorphic<...>?" without naming the inner type.
template <typename T, typename U>
inline constexpr bool is_polymorphic_for_v = false;

template <typename U>
inline constexpr bool is_polymorphic_for_v<Polymorphic<U>, U> = true;

// Type trait: can we call merge(T&, T&&) via ADL?
template <typename T, typename = void>
struct has_adl_merge : std::false_type {};

template <typename T>
struct has_adl_merge<T,
    std::void_t<decltype(merge(
        std::declval<T&>(), std::declval<T&&>()))>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_adl_merge_v = has_adl_merge<T>::value;

// Type trait: is this a vector<T> whose element type has operator==?
template <typename T>
inline constexpr bool is_equality_comparable_vector_v = false;

template <typename T, typename A>
    requires std::equality_comparable<T>
inline constexpr bool is_equality_comparable_vector_v<std::vector<T, A>> = true;

/** Check if a Polymorphic\<Type\> is in a placeholder state.

    A type is a placeholder when it holds AutoType{} (the default
    for function return types and parameters) or a blank NamedType
    with an empty Identifier (the default for variable and typedef
    types). Both represent "unknown type, to be filled in."

    Defined in Type.cpp to avoid including heavy Type headers.
*/
MRDOCS_DECL
bool
isPlaceholderType(Polymorphic<Type> const& t);

/** Merge a single member using a default type-based strategy.

    The strategies are tried in this order:

    1.  `bool`             — `dst = dst | src`
    2.  `SymbolID`         — take if invalid
    3.  ADL `merge()`      — custom merge function (ExtractionMode,
                             ExprInfo, SourceInfo, vector\<Param\>, etc.)
    4.  `Polymorphic<Type>` — take if placeholder (AutoType or blank
                             NamedType)
    5.  `Polymorphic<Name>` — take if Identifier is empty
    6.  `.Implicit` types   — take if dst is implicit
    7.  `Optional<T>`       — take if disengaged; recursive merge
                             if both engaged and T has `merge()`
    8.  `enum`              — take if zero-initialized
    9.  `string`            — take if empty
    10. `vector<T>` with `==` — dedup-append
    11. `vector<T>` fallback — take if dst is empty

    Returns `false` only for types none of the above handles.
*/
template <typename T>
bool
mergeByType(T& dst, T&& src)
{
    // bool: OR-merge (any TU seeing `true` wins).
    if constexpr (std::is_same_v<T, bool>)
    {
        dst = dst | src;
        return true;
    }
    // SymbolID: take src if dst is invalid.
    else if constexpr (std::is_same_v<T, SymbolID>)
    {
        if (!dst)
        {
            dst = src;
        }
        return true;
    }
    // ADL merge: custom merge function takes priority over
    // generic strategies. Catches ExtractionMode, ExprInfo,
    // SourceInfo, vector<Param>, vector<FriendInfo>, etc.
    else if constexpr (has_adl_merge_v<T>)
    {
        merge(dst, std::move(src));
        return true;
    }
    // Polymorphic<Type>: take src if dst is in a placeholder
    // state — either AutoType{} or a blank NamedType with an
    // empty Identifier.
    else if constexpr (is_polymorphic_for_v<T, Type>)
    {
        if (isPlaceholderType(dst))
        {
            dst = std::move(src);
        }
        return true;
    }
    // Polymorphic<Name>: take src if dst has an empty Identifier.
    else if constexpr (is_polymorphic_for_v<T, Name>)
    {
        if (dst->Identifier.empty())
        {
            dst = std::move(src);
        }
        return true;
    }
    // Types with .Implicit flag (NoexceptInfo, ExplicitInfo):
    // take src if dst is still implicit (compiler-generated).
    else if constexpr (requires(T const& t) { { t.Implicit } -> std::convertible_to<bool>; })
    {
        if (dst.Implicit)
        {
            dst = std::move(src);
        }
        return true;
    }
    // Optional<T>: take if disengaged; recursive merge if both
    // engaged and the value type has a merge() function.
    else if constexpr (is_specialization_of_v<T, Optional>)
    {
        if (!dst)
        {
            dst = std::move(src);
        }
        else if constexpr (has_adl_merge_v<typename T::value_type>)
        {
            if (src)
            {
                merge(*dst, std::move(*src));
            }
        }
        return true;
    }
    // enum: take src if dst is zero-initialized.
    else if constexpr (std::is_enum_v<T>)
    {
        if (isDefaultEnum(dst))
        {
            dst = src;
        }
        return true;
    }
    // string: take src if dst is empty.
    else if constexpr (std::is_same_v<T, std::string>)
    {
        if (dst.empty())
        {
            dst = std::move(src);
        }
        return true;
    }
    // vector<T> where T has operator==: dedup-append.
    else if constexpr (is_equality_comparable_vector_v<T>)
    {
        for (auto& elem : src)
        {
            if (std::ranges::find(dst, elem) == dst.end())
            {
                dst.push_back(std::move(elem));
            }
        }
        return true;
    }
    // vector<T> fallback: take if dst is empty.
    else if constexpr (is_specialization_of_v<T, std::vector>)
    {
        if (dst.empty())
        {
            dst = std::move(src);
        }
        return true;
    }
    else
    {
        return false;
    }
}

} // namespace detail

/** Generic merge for any described type.

    Found via ADL for any type with MRDOCS_DESCRIBE_STRUCT.
    Non-template overloads (custom merge functions) are
    preferred by overload resolution, so types with special
    merge semantics are unaffected.

    Iterates base classes (via `describe_bases`) and own members
    (via `describe_members`).

    Base classes are merged first by calling `merge(base_dst,
    base_src)`, which must be found via ADL.

    For each own member, a default merge strategy is applied
    based on the member type (see `mergeByType` for the full
    list of strategies).

    @tparam T    The type to merge (must have MRDOCS_DESCRIBE_STRUCT).
    @param  dst  The destination object.
    @param  src  The source object. Members are moved from
                 individually.
*/
template <typename T>
    requires describe::has_describe_members<T>::value
void
merge(
    T& dst,
    T&& src)
{
    // First, merge all base classes.
    describe::for_each(
        describe::describe_bases<T>{},
        [&](auto const& descriptor)
        {
            using BaseType = typename std::decay_t<decltype(descriptor)>::type;
            merge(
                static_cast<BaseType&>(dst),
                static_cast<BaseType&&>(src));
        }
    );

    // Then, merge all own members.
    describe::for_each(
        describe::describe_members<T>{},
        [&](auto const& descriptor)
        {
            using Descriptor = std::decay_t<decltype(descriptor)>;

            auto& dstMember = dst.*Descriptor::pointer;
            auto&& srcMember = std::move(src.*Descriptor::pointer);

            detail::mergeByType(dstMember, std::move(srcMember));
        });
}

} // namespace mrdocs

#endif // MRDOCS_API_SUPPORT_REFLECTION_MERGEREFLECTEDTYPE_HPP
