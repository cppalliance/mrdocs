//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_TYPETRAITS_HPP
#define MRDOCS_API_SUPPORT_TYPETRAITS_HPP

#include <type_traits>

namespace clang {
namespace mrdocs {

/** Return the value as its underlying type.

    @param value The enum value to convert
*/
template<class Enum>
requires std::is_enum_v<Enum>
constexpr auto
to_underlying(
    Enum value) noexcept ->
    std::underlying_type_t<Enum>
{
    return static_cast<
        std::underlying_type_t<Enum>>(value);
}

// ----------------------------------------------------------------

template<typename T, typename U>
struct make_dependent
{
    using type = T;
};

template<typename T, typename U>
using make_dependent_t = make_dependent<T, U>::type;

// ----------------------------------------------------------------

template<typename From, typename To>
struct add_lvalue_reference_from : std::conditional<
    std::is_lvalue_reference_v<From>,
        std::add_lvalue_reference_t<To>, To> { };

template<typename From, typename To>
using add_lvalue_reference_from_t =
    add_lvalue_reference_from<From, To>::type;

// ----------------------------------------------------------------

template<typename From, typename To>
struct add_rvalue_reference_from : std::conditional<
    std::is_rvalue_reference_v<From>,
        std::add_rvalue_reference_t<To>, To> { };

template<typename From, typename To>
using add_rvalue_reference_from_t =
    add_rvalue_reference_from<From, To>::type;

// ----------------------------------------------------------------

template<typename From, typename To>
struct add_reference_from
    : add_lvalue_reference_from<From,
        add_rvalue_reference_from_t<From, To>> { };

template<typename From, typename To>
using add_reference_from_t =
    add_reference_from<From, To>::type;

// ----------------------------------------------------------------

template<typename From, typename To>
struct add_const_from : std::conditional<
    std::is_const_v<std::remove_reference_t<From>>,
        add_reference_from_t<To, const std::remove_reference_t<To>>, To> { };

template<typename From, typename To>
using add_const_from_t =
    add_const_from<From, To>::type;

// ----------------------------------------------------------------

template<typename From, typename To>
struct add_volatile_from : std::conditional<
    std::is_volatile_v<std::remove_reference_t<From>>,
        add_reference_from_t<To, volatile std::remove_reference_t<To>>, To> { };


template<typename From, typename To>
using add_volatile_from_t =
    add_volatile_from<From, To>::type;

// ----------------------------------------------------------------

template<typename From, typename To>
struct add_cv_from
    : add_const_from<From,
        add_volatile_from_t<From, To>> { };

template<typename From, typename To>
using add_cv_from_t =
    add_cv_from<From, To>::type;

// ----------------------------------------------------------------

template<typename From, typename To>
struct add_cvref_from
    : add_reference_from<From,
        add_cv_from_t<From, To>> { };

template<typename From, typename To>
using add_cvref_from_t =
    add_cvref_from<From, To>::type;

} // mrdocs
} // clang

#endif // MRDOCS_API_SUPPORT_TYPETRAITS_HPP
