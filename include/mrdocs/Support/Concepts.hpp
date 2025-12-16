//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_CONCEPTS_HPP
#define MRDOCS_API_SUPPORT_CONCEPTS_HPP

#include <ranges>

namespace mrdocs {

/** Concept to check if a type is a range of T */
template <class Range, class T>
concept range_of = std::ranges::range<Range> && std::same_as<std::ranges::range_value_t<Range>, T>;

/** Concept to check if a type is representing a polymorphic storage

    This concept checks if a type is used to store derived objects
    of a base class.

    Examples of such types are std::unique_ptr, std::shared_ptr,
    Polymorphic, etc.

    The `get()` function might not always be available, but
    `operator*` and `operator->` should be available and return
    a reference to the Base class.
 */
template <class T, class Base>
concept polymorphic_storage_for = requires(T const& t)
{
    { *t } -> std::convertible_to<Base const&>;
    { t.operator->() } -> std::convertible_to<Base const*>;
};

/** Determine if a type is dereferenceable

    This concept checks if a type can be dereferenced
    to a value it represents and converted to a boolean
    value that represents if the object is in a valid state.

    Examples of such types are std::optional, std::unique_ptr,
    std::shared_ptr, Polymorphic, pointers, etc.
 */
template <class T>
concept dereferenceable = requires(T const& t)
{
    { static_cast<bool>(t) };
    { *t };
};

namespace detail {
// ---------- step 1: detect if std::tuple_size<U> is specialized (SFINAE-safe)

template<class U, class = void>
struct has_tuple_size : std::false_type {};

// This partial specialization is considered only if the operand inside void_t
// is well-formed; otherwise, substitution fails quietly (no hard error).
template <class U>
struct has_tuple_size<
    U,
    std::void_t<decltype(std::tuple_size<std::remove_cvref_t<U>>::value)>>
    : std::true_type {};

// ---------- step 2: if tuple_size exists, verify all tuple_element<I, U> exist
template<class U, class = void>
struct all_tuple_elements : std::false_type {};

// Enabled only when tuple_size<U> is known to exist (avoids hard errors)
template <class U>
struct all_tuple_elements<U, std::enable_if_t<has_tuple_size<U>::value>> {
    using T = std::remove_cvref_t<U>;
    static constexpr std::size_t N = std::tuple_size<T>::value;

    template <std::size_t... I>
    static consteval bool
    check(std::index_sequence<I...>)
    {
        return (requires { typename std::tuple_element<I, T>::type; } && ...);
    }

    static constexpr bool value = check(std::make_index_sequence<N>{});
};
}

/** Concept to check if a type is tuple-like

    This concept checks if a type has a specialization
    of std::tuple_size and std::tuple_element for all
    elements in the range [0, N), where N is the value
    of std::tuple_size.

    Examples of such types are std::tuple, std::pair,
    std::array, and user-defined types that provide
    specializations for std::tuple_size and std::tuple_element.
 */
template<class T>
concept tuple_like =
    detail::has_tuple_size<T>::value &&
    detail::all_tuple_elements<T>::value;

/** Concept to check if a type is pair-like

    This concept checks if a type is tuple-like
    and has exactly two elements.

    Examples of such types are std::pair,
    std::array with two elements, and user-defined
    types that provide specializations for
    std::tuple_size and std::tuple_element with
    exactly two elements.
 */
template<class T>
concept pair_like =
    tuple_like<T> &&
    (std::tuple_size<std::remove_cvref_t<T>>::value == 2);

/** Concept to check if a range is a range of tuple-like elements

    This concept checks if a type is a range
    and all its elements are tuple-like.

    Examples of such types are std::vector<std::tuple<...>>,
    std::list<std::pair<...>>, and user-defined types
    that provide specializations for std::tuple_size
    and std::tuple_element for their element type.
 */
template <class Range>
concept range_of_tuple_like =
    std::ranges::range<Range> && tuple_like<std::ranges::range_value_t<Range>>;

#ifdef __cpp_lib_reference_from_temporary
    using std::reference_constructs_from_temporary_v;
    using std::reference_converts_from_temporary_v;
#else
    template <class To, class From>
    concept reference_converts_from_temporary_v
        = std::is_reference_v<To>
          && ((!std::is_reference_v<From>
               && std::is_convertible_v<
                   std::remove_cvref_t<From>*,
                   std::remove_cvref_t<To>*>)
              || (std::is_lvalue_reference_v<To>
                  && std::is_const_v<std::remove_reference_t<To>>
                  && std::is_convertible_v<From, const std::remove_cvref_t<To>&&>
                  && !std::is_convertible_v<From, std::remove_cvref_t<To>&>) );

    template <class To, class From>
    concept reference_constructs_from_temporary_v
        = reference_converts_from_temporary_v<To, From>;
#endif

} // namespace mrdocs


#endif // MRDOCS_API_SUPPORT_CONCEPTS_HPP
