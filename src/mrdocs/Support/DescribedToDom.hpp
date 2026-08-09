//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_HPP
#define MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_HPP

// The reflection-to-DOM bridge: turns any value of a `describe`d C++ type
// into a `dom::Value`, and back (writes). This is the header to include.
// Its parts live under DescribedToDom/: DescribedToDomForward.hpp (the
// shared forward declarations that break the proxy/entry-point cycle),
// DescribedToDomObject.hpp and DescribedToDomArray.hpp (the proxies), and
// detail/ (their private helpers and the write machinery). This file
// provides the entry point, describedToDom, and the dom::ValueFrom
// bridge; it includes the proxies, which supply the rest.

#include <mrdocs/Support/DescribedToDom/DescribedToDomObject.hpp>
#include <mrdocs/Support/DescribedToDom/DescribedToDomArray.hpp>
#include <map>

namespace mrdocs {

/** Project a value of a described C++ type into a `dom::Value`.

    This is the read side of the reflection-to-DOM bridge. The mapping is:

    @li Primitives (string, bool, integers) become matching DOM scalars.
    @li `SymbolID` becomes its base58 string, or `undefined` when invalid.
    @li Described enums become their kebab-case string.
    @li `Optional<T>` becomes `undefined` when empty, else recurses.
    @li `vector<T>` becomes a `DescribedArrayProxy` over the live vector.
    @li `Polymorphic<Base>` visits to the concrete derived class and
        returns a `DescribedObjectProxy` over it.
    @li Any other described type returns a `DescribedObjectProxy<T>`.
    @li Anything not recognized returns `undefined`.

    The array and object results are proxies that read and write `value`
    in place, so `value` must outlive the returned `dom::Value`.

    @param value The described value to project.
    @return The DOM projection of `value`.
*/
template <class T>
dom::Value
describedToDom(T& value)
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
    else if constexpr (std::is_same_v<U, dom::Value>
        || std::is_same_v<U, dom::Object>
        || std::is_same_v<U, dom::Array>)
    {
        // Already a DOM value (e.g. the free-form generator-options /
        // transform-options maps hold dom::Object values), so pass it
        // through unchanged.
        return dom::Value(value);
    }
    else if constexpr (std::is_same_v<U, SymbolID>)
    {
        if (value == SymbolID::invalid)
        {
            return {dom::Kind::Undefined};
        }
        return dom::Value(toBase58Str(value));
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
    else if constexpr (mrdocs::specialization_of<U, mrdocs::Optional>)
    {
        if (!value.has_value())
        {
            // An empty optional is an absent value, which the DOM spells
            // `undefined` (as with an invalid SymbolID above), not a
            // present null.
            return {dom::Kind::Undefined};
        }
        return describedToDom(*value);
    }
    else if constexpr (mrdocs::specialization_of<U, std::vector>)
    {
        // This keeps the vector's constness
        return dom::Value(dom::newArray<
            DescribedArrayProxy<std::remove_reference_t<T>>>(value));
    }
    else if constexpr (mrdocs::specialization_of<U, std::map>)
    {
        // A string-keyed map projects to a DOM object, converting each
        // value in turn (the dynamic generator-options / transform-options
        // settings, whose values are dom::Object).
        dom::Object obj;
        for (auto& [key, mapped] : value)
        {
            obj.set(key, describedToDom(mapped));
        }
        return dom::Value(std::move(obj));
    }
    else if constexpr (mrdocs::specialization_of<U, mrdocs::Polymorphic>)
    {
        if (value.valueless_after_move())
        {
            // A valueless polymorphic holds no object, i.e. an absent
            // value, which the DOM spells `undefined`, not a present null.
            return {dom::Kind::Undefined};
        }
        dom::Value result;
        visit(*value, [&](auto& concrete)
        {
            // Preserve constness of the object.
            using ConcreteQ = std::remove_reference_t<decltype(concrete)>;
            result = dom::Value(dom::newObject<
                DescribedObjectProxy<ConcreteQ>>(concrete));
        });
        return result;
    }
    else if constexpr (describe::isSingleStringObject<U>)
    {
        // A described struct whose only reflected member is a string
        // collapses to that string, exactly as the XML writer emits it
        // (see describe::isSingleStringObject). E.g. ExprInfo reflects as
        // its written expression, so a template writes `{{requires}}`
        // rather than `{{requires.written}}`.
        dom::Value result;
        describe::for_each_member<U>(
            [&](auto d) { result = dom::Value(value.*d.pointer); });
        return result;
    }
    else if constexpr (describe::has_describe_members<U>::value)
    {
        return dom::Value(dom::newObject<
            DescribedObjectProxy<std::remove_reference_t<T>>>(value));
    }
    else if constexpr (dom::HasValueFromWithoutContext<U>)
    {
        // A non-described value type that knows how to convert itself
        // to a dom scalar (e.g. NoexceptInfo -> its "noexcept(...)"
        // string).
        return dom::ValueFrom(value);
    }
    else
    {
        return {dom::Kind::Undefined};
    }
}

/** Convert any described value to a `dom::Value` via the `ValueFrom` idiom.

    Lets `dom::ValueFrom(obj)` produce the reflection view for any
    described type, so callers use the standard dom conversion point
    instead of naming `DescribedObjectProxy`/`describedToDom`
    directly. It simply forwards to @ref describedToDom.

    @param v The output value.
    @param obj The described value to convert (its constness is kept, so
               a `const` object yields a read-only view).
*/
template <class T>
    requires describe::has_describe_members<T>::value
void
tag_invoke(dom::ValueFromTag, dom::Value& v, T const& obj)
{
    v = describedToDom(obj);
}


} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_HPP
