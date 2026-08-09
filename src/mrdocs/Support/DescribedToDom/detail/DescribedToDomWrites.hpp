//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_DETAIL_WRITES_HPP
#define MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_DETAIL_WRITES_HPP

#include <mrdocs/Support/DescribedToDom/DescribedToDomForward.hpp>
#include <mrdocs/Support/DescribedToDom/detail/DescribedToDomDetail.hpp>

namespace mrdocs {

//------------------------------------------------
//
// Reflection-driven writes
//
// Assigns a dom::Value into any described field of a described object.
// The proxy `set` paths use it to write through to the underlying C++
// object.
//
//------------------------------------------------

namespace detail::described_write {

// =====================================================================
// Polymorphic kind name matching
// =====================================================================

/** Return the kebab-case name of `Derived`'s kind discriminator.

    A script identifies a concrete kind by a kebab-case name (e.g.
    `"named-type"`). This returns that name for the fixed kind that
    `Derived` represents: it default-constructs a `Derived` to read its
    `Kind` member and renders it. For a discriminator described with
    `MRDOCS_DESCRIBE_ENUM` the name comes from the matching enumerator;
    `TypeKind` is deliberately undescribed, so its `toString(TypeKind)`
    overload is used instead. A `Derived` that is not default-constructible
    yields the empty string (which never matches a script-supplied name).

    @tparam Derived A concrete kind whose `Kind` member is fixed.
    @return The discriminator's kebab-case name, or empty if unavailable.
*/
template <typename Derived>
std::string
computeKindKebabName()
{
    if constexpr (!std::is_default_constructible_v<Derived>)
    {
        return {};
    }
    else
    {
        Derived instance;
        using KindType = std::decay_t<decltype(instance.Kind)>;
        if constexpr (describe::has_describe_enumerators<KindType>::value)
        {
            std::string result;
            describe::for_each(
                describe::describe_enumerators<KindType>{},
                [&](auto enumDesc)
                {
                    if (!result.empty())
                    {
                        return;
                    }
                    if (enumDesc.value == instance.Kind)
                    {
                        result = toKebabCase(enumDesc.name);
                    }
                });
            return result;
        }
        else if constexpr (requires { toString(instance.Kind); })
        {
            return std::string(toString(instance.Kind));
        }
        else
        {
            return {};
        }
    }
}

/** Whether `name` is the kebab-case kind name of `Derived`.

    Compares `name` against @ref computeKindKebabName for `Derived`,
    caching the computed name so the (per-process constant) result is
    rendered only once.

    @tparam Derived A concrete kind whose `Kind` member is fixed.
    @param name The script-supplied kebab-case name to test.
    @return `true` when `name` matches `Derived`'s kind name.
*/
template <typename Derived>
bool
isKindKebabName(std::string_view name)
{
    static std::string const cached = computeKindKebabName<Derived>();
    return !cached.empty() && cached == name;
}

// =====================================================================
// assignFromDom overload set
// =====================================================================

template <typename T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

inline
Expected<void>
assignFromDom(
    std::string& dest, std::string_view fieldName, dom::Value const& src);

inline
Expected<void>
assignFromDom(
    bool& dest, std::string_view fieldName, dom::Value const& src);

inline
Expected<void>
assignFromDom(
    SymbolID& dest, std::string_view fieldName, dom::Value const& src);

template <typename T>
    requires (std::is_enum_v<T> && describe::has_describe_enumerators<T>::value)
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

template <mrdocs::specialization_of<mrdocs::Optional> T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

template <mrdocs::specialization_of<std::vector> T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

template <mrdocs::specialization_of<mrdocs::Polymorphic> T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

template <typename T>
    requires describe::has_describe_members<T>::value
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

template <typename T>
std::optional<Expected<void>>
trySetMember(T& obj, std::string_view fieldName, dom::Value const& src);

template <typename Poly, typename Place>
Expected<void>
buildPolymorphic(
    dom::Value const& src, std::string_view fieldName, Place&& place);

// =====================================================================
// assignFromDom definitions
// =====================================================================

template <typename T>
Expected<void>
assignFromDom(T&, std::string_view fieldName, dom::Value const&)
{
    return Unexpected(formatError(
        "field '{}' has a type the generic setter cannot yet write",
        fieldName));
}

inline
Expected<void>
assignFromDom(
    std::string& dest, std::string_view fieldName, dom::Value const& src)
{
    if (!src.isString())
    {
        return Unexpected(formatError(
            "field '{}' expects a string", fieldName));
    }
    dest = std::string(src.getString());
    return {};
}

inline
Expected<void>
assignFromDom(
    bool& dest, std::string_view fieldName, dom::Value const& src)
{
    if (!src.isBoolean())
    {
        return Unexpected(formatError(
            "field '{}' expects a boolean", fieldName));
    }
    dest = src.getBool();
    return {};
}

inline
Expected<void>
assignFromDom(
    SymbolID& dest, std::string_view fieldName, dom::Value const& src)
{
    if (!src.isString())
    {
        return Unexpected(formatError(
            "field '{}' expects a base58 SymbolID string",
            fieldName));
    }
    auto const id = fromBase58Str(src.getString());
    if (!id)
    {
        return Unexpected(formatError(
            "field '{}' expects a valid base58 SymbolID", fieldName));
    }
    dest = *id;
    return {};
}

template <typename T>
    requires (std::is_enum_v<T> && describe::has_describe_enumerators<T>::value)
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src)
{
    if (!src.isString())
    {
        return Unexpected(formatError(
            "field '{}' expects an enumerator name", fieldName));
    }
    std::string_view const candidate = src.getString();
    bool found = false;
    describe::for_each(
        describe::describe_enumerators<T>{},
        [&](auto const& D)
        {
            if (found) { return; }
            if (toKebabCase(D.name) == candidate)
            {
                dest = D.value;
                found = true;
            }
        });
    if (!found)
    {
        return Unexpected(formatError(
            "field '{}' has no enumerator named '{}'",
            fieldName, candidate));
    }
    return {};
}

template <mrdocs::specialization_of<mrdocs::Optional> T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src)
{
    if (src.isNull())
    {
        dest.reset();
        return {};
    }
    using Inner = std::remove_reference_t<decltype(*dest)>;
    if constexpr (std::is_default_constructible_v<Inner>)
    {
        if (!dest.has_value())
        {
            dest.emplace();
        }
        return assignFromDom(*dest, fieldName, src);
    }
    else
    {
        return Unexpected(formatError(
            "field '{}' wraps a type the generic setter cannot construct "
            "(only `null` is accepted)", fieldName));
    }
}

template <mrdocs::specialization_of<std::vector> T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src)
{
    using Element = typename T::value_type;

    // A bare string assigned to a vector field is shorthand for
    // "a singleton vector containing that string, built however
    // the element type accepts a string." For `vector<Polymorphic<Inline>>`
    // (typically `children` on inline containers) that gives a one-element
    // sequence with `TextInline{string}`; for `vector<ParamBlock>` and
    // similar named-block sequences it gives one block whose inline
    // content is the string and whose name stays empty.
    if (src.isString())
    {
        T fresh;
        fresh.reserve(1);
        if constexpr (mrdocs::specialization_of<Element, mrdocs::Polymorphic>)
        {
            Expected<void> r = buildPolymorphic<Element>(
                src, fieldName,
                [&](Element&& v) { fresh.push_back(std::move(v)); });
            if (!r) { return Unexpected(r.error()); }
        }
        else if constexpr (std::is_default_constructible_v<Element>
                        && std::is_assignable_v<Element&, std::string_view>)
        {
            Element elem{};
            elem = std::string_view(src.getString());
            fresh.push_back(std::move(elem));
        }
        else
        {
            return Unexpected(formatError(
                "field '{}' contains a type that cannot be built "
                "from a bare string", fieldName));
        }
        dest = std::move(fresh);
        return {};
    }

    if (!src.isArray())
    {
        return Unexpected(formatError(
            "field '{}' expects an array", fieldName));
    }
    dom::Array const arr = src.getArray();
    std::size_t const n = arr.size();
    T fresh;
    fresh.reserve(n);
    if constexpr (mrdocs::specialization_of<Element, mrdocs::Polymorphic>)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            Expected<void> r = buildPolymorphic<Element>(
                arr.get(i), fieldName,
                [&](Element&& v) { fresh.push_back(std::move(v)); });
            if (!r) { return Unexpected(r.error()); }
        }
    }
    else if constexpr (std::is_default_constructible_v<Element>)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            Element elem{};
            Expected<void> r = assignFromDom(elem, fieldName, arr.get(i));
            if (!r) { return r; }
            fresh.push_back(std::move(elem));
        }
    }
    else
    {
        return Unexpected(formatError(
            "field '{}' contains a type the generic setter cannot construct",
            fieldName));
    }
    dest = std::move(fresh);
    return {};
}

template <mrdocs::specialization_of<mrdocs::Polymorphic> T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src)
{
    return buildPolymorphic<T>(
        src, fieldName,
        [&](T&& v) { dest = std::move(v); });
}

template <typename T>
    requires describe::has_describe_members<T>::value
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src)
{
    // Described structs that expose `operator=(std::string_view)` (the
    // `InlineContainer`-derived blocks: `BriefBlock`, paragraphs, params,
    // etc.) accept a bare string as shorthand for the single-text-child
    // shape they would otherwise be assembled by hand from an object
    // literal. `sym.doc.brief = "..."` is the natural way to write a
    // brief that is just one sentence.
    if (src.isString())
    {
        if constexpr (std::is_assignable_v<T&, std::string_view>)
        {
            dest = std::string_view(src.getString());
            return {};
        }
        return Unexpected(formatError(
            "field '{}' expects an object (this type is not "
            "string-assignable)", fieldName));
    }
    if (!src.isObject())
    {
        return Unexpected(formatError(
            "field '{}' expects an object", fieldName));
    }
    dom::Object const obj = src.getObject();
    Expected<void> outerResult;
    obj.visit(
        [&](dom::String key, dom::Value value) -> bool
        {
            std::optional<Expected<void>> inner =
                trySetMember<T>(dest, key, value);
            if (!inner.has_value())
            {
                outerResult = Unexpected(formatError(
                    "field '{}': unknown sub-field '{}'", fieldName, key));
                return false;
            }
            if (!inner.value())
            {
                outerResult = Unexpected(inner.value().error());
                return false;
            }
            return true;
        });
    return outerResult;
}

template <typename T>
std::optional<Expected<void>>
trySetMember(T& obj, std::string_view fieldName, dom::Value const& src)
{
    std::optional<Expected<void>> outcome;
    if constexpr (describe::has_describe_members<T>::value)
    {
        describe::for_each(
            describe::describe_members<T>{},
            [&](auto const& descriptor)
            {
                if (outcome.has_value()) { return; }
                using Descriptor = std::decay_t<decltype(descriptor)>;
                std::string const normalized =
                    dom::detail::normalizeMemberName(Descriptor::name);
                if (normalized != fieldName) { return; }
                outcome = assignFromDom(
                    obj.*Descriptor::pointer, fieldName, src);
            });
    }
    if (outcome.has_value()) { return outcome; }
    if constexpr (describe::has_describe_bases<T>::value)
    {
        describe::for_each(
            describe::describe_bases<T>{},
            [&](auto const& descriptor)
            {
                if (outcome.has_value()) { return; }
                using BaseType =
                    typename std::decay_t<decltype(descriptor)>::type;
                outcome = trySetMember<BaseType>(
                    static_cast<BaseType&>(obj), fieldName, src);
            });
    }
    return outcome;
}

template <typename Derived>
Expected<void>
applyDerivedFields(
    Derived& instance,
    dom::Object const& obj,
    std::string_view fieldName,
    std::string_view kindStr)
{
    Expected<void> result;
    obj.visit(
        [&](dom::String key, dom::Value value) -> bool
        {
            if (key == "kind") { return true; }
            std::optional<Expected<void>> outcome =
                trySetMember<Derived>(instance, key, value);
            if (!outcome.has_value())
            {
                result = Unexpected(formatError(
                    "field '{}': unknown sub-field '{}' for kind '{}'",
                    fieldName, key, kindStr));
                return false;
            }
            if (!outcome.value())
            {
                result = Unexpected(outcome.value().error());
                return false;
            }
            return true;
        });
    return result;
}

template <typename Poly, typename Derived, typename Place>
Expected<void>
tryBuildDerived(
    dom::Object const& obj,
    std::string_view fieldName,
    std::string_view kindStr,
    Place& place)
{
    if constexpr (std::is_default_constructible_v<Derived>)
    {
        Derived instance;
        Expected<void> r = applyDerivedFields<Derived>(
            instance, obj, fieldName, kindStr);
        if (!r) { return Unexpected(r.error()); }
        place(Poly(std::in_place_type<Derived>, std::move(instance)));
        return {};
    }
    else
    {
        return Unexpected(formatError(
            "field '{}': derived kind '{}' is not default-constructible",
            fieldName, kindStr));
    }
}

template <typename Poly, typename Place>
Expected<void>
buildPolymorphic(
    dom::Value const& src, std::string_view fieldName, Place&& place)
{
    using Base = typename Poly::value_type;

    // A bare string is shorthand for "the text kind, with the string
    // as its sole content". The convention is one specific derived
    // class registered under the kebab-name `text`: for an `Inline`
    // base that lands on `TextInline`, for any other base it must
    // expose the same kind name. Without it, a string source has no
    // unambiguous derived class to target and is rejected.
    if (src.isString())
    {
        if constexpr (!describe::has_describe_kinds<Base>::value)
        {
            return Unexpected(formatError(
                "field '{}' is a polymorphic base with no described kinds",
                fieldName));
        }
        else
        {
            Expected<void> result = Unexpected(formatError(
                "field '{}': no 'text' kind registered to accept a "
                "bare string", fieldName));
            bool matched = false;
            describe::for_each(
                describe::describe_kinds<Base>{},
                [&](auto descriptor)
                {
                    if (matched) { return; }
                    using Descriptor = std::decay_t<decltype(descriptor)>;
                    using Derived = typename Descriptor::type;
                    if (!isKindKebabName<Derived>("text")) { return; }
                    matched = true;
                    if constexpr (std::is_constructible_v<
                        Derived, std::string_view>)
                    {
                        Derived instance{std::string_view(src.getString())};
                        place(Poly(std::in_place_type<Derived>,
                            std::move(instance)));
                        result = {};
                    }
                    else
                    {
                        result = Unexpected(formatError(
                            "field '{}': 'text' kind is not "
                            "constructible from a string", fieldName));
                    }
                });
            return result;
        }
    }

    if (!src.isObject())
    {
        return Unexpected(formatError(
            "field '{}' expects an object describing a polymorphic value",
            fieldName));
    }
    dom::Object const obj = src.getObject();
    dom::Value kindV = obj.get("kind");
    if (!kindV.isString())
    {
        return Unexpected(formatError(
            "field '{}' expects an object with a string `kind` field",
            fieldName));
    }
    std::string_view const kindStr = kindV.getString();

    if constexpr (!describe::has_describe_kinds<Base>::value)
    {
        return Unexpected(formatError(
            "field '{}' is a polymorphic base with no described kinds",
            fieldName));
    }
    else
    {
        Expected<void> result = Unexpected(formatError(
            "field '{}' has no derived class with kind '{}'",
            fieldName, kindStr));
        bool matched = false;
        describe::for_each(
            describe::describe_kinds<Base>{},
            [&](auto descriptor)
            {
                if (matched) { return; }
                using Descriptor = std::decay_t<decltype(descriptor)>;
                using Derived = typename Descriptor::type;
                if (!isKindKebabName<Derived>(kindStr)) { return; }
                matched = true;
                result = tryBuildDerived<Poly, Derived>(
                    obj, fieldName, kindStr, place);
            });
        return result;
    }
}

} // namespace detail::described_write

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_DETAIL_WRITES_HPP
