//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_TYPETRAITS_HPP
#define MRDOCS_API_SUPPORT_TYPETRAITS_HPP

#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <type_traits>
#include <vector>


namespace mrdocs {

/** Whether `T` is a specialization of the class template `Template`.

    @tparam T The type to test.
    @tparam Template The class template to test against.
*/
template <typename T, template <typename...> class Template>
inline constexpr bool is_specialization_of_v = false;

/// @copydoc is_specialization_of_v
template <template <typename...> class Template, typename... Args>
inline constexpr bool is_specialization_of_v<Template<Args...>, Template> = true;

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

/** Dependent alias of `T`, useful for delaying instantiation.
*/
template<typename T, typename U>
struct make_dependent
{
    /** Make a type dependent on another template parameter.
    */
    using type = T;
};

/** Alias for `make_dependent<T, U>::type`.
    @tparam T Type to make dependent.
    @tparam U Dependent parameter.
*/
template<typename T, typename U>
using make_dependent_t = make_dependent<T, U>::type;

// ----------------------------------------------------------------

/** Propagate lvalue-reference from `From` to `To` if present.
*/
template<typename From, typename To>
struct add_lvalue_reference_from : std::conditional<
    std::is_lvalue_reference_v<From>,
        std::add_lvalue_reference_t<To>, To> { };

/** Alias for `add_lvalue_reference_from<From, To>::type`.
    @tparam From Source type providing reference qualifier.
    @tparam To Destination type to adjust.
*/
template<typename From, typename To>
using add_lvalue_reference_from_t =
    add_lvalue_reference_from<From, To>::type;

// ----------------------------------------------------------------

/** Propagate rvalue-reference from `From` to `To` if present.
*/
template<typename From, typename To>
struct add_rvalue_reference_from : std::conditional<
    std::is_rvalue_reference_v<From>,
        std::add_rvalue_reference_t<To>, To> { };

/** Alias for `add_rvalue_reference_from<From, To>::type`.
    @tparam From Source type providing reference qualifier.
    @tparam To Destination type to adjust.
*/
template<typename From, typename To>
using add_rvalue_reference_from_t =
    add_rvalue_reference_from<From, To>::type;

// ----------------------------------------------------------------

/** Propagate reference qualification from `From` to `To`.
*/
template<typename From, typename To>
struct add_reference_from
    : add_lvalue_reference_from<From,
        add_rvalue_reference_from_t<From, To>> { };

/** Alias for `add_reference_from<From, To>::type`.
    @tparam From Source type providing reference qualifier.
    @tparam To Destination type to adjust.
*/
template<typename From, typename To>
using add_reference_from_t =
    add_reference_from<From, To>::type;

// ----------------------------------------------------------------

/** Propagate const qualification from `From` to `To`, keeping references.
*/
template<typename From, typename To>
struct add_const_from : std::conditional<
    std::is_const_v<std::remove_reference_t<From>>,
        add_reference_from_t<To, const std::remove_reference_t<To>>, To> { };

/** Alias for `add_const_from<From, To>::type`.
    @tparam From Source type providing const qualifier.
    @tparam To Destination type to adjust.
*/
template<typename From, typename To>
using add_const_from_t =
    add_const_from<From, To>::type;

// ----------------------------------------------------------------

/** Propagate volatile qualification from `From` to `To`, keeping references.
*/
template<typename From, typename To>
struct add_volatile_from : std::conditional<
    std::is_volatile_v<std::remove_reference_t<From>>,
        add_reference_from_t<To, volatile std::remove_reference_t<To>>, To> { };


/** Alias for `add_volatile_from<From, To>::type`.
    @tparam From Source type providing volatile qualifier.
    @tparam To Destination type to adjust.
*/
template<typename From, typename To>
using add_volatile_from_t =
    add_volatile_from<From, To>::type;

// ----------------------------------------------------------------

/** Propagate both const and volatile qualifiers from `From` to `To`.
*/
template<typename From, typename To>
struct add_cv_from
    : add_const_from<From,
        add_volatile_from_t<From, To>> { };

/** Alias for `add_cv_from<From, To>::type`.
    @tparam From Source type providing cv qualifiers.
    @tparam To Destination type to adjust.
*/
template<typename From, typename To>
using add_cv_from_t =
    add_cv_from<From, To>::type;

// ----------------------------------------------------------------

/** Propagate cv-qualification and reference category from `From` to `To`.
*/
template<typename From, typename To>
struct add_cvref_from
    : add_reference_from<From,
        add_cv_from_t<From, To>> { };

/** Alias for `add_cvref_from<From, To>::type`.
    @tparam From Source type providing cv-ref qualifiers.
    @tparam To Destination type to adjust.
*/
template<typename From, typename To>
using add_cvref_from_t =
    add_cvref_from<From, To>::type;

} // mrdocs


#endif // MRDOCS_API_SUPPORT_TYPETRAITS_HPP
