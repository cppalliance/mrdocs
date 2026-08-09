//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_OBJECT_HPP
#define MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_OBJECT_HPP

#include <mrdocs/Support/DescribedToDom/DescribedToDomForward.hpp>
#include <mrdocs/Support/DescribedToDom/detail/DescribedToDomDetail.hpp>
#include <mrdocs/Support/DescribedToDom/detail/DescribedToDomWrites.hpp>

namespace mrdocs {

//------------------------------------------------
//
// DescribedObjectProxy<T>
//
// Templated reflection-driven proxy over a described C++ object.
// Reads walk the described members via `describe::*` and convert
// each field through `describedToDom`. Writes go through the
// reflective setter and can target any described field of the
// underlying type.
//
//------------------------------------------------

/** A `dom::ObjectImpl` view over a described C++ value.

    The proxy holds a non-owning pointer to `obj` and forwards
    DOM accesses to its described fields. Writes go through the
    reflection-driven setter machinery and can target any described
    field of the underlying type.
*/
template <class T>
class DescribedObjectProxy final : public dom::ObjectImpl
{
    T* underlying_;

    /*  Invoke `fn(name, value)` for each described member of `obj`.

        Walks `obj`'s reflected base classes (depth-first) and then its
        own members, calling `fn` with the DOM-normalized member name
        (see `dom::detail::normalizeMemberName`) and a reference to the
        member value. `fn` returns `bool`: `false` stops the walk early
        (used by `get`/`exists`). The member reference keeps `obj`'s
        cv-qualification, so a `const` object yields `const` members.

        A thin wrapper over the value-yielding `describe::for_each_member`
        overload (which yields each member's value, preserves constness,
        and supports early-exit via a bool return); this wrapper adds only
        the DOM policy layer -- skipping empty members and normalizing
        member names.

        @param obj The (possibly `const`) described object to walk.
        @param fn A callable `bool(std::string_view name, auto& value)`.
        @return `true` if the walk completed, `false` if `fn` stopped it.
    */
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
        if (key == "$meta")
        {
            return dom::Value(
                dom::detail::proxyMetaObject<std::remove_const_t<T>>());
        }
        dom::Value result;
        forEachMember(*underlying_,
            [&](std::string_view name, auto& value) -> bool
            {
                if (name == key)
                {
                    result = describedToDom(value);
                    return false;
                }
                return true;
            });
        return result;
    }

    /** Write `value` into the described field named `key`.

        Throws `std::runtime_error` when the underlying object is
        `const` (a read-only proxy, as used by the generator render
        path), when `key` is not a field of the underlying type, or
        when the value cannot be converted to the field's declared
        type.
    */
    void
    set(dom::String key, dom::Value value) override
    {
        std::string_view const k = key.get();
        if constexpr (std::is_const_v<T>)
        {
            throw std::runtime_error(formatError(
                "cannot set field '{}': this is a read-only view",
                k).reason());
        }
        else
        {
        std::optional<Expected<void>> outcome =
            detail::described_write::trySetMember<T>(*underlying_, k, value);
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
    }

    /** Return whether the underlying object has a described field named `key`.

        @param key The field name to look up.
    */
    bool
    exists(std::string_view key) const override
    {
        if (key == "$meta")
        {
            return true;
        }
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
        // A compile-time count (describe::describedMemberCount) would be
        // cheaper but wrong: `size` must match what `get`/`visit`
        // expose, and those omit empty members (see proxyShouldEmit).
        // Omission depends on the runtime value, so the count is a
        // runtime walk that applies the same rule.
        std::size_t count = 1; // the synthesized `$meta`
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
        if (!fn(dom::String("$meta"),
                dom::Value(dom::detail::proxyMetaObject<
                    std::remove_const_t<T>>())))
        {
            return false;
        }
        bool moreLeft = true;
        forEachMember(*underlying_,
            [&](std::string_view name, auto& value) -> bool
            {
                if (!fn(dom::String(name), describedToDom(value)))
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
    // Delegate the actual traversal (bases included, member constness
    // preserved, early-exit on a false return) to the value-yielding
    // describe::for_each_member overload. This proxy only layers on the
    // DOM-facing policy: skip empty members (proxyShouldEmit, matching
    // the XML writer and the previous DOM) and hand the callback the
    // DOM-normalized field name. describe::for_each_member returns true
    // when it ran to completion; this function's historical contract is
    // the opposite (true means the walk stopped early), so it negates.
    bool const completed = describe::for_each_member(
        obj,
        [&](std::string_view rawName, auto& value) -> bool
        {
            if (!dom::detail::proxyShouldEmit(value))
            {
                return true; // skip this member, keep walking
            }
            return fn(dom::detail::normalizeMemberName(rawName), value);
        });
    return !completed;
}

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_OBJECT_HPP
