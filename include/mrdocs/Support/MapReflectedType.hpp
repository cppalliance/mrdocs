//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_MAPREFLECTEDTYPE_HPP
#define MRDOCS_API_SUPPORT_MAPREFLECTEDTYPE_HPP

#include <mrdocs/Dom/Array.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Specifiers/ConstexprKind.hpp>
#include <mrdocs/Metadata/Specifiers/ReferenceKind.hpp>
#include <mrdocs/Metadata/Specifiers/StorageClassKind.hpp>
#include <mrdocs/Support/Assert.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <mrdocs/Support/EnumToString.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <locale>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace mrdocs {

class DomCorpus;

namespace detail {

// --- Readable type name ---

/** Remove namespace qualifiers from a type name.

    E.g.: "mrdocs::FunctionSymbol" -> "FunctionSymbol".
*/
constexpr std::string_view
removeNamespaceQualifiers(std::string_view name)
{
    constexpr std::string_view scopeDelimiter = "::";
    std::string_view::size_type const pos = name.rfind(scopeDelimiter);
    return pos != std::string_view::npos
        ? name.substr(pos + scopeDelimiter.size())
        : name;
}

} // namespace detail

/** Get the unqualified name of a type.

    Extracts the name from __PRETTY_FUNCTION__ (Clang/GCC) or __FUNCSIG__ (MSVC).

    E.g.: readableTypeName<mrdocs::FunctionSymbol>() -> "FunctionSymbol".
*/
template <typename T>
constexpr std::string_view
readableTypeName()
{
#if defined(__clang__) || defined(__GNUC__)
    constexpr std::string_view typePrefix = "T = ";
    constexpr std::string_view fn = __PRETTY_FUNCTION__;
    constexpr std::string_view::size_type start = fn.find(typePrefix);
    static_assert(start != std::string_view::npos, "__PRETTY_FUNCTION__ does not have the expected format");

    std::string_view::size_type pos = start + typePrefix.size();
    std::string_view::size_type const end = fn.find_first_of(";]", pos);
    std::string_view const name = fn.substr(pos, end - pos);
    return detail::removeNamespaceQualifiers(name);

#elif defined(_MSC_VER)
    constexpr std::string_view funcPrefix = "readableTypeName<";
    constexpr std::string_view fn = __FUNCSIG__;
    constexpr std::string_view::size_type start = fn.find(funcPrefix);
    static_assert(start != std::string_view::npos, "__FUNCSIG__ does not have the expected format");
    std::string_view::size_type pos = start + funcPrefix.size();

    // Skip "struct ", "class ", "enum ".
    for (std::string_view const prefix : { "struct ", "class ", "enum " })
    {
        if (fn.substr(pos, prefix.size()) == prefix)
        {
            pos += prefix.size();
        }
    }

    std::string_view::size_type const end = fn.find('>', pos);
    std::string_view const name = fn.substr(pos, end - pos);
    return detail::removeNamespaceQualifiers(name);

#else
    return "unknown";
#endif
}

namespace detail {

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

    Traverses the class hierarchy using reflection and collects
    the names of all base classes. This enables templates to check
    inheritance relationships (e.g., whether a type is derived from
    Symbol or Name).
*/
template <typename T>
std::vector<std::string>
collectBaseNames()
{
    std::vector<std::string> names;
    if constexpr (describe::has_describe_bases<T>::value)
    {
        describe::for_each(
            describe::describe_bases<T>{},
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

    if constexpr (is_optional_v<T>)
    {
        // Unwrap optionals — shouldMapValue already verified has_value().
        mapMember(io, name, *value, domCorpus);
    }
    else if constexpr (detail::is_vector_v<T>)
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
    else if constexpr (describe::has_describe_enumerators<T>::value)
    {
        // Described enums map to their kebab-case string name.
        io.map(domName, toString(value));
    }
    else if constexpr (describe::has_describe_members<T>::value)
    {
        // Compound described types without a custom ValueFrom
        // become lazy objects (e.g. DocComment, SourceInfo).
        MRDOCS_ASSERT(domCorpus != nullptr);
        io.map(domName, dom::LazyObject(value, domCorpus));
    }
    else
    {
        io.map(domName, dom::ValueFrom(value));
    }
}

} // namespace detail

/** Add a $meta object with type information.

    Creates a $meta object containing:
    - type: The unqualified C++ class name (e.g., "FunctionSymbol").
    - bases: Array of base class names (e.g., ["Symbol", "SourceInfo"]).

    @tparam T The type whose metadata to add.
    @param io The lazy object IO to map into.
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

/** Automatically map all described members of a type to the DOM.

    @tparam isMostDerived Whether this is the most-derived type.
                          When true, adds the $meta object.
    @param io The IO object to use for mapping.
    @param obj The object to be mapped.
    @param domCorpus The DomCorpus used to create the DOM values, or a null pointer.
*/
template <bool isMostDerived, typename IO, typename T>
    requires describe::has_describe_members<T>::value
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
    describe::for_each(
        describe::describe_bases<T>{},
        [&](auto const& descriptor)
        {
            using BaseType = typename std::decay_t<decltype(descriptor)>::type;
            tag_invoke(dom::LazyObjectMapTag{}, io, static_cast<BaseType const&>(obj), domCorpus);
        }
    );

    // Then, map all members.
    describe::for_each(
        describe::describe_members<T>{},
        [&](auto const& descriptor) {
            using Descriptor = std::decay_t<decltype(descriptor)>;
            using MemberType = std::decay_t<decltype(obj.*Descriptor::pointer)>;

            constexpr char const* name = Descriptor::name;
            auto const& value = obj.*Descriptor::pointer;

            static_assert(
                detail::is_vector_v<MemberType> ||
                detail::is_optional_v<MemberType> ||
                dom::HasValueFrom<MemberType, DomCorpus const*>,
                "No ValueFrom() overload found for this member type");

            if (detail::shouldMapValue(value))
            {
                detail::mapMember(io, detail::normalizeMemberName(name), value, domCorpus);
            }
        });
}

/** Map all described members without converting values.

    This version passes raw member values to `io.map()`, letting
    the IO object handle conversion with its stored context.

    @tparam isMostDerived Whether this is the most-derived type (adds $meta if true).
    @param io The IO object to use for mapping.
    @param obj The object to be mapped.
*/
template <bool isMostDerived, typename IO, typename T>
    requires describe::has_describe_members<T>::value
void
mapReflectedType(
    IO& io,
    T const& obj)
{
    if constexpr (isMostDerived)
    {
        addMetaObject<T>(io);
    }

    describe::for_each(
        describe::describe_members<T>{},
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

/** Generic tag_invoke for any described type with DomCorpus context.

    Handles all types whose mapping is a straightforward
    mapReflectedType<true>. Custom overloads (more-specialized
    template or non-template) are preferred by overload resolution.

    @param io The lazy object builder to map fields into.
    @param I The described instance to serialize.
    @param domCorpus Optional corpus used to resolve cross-references.
*/
template <typename IO, typename T>
    requires describe::has_describe_members<T>::value
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    T const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

/** Generic tag_invoke for described types without context.

    Same as above but for types that don't need a DomCorpus
    (e.g. Location).

    @param io The lazy object builder to map fields into.
    @param I The described instance to serialize.
*/
template <typename IO, typename T>
    requires describe::has_describe_members<T>::value
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    T const& I)
{
    mapReflectedType<true>(io, I);
}

/** Generic ValueFrom for any described enum.

    @param e The enumerator to convert to a dom::Value string.
*/
template <typename Enum>
    requires describe::has_describe_enumerators<Enum>::value
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Enum e)
{
    v = toString(e);
}

/** Generic ValueFrom for described enums, with context.

    Same as above, ignoring the context.

    @param e The enumerator to convert to a dom::Value string.
*/
template <typename Enum, typename Context>
    requires describe::has_describe_enumerators<Enum>::value
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Enum e,
    Context const&)
{
    v = toString(e);
}

/** Generic ValueFrom for any described compound type.

    @param v The output value.
    @param I The object to convert.
    @param domCorpus The DomCorpus context.
*/
template <typename T>
    requires (describe::has_describe_members<T>::value &&
              !describe::has_describe_enumerators<T>::value)
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    T const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // namespace mrdocs

#endif // MRDOCS_API_SUPPORT_MAPREFLECTEDTYPE_HPP
