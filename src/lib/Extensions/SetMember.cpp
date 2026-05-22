//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "SetMember.hpp"

#include <mrdocs/Extensions/AllowedFields.gen.hpp>

#include <lib/CorpusImpl.hpp>
#include <lib/Metadata/DocComment/Block/BlockKinds.hpp>
#include <lib/Metadata/DocComment/Inline/InlineKinds.hpp>
#include <lib/Metadata/NameKinds.hpp>
#include <lib/Metadata/Symbol/SymbolKinds.hpp>
#include <lib/Metadata/TArgKinds.hpp>
#include <lib/Metadata/TParamKinds.hpp>
#include <lib/Metadata/TypeKinds.hpp>

#include <mrdocs/Metadata.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>
#include <mrdocs/Support/String.hpp>
#include <mrdocs/Support/TypeTraits.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mrdocs {

dom::Value
buildCorpusDom(
    CorpusImpl& corpus,
    DomCorpus const& domCorpus,
    ExtensionState& state)
{
    dom::Array symbols;
    for (Symbol const& sym : corpus)
    {
        std::string idStr = toBase16Str(sym.id);
        state.byId[idStr] = corpus.find(sym.id);
        dom::Value symValue = domCorpus.get(sym.id);
        // Add a flat base16 id alongside the reflection-driven `id`
        // (which is the recursive Symbol object so templates and
        // scripts agree on its shape and can navigate `symbol.id.name`
        // identically). Scripts pass `_id` back to `mrdocs.set` to
        // identify the symbol to act on; the recursive `id` stays
        // available for navigation.
        symValue.getObject().set("_id", idStr);
        symbols.emplace_back(std::move(symValue));
    }
    dom::Object corpusObj;
    corpusObj.set("symbols", std::move(symbols));
    return dom::Value(std::move(corpusObj));
}

namespace {

// =====================================================================
// Allowlist
// =====================================================================
//
// `mrdocs.set(symbol_id, field, value)` dispatches through reflection,
// but the user-facing surface is a curated allowlist: only the fields
// listed in `kSettableFields` can be written, regardless of what
// described members the underlying symbol type happens to expose. The
// allowlist starts strict and grows as concrete needs surface; this
// keeps scripts from quietly breaking corpus invariants (changing a
// symbol's `kind`, re-parenting it, mutating structural collections,
// ...).
//
// `kSettableFields` is generated from
// `src/lib/Extensions/AllowedFields.json` at build time; the same JSON
// drives the AsciiDoc reference table in `extensions.adoc`, so the
// runtime allowlist and the rendered docs cannot drift. Add a new
// entry by editing the JSON.
//
// `access` is intentionally absent for now: `AccessKind` is not yet
// registered with `MRDOCS_DESCRIBE_ENUM` on this branch, so the
// reflection-driven setter has no way to convert a kebab-case
// enumerator string into the right enum value, and writing to
// `access` would hit the generic "cannot yet write" error path. It
// can be added back to the JSON once `AccessKind` is described (the
// change landing on `feat/schema_generation`).

bool
isSettableField(std::string_view fieldName)
{
    return std::find(
        std::begin(kSettableFields),
        std::end(kSettableFields),
        fieldName) != std::end(kSettableFields);
}

// Compute the script-facing kebab name for `Derived`'s discriminator
// exactly once per process. Used by `isKindKebabName` to avoid
// default-constructing the derived class on every polymorphic write.
//
// The discriminator is read from the inherited `Kind` field of a
// default-constructed instance, the one convention shared by every
// polymorphic base (`Symbol::Kind`, `Inline::Kind`, `Name::Kind`, ...).
// A static `kind_id` wouldn't work for hierarchies such as `Name`,
// where the derived classes set the base's `Kind` from their
// constructor and don't expose a compile-time discriminator.
//
// Two address schemes coexist:
//
// - Most hierarchies expose their discriminator through
//   `MRDOCS_DESCRIBE_ENUM`, and the script-facing name is the
//   kebab-case of the enumerator (e.g., `namespace-alias`). This is
//   also what the DOM exposes, so script names and DOM names agree.
//
// - `TypeKind` is left undescribed (adding the description would
//   force a redundant `<kind>...</kind>` into every type element of
//   every XML golden). Its variants are instead reached via the
//   `toString(TypeKind)` overload that DOM serialization already
//   uses, so script names (`lvalue-reference`, ...) match the DOM
//   and Handlebars side and differ only from the XML writer's tag
//   form (`l-value-reference`, ...).
//
// Non-default-constructible derived classes remain unaddressable;
// the `setMember` allowlist controls which fields scripts can reach
// today, so this has never mattered in practice. The cache returns
// the empty string for those, which compares unequal to every
// non-empty input.
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
            // Fallback for hierarchies (like `TypeKind`) whose
            // discriminator deliberately skips `MRDOCS_DESCRIBE_ENUM`.
            return std::string(toString(instance.Kind));
        }
        else
        {
            return {};
        }
    }
}

// Test whether the script-facing name of `Derived`'s discriminator
// matches `name`. Uses a per-`Derived` cached string so the cost is
// one comparison per polymorphic write per registered kind, not one
// constructor call plus an enumerator scan.
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
//
// `assignFromDom` decodes a DOM value into one allowlisted member. It
// is an overload set: each shape of member (string, enum,
// `Optional<T>`, ...) is a separate overload constrained by a
// `requires` clause, so dispatch happens through overload resolution
// rather than a central `if constexpr` chain. The primary template
// below catches any T not handled by a constrained overload and reports
// a clean error.
//
// All overloads are forward-declared first so the recursive shapes
// (`Optional<T>`, `vector<T>`) find every other overload at template-
// instantiation time, regardless of definition order.

template <typename T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

Expected<void>
assignFromDom(
    std::string& dest, std::string_view fieldName, dom::Value const& src);

Expected<void>
assignFromDom(
    bool& dest, std::string_view fieldName, dom::Value const& src);

template <typename T>
    requires (std::is_enum_v<T> && describe::has_describe_enumerators<T>::value)
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

template <typename T>
    requires detail::is_optional_v<T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

template <typename T>
    requires detail::is_vector_v<T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

template <typename T>
    requires detail::is_polymorphic_v<T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

template <typename T>
    requires describe::has_describe_members<T>::value
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src);

// Forward-declared too: the described-struct overload above calls
// `trySetMember` from inside a lambda, and Clang's two-phase lookup
// (stricter than MSVC) requires the unqualified name to be visible at
// template-definition time, not just at instantiation.
template <typename T>
std::optional<Expected<void>>
trySetMember(T& obj, std::string_view fieldName, dom::Value const& src);

// Build a `Polymorphic<Base>` from a DOM object whose `kind:` field
// names a derived class registered with `MRDOCS_DESCRIBE_KINDS`,
// then hand the constructed value to `place`. The placement callback
// (instead of an `Expected<Poly>` return) avoids instantiating
// `Expected<Polymorphic<X>>`, which triggers a circular concept
// evaluation under MSVC: `Polymorphic`'s converting constructor
// constrains on `copy_constructible<U>` for `U = Expected<Polymorphic<X>>`,
// and `Expected`'s noexcept clause in turn looks at `Polymorphic`'s
// constructors. Routing the value through a callback breaks the cycle
// and serves both the `assignFromDom` overload and the vector branch
// without further plumbing.
//
// No upstream Microsoft Developer Community ticket has been filed for
// this; please link one here if it ever surfaces.
template <typename Poly, typename Place>
Expected<void>
buildPolymorphic(
    dom::Value const& src, std::string_view fieldName, Place&& place);

// Catch-all for any T not matched by a constrained overload above.
template <typename T>
Expected<void>
assignFromDom(T&, std::string_view fieldName, dom::Value const&)
{
    return Unexpected(formatError(
        "mrdocs.set: field '{}' has a type the generic setter cannot yet write",
        fieldName));
}

Expected<void>
assignFromDom(
    std::string& dest, std::string_view fieldName, dom::Value const& src)
{
    if (!src.isString())
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' expects a string", fieldName));
    }
    dest = std::string(src.getString());
    return {};
}

Expected<void>
assignFromDom(
    bool& dest, std::string_view fieldName, dom::Value const& src)
{
    if (!src.isBoolean())
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' expects a boolean", fieldName));
    }
    dest = src.getBool();
    return {};
}

// Described enums round-trip through kebab-case enumerator names,
// matching the read direction.
template <typename T>
    requires (std::is_enum_v<T> && describe::has_describe_enumerators<T>::value)
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src)
{
    if (!src.isString())
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' expects an enumerator name",
            fieldName));
    }
    std::string_view const candidate = src.getString();
    bool found = false;
    describe::for_each(
        describe::describe_enumerators<T>{},
        [&](auto const& D)
        {
            if (found)
            {
                return;
            }
            if (toKebabCase(D.name) == candidate)
            {
                dest = D.value;
                found = true;
            }
        });
    if (!found)
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' has no enumerator named '{}'",
            fieldName, candidate));
    }
    return {};
}

// `null` clears the optional; any other value emplaces the inner T and
// recurses. Inner types that aren't default-constructible from this
// translation unit only accept `null`.
template <typename T>
    requires detail::is_optional_v<T>
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
            "mrdocs.set: field '{}' wraps a type the generic setter cannot construct (only `null` is accepted)",
            fieldName));
    }
}

// Vectors are cleared and rebuilt rather than appended to, so the
// script's array is authoritative. `Polymorphic<U>` elements (which
// aren't default-constructible by design) go through `buildPolymorphic`
// with `push_back` as the placement callback; everything else gets a
// default-constructed element mutated in place by `assignFromDom`.
template <typename T>
    requires detail::is_vector_v<T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src)
{
    using Element = typename T::value_type;
    if (!src.isArray())
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' expects an array", fieldName));
    }
    dom::Array const arr = src.getArray();
    std::size_t const n = arr.size();
    T fresh;
    fresh.reserve(n);
    if constexpr (detail::is_polymorphic_v<Element>)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            Expected<void> r = buildPolymorphic<Element>(
                arr.get(i), fieldName,
                [&](Element&& v) { fresh.push_back(std::move(v)); });
            if (!r)
            {
                return Unexpected(r.error());
            }
        }
    }
    else if constexpr (std::is_default_constructible_v<Element>)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            Element elem{};
            Expected<void> r = assignFromDom(elem, fieldName, arr.get(i));
            if (!r)
            {
                return r;
            }
            fresh.push_back(std::move(elem));
        }
    }
    else
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' contains a type the generic setter cannot construct",
            fieldName));
    }
    dest = std::move(fresh);
    return {};
}

// Polymorphic values are written as a DOM object whose `kind:` field
// picks the concrete derived class; the build logic lives in
// `buildPolymorphic` so the vector overload can reuse it.
template <typename T>
    requires detail::is_polymorphic_v<T>
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src)
{
    return buildPolymorphic<T>(
        src, fieldName,
        [&](T&& v) { dest = std::move(v); });
}

// Described structs accept a partial DOM object: each key is looked up
// against the struct's described members (and bases) and assigned
// through `trySetMember`. Unknown keys are an error so typos don't
// silently no-op.
template <typename T>
    requires describe::has_describe_members<T>::value
Expected<void>
assignFromDom(T& dest, std::string_view fieldName, dom::Value const& src)
{
    if (!src.isObject())
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' expects an object", fieldName));
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
                    "mrdocs.set: field '{}': unknown sub-field '{}'",
                    fieldName, key));
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

// Walk one type's described members; if `fieldName` matches one, try
// to assign and return the result. Bases are walked recursively so a
// derived symbol can set a base-class field by name. `std::nullopt`
// means "no member of that name was found at this level."
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
                if (outcome.has_value())
                {
                    return;
                }
                using Descriptor = std::decay_t<decltype(descriptor)>;
                std::string const normalized =
                    detail::normalizeMemberName(Descriptor::name);
                if (normalized != fieldName)
                {
                    return;
                }
                outcome = assignFromDom(
                    obj.*Descriptor::pointer, fieldName, src);
            });
    }
    if (outcome.has_value())
    {
        return outcome;
    }
    if constexpr (describe::has_describe_bases<T>::value)
    {
        describe::for_each(
            describe::describe_bases<T>{},
            [&](auto const& descriptor)
            {
                if (outcome.has_value())
                {
                    return;
                }
                using BaseType =
                    typename std::decay_t<decltype(descriptor)>::type;
                outcome = trySetMember<BaseType>(
                    static_cast<BaseType&>(obj), fieldName, src);
            });
    }
    return outcome;
}

// Apply each key in `obj` other than `kind` to a default-constructed
// derived instance, dispatching via `trySetMember`. Used by
// `buildPolymorphic` once the right derived class has been picked.
// The traversal mirrors the described-struct overload of
// `assignFromDom`, but skips the `kind` discriminator and annotates
// the error message with the kind tag.
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
            if (key == "kind")
            {
                return true;
            }
            std::optional<Expected<void>> outcome =
                trySetMember<Derived>(instance, key, value);
            if (!outcome.has_value())
            {
                result = Unexpected(formatError(
                    "mrdocs.set: field '{}': unknown sub-field '{}' for kind '{}'",
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

// Build a `Polymorphic<Base>` once the matching derived class has been
// picked, then hand it to `place`. Default-constructible derivatives
// go through `applyDerivedFields`; the rest are unreachable from
// scripts and produce a clean error.
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
        if (!r)
        {
            return Unexpected(r.error());
        }
        place(Poly(std::in_place_type<Derived>, std::move(instance)));
        return {};
    }
    else
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}': derived kind '{}' is not default-constructible",
            fieldName, kindStr));
    }
}

// Build a `Polymorphic<Base>` from a DOM object. See the forward
// declaration above for the rationale (callback instead of
// `Expected<Poly>` return).
template <typename Poly, typename Place>
Expected<void>
buildPolymorphic(
    dom::Value const& src, std::string_view fieldName, Place&& place)
{
    using Base = typename Poly::value_type;
    if (!src.isObject())
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' expects an object describing a polymorphic value",
            fieldName));
    }
    dom::Object const obj = src.getObject();
    dom::Value kindV = obj.get("kind");
    if (!kindV.isString())
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' expects an object with a string `kind` field",
            fieldName));
    }
    std::string_view const kindStr = kindV.getString();

    if constexpr (!describe::has_describe_kinds<Base>::value)
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' is a polymorphic base with no described kinds",
            fieldName));
    }
    else
    {
        Expected<void> result = Unexpected(formatError(
            "mrdocs.set: field '{}' has no derived class with kind '{}'",
            fieldName, kindStr));
        bool matched = false;
        describe::for_each(
            describe::describe_kinds<Base>{},
            [&](auto descriptor)
            {
                if (matched)
                {
                    return;
                }
                using Descriptor = std::decay_t<decltype(descriptor)>;
                using Derived = typename Descriptor::type;
                if (!isKindKebabName<Derived>(kindStr))
                {
                    return;
                }
                matched = true;
                result = tryBuildDerived<Poly, Derived>(
                    obj, fieldName, kindStr, place);
            });
        return result;
    }
}

} // (anon)

// Language-agnostic generic setter. Refuses anything outside the
// allowlist up front, then dispatches reflection through `trySetMember`
// on the dynamic symbol type.
Expected<dom::Value, Error>
setMemberImpl(
    ExtensionState& state,
    dom::Value const& idArg,
    dom::Value const& fieldArg,
    dom::Value const& valueArg)
{
    if (!idArg.isString())
    {
        return Unexpected(Error(
            "mrdocs.set: argument 1 (symbol id) must be a string"));
    }
    if (!fieldArg.isString())
    {
        return Unexpected(Error(
            "mrdocs.set: argument 2 (field name) must be a string"));
    }

    std::string_view const idView = idArg.getString();
    std::string_view const fieldView = fieldArg.getString();

    if (!isSettableField(fieldView))
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' is not user-settable from an extension",
            fieldView));
    }

    auto it = state.byId.find(std::string(idView));
    if (it == state.byId.end())
    {
        return Unexpected(formatError(
            "mrdocs.set: unknown symbol id '{}'", idView));
    }
    Symbol* sym = it->second;
    MRDOCS_ASSERT(sym != nullptr);

    std::optional<Expected<void>> outcome;
    visit(*sym, [&]<typename DerivedSymbolTy>(DerivedSymbolTy& derived)
    {
        outcome = trySetMember<DerivedSymbolTy>(derived, fieldView, valueArg);
    });
    if (!outcome.has_value())
    {
        return Unexpected(formatError(
            "mrdocs.set: field '{}' is allowlisted but missing on this symbol",
            fieldView));
    }
    if (!outcome.value())
    {
        return Unexpected(outcome.value().error());
    }
    return dom::Value();
}

} // mrdocs
