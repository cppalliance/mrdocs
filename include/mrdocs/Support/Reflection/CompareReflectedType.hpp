//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_REFLECTION_COMPAREREFLECTEDTYPE_HPP
#define MRDOCS_API_SUPPORT_REFLECTION_COMPAREREFLECTEDTYPE_HPP

#include <mrdocs/Support/Reflection/Describe.hpp>
#include <compare>

namespace mrdocs {

/** True if T has described members (for comparison).

    Semantically equivalent to `has_describe_members<T>::value`,
    but checked via a requires-expression so that MSVC treats it
    as a wholly distinct atomic constraint. This avoids an MSVC
    bug where a constrained operator<=>() in the same namespace
    breaks constraint evaluation for the unrelated merge()
    template.
*/
template <typename T>
concept DescribedComparable = requires {
    typename describe::describe_members<T>;
};

/** Three-way comparison for any described type.

    Compares base classes first (in description order), then
    own members.
*/
template <typename T>
requires DescribedComparable<T>
std::strong_ordering
operator<=>(T const& a, T const& b)
{
    std::strong_ordering r = std::strong_ordering::equal;
    if constexpr (describe::has_describe_bases<T>::value)
    {
        describe::for_each(
            describe::describe_bases<T>{},
            [&](auto D) {
                if (std::is_eq(r))
                {
                    using Base = typename decltype(D)::type;
                    r = static_cast<Base const&>(a) <=>
                        static_cast<Base const&>(b);
                }
            });
    }
    describe::for_each(
        describe::describe_members<T>{},
        [&](auto D) {
            if (std::is_eq(r))
            {
                using M = std::remove_cvref_t<
                    decltype(a.*D.pointer)>;
                if constexpr (std::is_enum_v<M>)
                {
                    using U = std::underlying_type_t<M>;
                    r = static_cast<U>(a.*D.pointer) <=>
                        static_cast<U>(b.*D.pointer);
                }
                else
                {
                    r = a.*D.pointer <=> b.*D.pointer;
                }
            }
        });
    return r;
}

/** Equality for any described type.

    Needed because removing a defaulted member `operator<=>()`
    also removes the implicitly-generated `operator==()`.
*/
template <typename T>
requires DescribedComparable<T>
bool
operator==(T const& a, T const& b)
{
    return std::is_eq(a <=> b);
}

} // namespace mrdocs

#endif // MRDOCS_API_SUPPORT_REFLECTION_COMPAREREFLECTEDTYPE_HPP
