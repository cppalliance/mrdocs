//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_REFLECTION_HPP
#define MRDOCS_API_SUPPORT_REFLECTION_HPP

#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom/Array.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Specifiers/ConstexprKind.hpp>
#include <mrdocs/Metadata/Specifiers/ReferenceKind.hpp>
#include <mrdocs/Metadata/Specifiers/StorageClassKind.hpp>
#include <mrdocs/Support/Assert.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <algorithm>
#include <concepts>
#include <locale>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace mrdocs {

// ================================================================
// EnumToString
// ================================================================

/** Convert a described enumerator to string form.

    @param e The enumerator to convert.
    @return The string form of the enumerator.
*/
template <typename Enum>
    requires describe::has_describe_enumerators<Enum>::value
std::string
toString(Enum e)
{
    std::string result;
    describe::for_each(
        describe::describe_enumerators<Enum>{},
        [&](auto const& D)
        {
            if (D.value == e)
            {
                result = toKebabCase(D.name);
            }
        });

    if (!result.empty())
    {
        return result;
    }

    MRDOCS_UNREACHABLE();
}

// ================================================================
// Merge
// ================================================================

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
template <typename T, typename U>
inline constexpr bool is_polymorphic_v = false;

template <typename U>
inline constexpr bool is_polymorphic_v<Polymorphic<U>, U> = true;

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
    else if constexpr (is_polymorphic_v<T, Type>)
    {
        if (isPlaceholderType(dst))
        {
            dst = std::move(src);
        }
        return true;
    }
    // Polymorphic<Name>: take src if dst has an empty Identifier.
    else if constexpr (is_polymorphic_v<T, Name>)
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
    else if constexpr (is_optional_v<T>)
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
    else if constexpr (is_vector_v<T>)
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

// ================================================================
// DOM mapping
// ================================================================

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
    // Expected format of __PRETTY_FUNCTION__:
    // Clang: "std::string_view mrdocs::detail::readableTypeName() [T = mrdocs::FunctionSymbol]"
    // GCC:   "constexpr std::string_view mrdocs::detail::readableTypeName() [with T = mrdocs::FunctionSymbol; ...]"
    constexpr std::string_view typePrefix = "T = ";
    constexpr std::string_view fn = __PRETTY_FUNCTION__;
    constexpr std::string_view::size_type start = fn.find(typePrefix);
    static_assert(start != std::string_view::npos, "__PRETTY_FUNCTION__ does not have the expected format");

    std::string_view::size_type pos = start + typePrefix.size();
    std::string_view::size_type const end = fn.find_first_of(";]", pos);
    std::string_view const name = fn.substr(pos, end - pos);
    return detail::removeNamespaceQualifiers(name);

#elif defined(_MSC_VER)
    // Expected format of __FUNCSIG__:
    // MSVC: "... __cdecl mrdocs::detail::readableTypeName<struct mrdocs::FunctionSymbol>(void)"
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

} // namespace detail

/** Add a $meta object with type information.

    Creates a $meta object containing:
    - type: The unqualified C++ class name (e.g., "FunctionSymbol").
    - bases: Array of base class names (e.g., ["Symbol", "SourceInfo"]).

    The bases array allows templates to check inheritance relationships.
    For example, a template can verify if an object is derived from Symbol
    or Name without knowing the exact derived type.

    The bases array is always included (even if empty), so templates can
    safely access it.

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

            // Always use tag_invoke() - it will call mapReflectedType() internally if needed.
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

} // namespace mrdocs

#endif // MRDOCS_API_SUPPORT_REFLECTION_HPP
