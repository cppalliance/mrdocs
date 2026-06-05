//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_DESCRIBEDOBJECTPROXY_HPP
#define MRDOCS_LIB_METADATA_DESCRIBEDOBJECTPROXY_HPP

#include <mrdocs/Dom/Array.hpp>
#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <mrdocs/Support/DescribeKinds.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <mrdocs/Support/String.hpp>
#include <mrdocs/Support/TypeTraits.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mrdocs {

template <class T> class DescribedObjectProxy;
template <class T> class DescribedArrayProxy;

/** Convert a described value to a `dom::Value`.

    Wraps `value` in the appropriate proxy
    (@ref DescribedObjectProxy or @ref DescribedArrayProxy)
    when `T` is a described struct or a range of one, or
    forwards the value through `dom::ValueFrom` otherwise.

    @param value The C++ value to wrap. The returned `dom::Value`
                 holds a non-owning reference to `value`.
*/
template <class T>
dom::Value
describedValueToDom(T& value);

namespace dom::detail {

// Lowercase the first character. Mirrors the convention used by the
// reflection machinery elsewhere: described member `Name` becomes
// the script-facing key `name`, `IsConst` becomes `isConst`, etc.
inline
std::string
normalizeMemberName(std::string_view name)
{
    std::string out(name);
    if (!out.empty() && out[0] >= 'A' && out[0] <= 'Z')
    {
        out[0] = static_cast<char>(out[0] - 'A' + 'a');
    }
    return out;
}

} // namespace dom::detail

//------------------------------------------------
//
// Reflection-driven setter machinery
//
// Lifted from the old SetMember.cpp. Lives here so the templated
// proxy `set` paths can reach it for any described type, not just
// the top-level `Symbol`.
//
//------------------------------------------------

namespace detail::described_setter {

// =====================================================================
// Allowlist
// =====================================================================
//
// Only the fields listed below can be written through the proxy when
// the target is a top-level Symbol, regardless of what described
// members the underlying type happens to expose. The allowlist starts
// strict and grows as concrete needs surface; this keeps scripts from
// quietly breaking corpus invariants (changing a symbol's `kind`,
// re-parenting it, mutating structural collections). Nested writes
// (already inside an allowlisted field) bypass this gate.

inline constexpr std::string_view kSettableFields[] = {
    "name",
    "extraction",
    "isCopyFromInherited",
    "loc",
    "doc",
    "returnType",
};

inline
bool
isSettableField(std::string_view fieldName)
{
    return std::find(
        std::begin(kSettableFields),
        std::end(kSettableFields),
        fieldName) != std::end(kSettableFields);
}

// =====================================================================
// Polymorphic kind cache
// =====================================================================
//
// Compute the script-facing kebab name for `Derived`'s discriminator
// exactly once per process. Two address schemes coexist: most
// hierarchies use `MRDOCS_DESCRIBE_ENUM`, so the kebab name comes
// from the enumerator; `TypeKind` is deliberately undescribed and
// uses the existing `toString(TypeKind)` overload instead. Derived
// classes that aren't default-constructible return the empty string
// (which compares unequal to any non-empty script-supplied name).

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

template <typename T>
    requires mrdocs::detail::is_optional_v<T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

template <typename T>
    requires mrdocs::detail::is_vector_v<T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

template <typename T>
    requires mrdocs::detail::is_polymorphic_v<T>
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
            "field '{}' expects a base16 SymbolID string",
            fieldName));
    }
    std::string_view const s = src.getString();
    constexpr std::size_t kIdBytes = 20;
    constexpr std::size_t kHexLen = kIdBytes * 2;
    if (s.size() != kHexLen)
    {
        return Unexpected(formatError(
            "field '{}' expects a {}-character base16 SymbolID, got {} characters",
            fieldName, kHexLen, s.size()));
    }
    auto const decode = [](char c) -> int
    {
        if (c >= '0' && c <= '9') { return c - '0'; }
        if (c >= 'a' && c <= 'f') { return 10 + (c - 'a'); }
        if (c >= 'A' && c <= 'F') { return 10 + (c - 'A'); }
        return -1;
    };
    SymbolID::value_type bytes[kIdBytes] = {};
    for (std::size_t i = 0; i < kIdBytes; ++i)
    {
        int const hi = decode(s[(i * 2) + 0]);
        int const lo = decode(s[(i * 2) + 1]);
        if (hi < 0 || lo < 0)
        {
            return Unexpected(formatError(
                "field '{}' has an invalid base16 character",
                fieldName));
        }
        bytes[i] = static_cast<SymbolID::value_type>((hi << 4) | lo);
    }
    dest = SymbolID(bytes);
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

template <typename T>
    requires mrdocs::detail::is_optional_v<T>
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

template <typename T>
    requires mrdocs::detail::is_vector_v<T>
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
        if constexpr (mrdocs::detail::is_polymorphic_v<Element>)
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
    if constexpr (mrdocs::detail::is_polymorphic_v<Element>)
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

template <typename T>
    requires mrdocs::detail::is_polymorphic_v<T>
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

} // namespace detail::described_setter

//------------------------------------------------
//
// DescribedObjectProxy<T>
//
// Templated reflection-driven proxy over a described C++ object.
// Reads walk the described members via `describe::*` and convert
// each field through `describedValueToDom`. Writes go through the
// reflective setter; a write that targets a top-level Symbol-derived
// field is also gated on the allowlist.
//
//------------------------------------------------

/** A `dom::ObjectImpl` view over a described C++ value.

    The proxy holds a non-owning pointer to `obj` and forwards
    DOM accesses to its described fields. Writes go through the
    reflection-driven setter machinery and, for `Symbol`-derived
    targets, are gated on the user-settable field allowlist.
*/
template <class T>
class DescribedObjectProxy final : public dom::ObjectImpl
{
    T* underlying_;

    template <class U, class F>
    static
    bool
    forEachMember(U& obj, F&& fn);

public:
    /** Construct a proxy that aliases `obj`.

        The proxy stores a pointer to `obj`; the caller is
        responsible for ensuring `obj` outlives the proxy.
    */
    explicit
    DescribedObjectProxy(T& obj) noexcept
        : underlying_(&obj)
    {
    }

    /** Return a stable type-identifying string for the proxy.
    */
    char const*
    type_key() const noexcept override
    {
        return "DescribedObjectProxy";
    }

    /** Look up `key` among the described fields of the underlying object.

        @param key The field name to look up.
        @return The field's value wrapped in a `dom::Value`, or an
                empty value when no field matches.
    */
    dom::Value
    get(std::string_view key) const override
    {
        dom::Value result;
        forEachMember(*underlying_,
            [&](std::string_view name, auto& value) -> bool
            {
                if (name == key)
                {
                    result = describedValueToDom(value);
                    return false;
                }
                return true;
            });
        return result;
    }

    /** Write `value` into the described field named `key`.

        Throws `std::runtime_error` when `key` is not a field of
        the underlying type, when a `Symbol`-derived target is
        written through a field outside the user-settable
        allowlist, or when the value cannot be converted to the
        field's declared type.
    */
    void
    set(dom::String key, dom::Value value) override
    {
        std::string_view const k = key.get();
        if constexpr (std::derived_from<T, Symbol>)
        {
            if (!detail::described_setter::isSettableField(k))
            {
                throw std::runtime_error(formatError(
                    "field '{}' is not user-settable from an extension",
                    k).reason());
            }
        }
        std::optional<Expected<void>> outcome =
            detail::described_setter::trySetMember<T>(*underlying_, k, value);
        if (!outcome.has_value())
        {
            throw std::runtime_error(formatError(
                "field '{}' is not on this type", k).reason());
        }
        if (!*outcome)
        {
            throw std::runtime_error(outcome.value().error().reason());
        }
    }

    /** Return whether the underlying object has a described field named `key`.

        @param key The field name to look up.
    */
    bool
    exists(std::string_view key) const override
    {
        bool found = false;
        forEachMember(*underlying_,
            [&](std::string_view name, auto&) -> bool
            {
                if (name == key)
                {
                    found = true;
                    return false;
                }
                return true;
            });
        return found;
    }

    /** Return the number of described fields on the underlying object.
    */
    std::size_t
    size() const override
    {
        std::size_t count = 0;
        forEachMember(*underlying_,
            [&](std::string_view, auto&) -> bool
            {
                ++count;
                return true;
            });
        return count;
    }

    /** Invoke `fn` for each described field of the underlying object.

        Iteration stops when `fn` returns `false`.

        @param fn Callback invoked with each field's name and value.
        @return `true` if all fields were visited, `false` if iteration
                stopped early.
    */
    bool
    visit(std::function<bool(dom::String, dom::Value)> fn) const override
    {
        bool moreLeft = true;
        forEachMember(*underlying_,
            [&](std::string_view name, auto& value) -> bool
            {
                if (!fn(dom::String(name), describedValueToDom(value)))
                {
                    moreLeft = false;
                    return false;
                }
                return true;
            });
        return moreLeft;
    }
};

template <class T>
template <class U, class F>
bool
DescribedObjectProxy<T>::
forEachMember(U& obj, F&& fn)
{
    bool stop = false;
    if constexpr (describe::has_describe_bases<U>::value)
    {
        describe::for_each(
            describe::describe_bases<U>{},
            [&](auto const& desc)
            {
                if (stop) { return; }
                using Base = typename std::decay_t<decltype(desc)>::type;
                if (DescribedObjectProxy<T>::template forEachMember<Base>(
                        static_cast<Base&>(obj), fn))
                {
                    stop = true;
                }
            });
    }
    if (stop) { return true; }
    if constexpr (describe::has_describe_members<U>::value)
    {
        describe::for_each(
            describe::describe_members<U>{},
            [&](auto const& desc)
            {
                if (stop) { return; }
                using Desc = std::decay_t<decltype(desc)>;
                std::string const name =
                    dom::detail::normalizeMemberName(Desc::name);
                if (!fn(name, obj.*Desc::pointer))
                {
                    stop = true;
                }
            });
    }
    return stop;
}

//------------------------------------------------
//
// DescribedArrayProxy<T>
//
// Templated reflection-driven proxy over a `std::vector<T>&`. Reads
// produce per-element proxies via `describedValueToDom`; writes
// (set, emplace_back) push through `assignFromDom` / `buildPolymorphic`
// and mutate the underlying vector. The DOM `ArrayImpl` interface
// covers indexed set and append; length-shrink and arbitrary erase
// would need an extension to that interface.
//
//------------------------------------------------

/** A `dom::ArrayImpl` view over a `std::vector<T>&`.

    Reads produce per-element proxies via @ref describedValueToDom.
    Writes go through the reflection-driven setter machinery and
    mutate the underlying vector in place.
*/
template <class T>
class DescribedArrayProxy final : public dom::ArrayImpl
{
    std::vector<T>* underlying_;

public:
    /** Construct a proxy that aliases `vec`.

        The proxy stores a pointer to `vec`; the caller is
        responsible for ensuring `vec` outlives the proxy.
    */
    explicit
    DescribedArrayProxy(std::vector<T>& vec) noexcept
        : underlying_(&vec)
    {
    }

    /** Return a stable type-identifying string for the proxy.
    */
    char const*
    type_key() const noexcept override
    {
        return "DescribedArrayProxy";
    }

    /** Return the number of elements in the underlying vector.
    */
    size_type
    size() const override
    {
        return underlying_->size();
    }

    /** Return element `i` as a `dom::Value`.

        Returns an empty value when `i` is out of range.
    */
    dom::Value
    get(size_type i) const override
    {
        if (i >= underlying_->size())
        {
            return dom::Value();
        }
        return describedValueToDom((*underlying_)[i]);
    }

    /** Replace element `i` with `value`.

        Throws `std::runtime_error` when `i` is out of range or
        when the value cannot be converted to `T`.
    */
    void
    set(size_type i, dom::Value value) override
    {
        if (i >= underlying_->size())
        {
            throw std::runtime_error("array index out of range");
        }
        Expected<void> r = applyArrayWrite(value, (*underlying_)[i]);
        if (!r)
        {
            throw std::runtime_error(r.error().reason());
        }
    }

    /** Append `value` to the end of the underlying vector.

        Throws `std::runtime_error` when the value cannot be
        converted to `T`.
    */
    void
    emplace_back(dom::Value value) override
    {
        if constexpr (mrdocs::detail::is_polymorphic_v<T>)
        {
            Expected<void> r = detail::described_setter::buildPolymorphic<T>(
                value, "array element",
                [this](T&& v) { underlying_->push_back(std::move(v)); });
            if (!r)
            {
                throw std::runtime_error(r.error().reason());
            }
        }
        else if constexpr (std::is_default_constructible_v<T>)
        {
            T fresh{};
            Expected<void> r = detail::described_setter::assignFromDom(
                fresh, "array element", value);
            if (!r)
            {
                throw std::runtime_error(r.error().reason());
            }
            underlying_->push_back(std::move(fresh));
        }
        else
        {
            throw std::runtime_error(
                "array element type is not default-constructible");
        }
    }

private:
    static
    Expected<void>
    applyArrayWrite(dom::Value const& value, T& dest)
    {
        if constexpr (mrdocs::detail::is_polymorphic_v<T>)
        {
            return detail::described_setter::buildPolymorphic<T>(
                value, "array element",
                [&dest](T&& v) { dest = std::move(v); });
        }
        else
        {
            return detail::described_setter::assignFromDom(
                dest, "array element", value);
        }
    }
};

//------------------------------------------------
//
// describedValueToDom
//
// Convert a described C++ member value to a DOM value:
//
//  - Primitives (string, bool, integers) become matching DOM scalars.
//  - `SymbolID` becomes its base16 string (no recursive-symbol trick).
//  - Described enums become their kebab-case string.
//  - `Optional<T>` collapses to `null` when empty, else recurses.
//  - `vector<T>` becomes a `DescribedArrayProxy` over the live vector.
//  - `Polymorphic<Base>` visits to the concrete derived class and
//    returns a `DescribedObjectProxy` over it.
//  - Any other described type returns a `DescribedObjectProxy<T>`.
//  - Anything not yet recognized returns `undefined`.
//
//------------------------------------------------

template <class T>
dom::Value
describedValueToDom(T& value)
{
    using U = std::remove_cvref_t<T>;

    if constexpr (std::is_same_v<U, std::string>)
    {
        return dom::Value(value);
    }
    else if constexpr (std::is_same_v<U, bool>)
    {
        return dom::Value(value);
    }
    else if constexpr (std::is_same_v<U, SymbolID>)
    {
        return dom::Value(toBase16Str(value));
    }
    else if constexpr (std::is_integral_v<U>)
    {
        return dom::Value(static_cast<std::int64_t>(value));
    }
    else if constexpr (std::is_enum_v<U>
        && describe::has_describe_enumerators<U>::value)
    {
        return dom::Value(toString(value));
    }
    else if constexpr (mrdocs::detail::is_optional_v<U>)
    {
        if (!value.has_value())
        {
            return dom::Value(nullptr);
        }
        return describedValueToDom(*value);
    }
    else if constexpr (mrdocs::detail::is_vector_v<U>)
    {
        using Element = typename U::value_type;
        return dom::Value(dom::newArray<DescribedArrayProxy<Element>>(value));
    }
    else if constexpr (mrdocs::detail::is_polymorphic_v<U>)
    {
        if (value.valueless_after_move())
        {
            return dom::Value(nullptr);
        }
        dom::Value result;
        visit(*value, [&](auto& concrete)
        {
            using Concrete = std::remove_cvref_t<decltype(concrete)>;
            result = dom::Value(dom::newObject<
                DescribedObjectProxy<Concrete>>(concrete));
        });
        return result;
    }
    else if constexpr (describe::has_describe_members<U>::value)
    {
        return dom::Value(dom::newObject<DescribedObjectProxy<U>>(value));
    }
    else
    {
        return dom::Value();
    }
}

} // namespace mrdocs

#endif
