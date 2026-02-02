//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_REFLECTION_MAPREFLECTEDTYPE_HPP
#define MRDOCS_LIB_SUPPORT_REFLECTION_MAPREFLECTEDTYPE_HPP

#include "ReadableTypeName.hpp"
#include <mrdocs/Dom/Array.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Specifiers/ConstexprKind.hpp>
#include <mrdocs/Metadata/Specifiers/ReferenceKind.hpp>
#include <mrdocs/Metadata/Specifiers/StorageClassKind.hpp>
#include <boost/describe.hpp>
#include <boost/mp11.hpp>
#include <locale>
#include <string_view>
#include <type_traits>
#include <vector>

namespace mrdocs {

class DomCorpus;

namespace detail {

/** Type traits to identify special types that need custom handling.
*/
template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

template <typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

template <typename T> struct is_optional : std::false_type {};
template <typename T> struct is_optional<Optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

/** Helper to determine if a member should be mapped based on its value.
*/
template <typename T>
constexpr bool
shouldMapValue(T const& value)
{
    if constexpr (is_optional_v<T>)
    {
        return value.has_value();
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        return !value.empty();
    }
    else if constexpr (std::is_same_v<T, ConstexprKind>)
    {
        return value != ConstexprKind::None;
    }
    else if constexpr (std::is_same_v<T, ReferenceKind>)
    {
        return value != ReferenceKind::None;
    }
    else if constexpr (std::is_same_v<T, StorageClassKind>)
    {
        return value != StorageClassKind::None;
    }
    else if constexpr (std::is_same_v<T, ExprInfo>)
    {
        return !value.Written.empty();
    }
    else
    {
        // All other types are always mapped.
        return true;
    }
}

/** Convert a member name to the corresponding DOM name.

    E.g.:
    - IsVariadic -> isVariadic
*/
inline
std::string
normalizeMemberName(std::string_view name)
{
    std::string result(name);
    if (!result.empty())
    {
        result.front() = std::tolower(result.front(), std::locale::classic());
    }
    return result;
}

/** Collect all base class names recursively.

    Traverses the class hierarchy using Boost.Describe and collects
    the names of all base classes. This enables templates to check
    inheritance relationships (e.g., whether a type is derived from
    Symbol or Name).
*/
template <typename T>
std::vector<std::string>
collectBaseNames()
{
    std::vector<std::string> names;
    if constexpr (boost::describe::has_describe_bases<T>::value)
    {
        boost::mp11::mp_for_each<boost::describe::describe_bases<T, boost::describe::mod_any_access>>(
            [&](auto const& descriptor)
            {
                using BaseType = typename std::decay_t<decltype(descriptor)>::type;
                constexpr std::string_view name = readableTypeName<BaseType>();
                names.emplace_back(name);
                // Recursively collect the bases of this base class.
                std::vector<std::string> const baseNames = collectBaseNames<BaseType>();
                names.insert(names.end(), baseNames.cbegin(), baseNames.cend());
            }
        );
    }
    return names;
}

/** Map a single member to the IO object.

    This function template also decides how to map the member based on its type.
*/
template <typename IO, typename T>
void
mapMember(
    IO& io,
    std::string_view name,
    T const& value,
    DomCorpus const* domCorpus)
{
    std::string const domName = detail::normalizeMemberName(name);

    if constexpr (detail::is_vector_v<T>)
    {
        // Vectors become lazy arrays — the decision is encapsulated here.
        MRDOCS_ASSERT(domCorpus != nullptr);
        io.map(domName, dom::LazyArray(value, domCorpus));
    }
    else if constexpr (dom::HasValueFromWithContext<T, DomCorpus const*>)
    {
        MRDOCS_ASSERT(domCorpus != nullptr);
        io.map(domName, dom::ValueFrom(value, domCorpus));
    }
    else
    {
        io.map(domName, dom::ValueFrom(value));
    }
}

}

/** Add a $meta object with type information.

    Creates a $meta object containing:
    - type: The unqualified C++ class name (e.g., "FunctionSymbol").
    - bases: Array of base class names (e.g., ["Symbol", "SourceInfo"]).

    The bases array allows templates to check inheritance relationships.
    For example, a template can verify if an object is derived from Symbol
    or Name without knowing the exact derived type.

    The bases array is always included (even if empty), so templates can
    safely access it.
*/
template <typename T, typename IO>
void
addMetaObject(IO& io)
{
    dom::Object meta;
    constexpr std::string_view typeName = readableTypeName<T>();
    meta.set("type", typeName);

    std::vector<std::string> const baseNames = detail::collectBaseNames<T>();
    dom::Array bases;
    for (std::string const& name : baseNames)
    {
        bases.push_back(name);
    }
    meta.set("bases", std::move(bases));

    io.map("$meta", meta);
}

/** Automatically map all Boost.Describe'd members of a type to the DOM.

    This replaces the manual `tag_invoke()` implementations with a single
    call that handles all member mappings via reflection.

    @tparam isMostDerived Whether this is the most-derived type.
                          When true, adds the $meta object.
    @param io The IO object to use for mapping.
    @param obj The object to be mapped.
    @param domCorpus The DomCorpus used to create the DOM values, or a null pointer.

    Usage in a Symbol type:

    @code
    template <class IO>
    void tag_invoke(
        dom::LazyObjectMapTag t,
        IO& io,
        FunctionSymbol const& I,
        DomCorpus const* domCorpus)
    {
        // Automatically map all members including bases.
        // Pass true for isMostDerived to add $meta.
        mapReflectedType<true>(io, I, domCorpus);
    }
    @endcode
*/
template <bool isMostDerived, typename IO, typename T>
    requires boost::describe::has_describe_members<T>::value
void
mapReflectedType(
    IO& io,
    T const& obj,
    DomCorpus const* domCorpus)
{
    if constexpr (isMostDerived)
    {
        addMetaObject<T>(io);
    }

    // First, map all bases.
    boost::mp11::mp_for_each<boost::describe::describe_bases<T, boost::describe::mod_any_access>>(
        [&](auto const& descriptor)
        {
            using BaseType = typename std::decay_t<decltype(descriptor)>::type;

            // Always use tag_invoke() - it will call mapReflectedType() internally if needed.
            tag_invoke(dom::LazyObjectMapTag{}, io, static_cast<BaseType const&>(obj), domCorpus);
        }
    );

    // Then, map all members.
    boost::mp11::mp_for_each<boost::describe::describe_members<T, boost::describe::mod_any_access>>(
        [&](auto const& descriptor) {
            using Descriptor = std::decay_t<decltype(descriptor)>;
            using MemberType = std::decay_t<decltype(obj.*Descriptor::pointer)>;

            constexpr char const* name = Descriptor::name;
            auto const& value = obj.*Descriptor::pointer;

            static_assert(
                detail::is_vector_v<MemberType> ||
                dom::HasValueFrom<MemberType, DomCorpus const*>,
                "No ValueFrom() overload found for this member type");

            if (detail::shouldMapValue(value))
            {
                detail::mapMember(io, detail::normalizeMemberName(name), value, domCorpus);
            }
        });
}

/** Map all Boost.Describe'd members without converting values.

    This version passes raw member values to `io.map()`, letting
    the IO object handle conversion with its stored context.

    @tparam isMostDerived Whether this is the most-derived type (adds $meta if true).
    @param io The IO object to use for mapping.
    @param obj The object to be mapped.
*/
template <bool isMostDerived, typename IO, typename T>
    requires boost::describe::has_describe_members<T>::value
void
mapReflectedType(
    IO& io,
    T const& obj)
{
    if constexpr (isMostDerived)
    {
        addMetaObject<T>(io);
    }

    boost::mp11::mp_for_each<boost::describe::describe_members<T, boost::describe::mod_any_access>>(
        [&](auto const& descriptor)
        {
            using Descriptor = std::decay_t<decltype(descriptor)>;
            constexpr char const* name = Descriptor::name;
            auto const& value = obj.*Descriptor::pointer;
            if (detail::shouldMapValue(value))
            {
                io.map(detail::normalizeMemberName(name), value);
            }
        });
}

}

#endif
