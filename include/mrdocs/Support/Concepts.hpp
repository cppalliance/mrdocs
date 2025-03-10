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

namespace clang::mrdocs {

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

} // namespace clang::mrdocs


#endif // MRDOCS_API_SUPPORT_CONCEPTS_HPP
