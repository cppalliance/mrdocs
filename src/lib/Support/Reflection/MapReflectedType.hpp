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

/** Helper to determine if a member should be mapped based on its value.
*/
template <typename T>
constexpr bool
shouldMapValue(T const& value)
{
    if constexpr (std::is_same_v<T, std::string>)
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
    // Special cases.
    if (name == "Constexpr")
    {
        return "constexprKind";
    }
    else if (name == "ReturnType")
    {
        return "return";
    }
    else if (name == "Noexcept")
    {
        return "exceptionSpec";
    }
    else if (name == "Explicit")
    {
        return "explicitSpec";
    }
    else if (name == "KeyKind")
    {
        return "tag";
    }
    else if (name == "Class")
    {
        return "usingClass";
    }
    else
    {
        std::string result(name);
        if (!result.empty())
        {
            result.front() = std::tolower(result.front(), std::locale::classic());
        }
        return result;
    }
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

/** Automatically map all Boost.Describe'd members of a type to the DOM.

    This replaces the manual `tag_invoke()` implementations with a single
    call that handles all member mappings via reflection.

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
        // First, map base Symbol members.
        tag_invoke(t, io, I.asInfo(), domCorpus);

        // Then, automatically map all FunctionSymbol-specific members.
        mapReflectedType(io, I, domCorpus);
    }
    @endcode
*/
template <typename IO, typename T>
    requires boost::describe::has_describe_members<T>::value
void
mapReflectedType(
    IO& io,
    T const& obj,
    DomCorpus const* domCorpus)
{
    // First, map all bases.
    boost::mp11::mp_for_each<boost::describe::describe_bases<T, boost::describe::mod_any_access>>(
        [&](auto const& descriptor)
        {
            using BaseType = typename std::decay_t<decltype(descriptor)>::type;

            if constexpr (boost::describe::has_describe_members<BaseType>::value)
            {
                // Base is described: recurse.
                mapReflectedType(io, static_cast<BaseType const&>(obj), domCorpus);
            }
            else
            {
                // Base is not described: map directly.
                tag_invoke(dom::LazyObjectMapTag{}, io, static_cast<BaseType const&>(obj), domCorpus);
            }
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

    @param io The IO object to use for mapping.
    @param obj The object to be mapped.
*/
template <typename IO, typename T>
    requires boost::describe::has_describe_members<T>::value
void
mapReflectedType(
    IO& io,
    T const& obj)
{
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
