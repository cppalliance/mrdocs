//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_ARRAY_HPP
#define MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_ARRAY_HPP

#include <mrdocs/Support/DescribedToDom/DescribedToDomForward.hpp>
#include <mrdocs/Support/DescribedToDom/detail/DescribedToDomDetail.hpp>
#include <mrdocs/Support/DescribedToDom/detail/DescribedToDomWrites.hpp>

namespace mrdocs {

/** A `dom::ArrayImpl` view over a `std::vector<E>&`.

    Reads produce per-element proxies via @ref describedToDom; writes
    (`set`, `emplace_back`) push through the reflection-driven write
    machinery and mutate the underlying vector. The interface covers
    indexed set and append; length-shrink and arbitrary erase would need
    an extension to `dom::ArrayImpl`.

    `Vec` is the vector type and may be `const`-qualified: a proxy over a
    `const` vector (the generator render path) is read-only and throws on
    `set`/`emplace_back`; a proxy over a mutable vector (the extension
    path) writes in place. It must be a `std::vector`, so the proxy
    addresses elements by index and appends to the back.
*/
template <class Vec>
requires mrdocs::specialization_of<std::remove_const_t<Vec>, std::vector>
class DescribedArrayProxy final : public dom::ArrayImpl
{
    using Element = typename std::remove_const_t<Vec>::value_type;

    Vec* underlying_;

public:
    /** Construct a proxy that aliases `vec`.

        The proxy stores a pointer to `vec`; the caller is
        responsible for ensuring `vec` outlives the proxy.
    */
    explicit
    DescribedArrayProxy(Vec& vec) noexcept
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
        return describedToDom((*underlying_)[i]);
    }

    /** Replace element `i` with `value`.

        Throws `std::runtime_error` when the underlying vector is
        `const`, when `i` is out of range, or when the value cannot
        be converted to the element type.
    */
    void
    set(size_type i, dom::Value value) override
    {
        if constexpr (std::is_const_v<Vec>)
        {
            throw std::runtime_error(
                "cannot set array element: this is a read-only view");
        }
        else
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
    }

    /** Append `value` to the end of the underlying vector.

        Throws `std::runtime_error` when the underlying vector is
        `const` or when the value cannot be converted to the element
        type.
    */
    void
    emplace_back(dom::Value value) override
    {
        if constexpr (std::is_const_v<Vec>)
        {
            throw std::runtime_error(
                "cannot append to array: this is a read-only view");
        }
        else if constexpr (mrdocs::specialization_of<Element, mrdocs::Polymorphic>)
        {
            Expected<void> r = detail::described_write::buildPolymorphic<Element>(
                value, "array element",
                [this](Element&& v) { underlying_->push_back(std::move(v)); });
            if (!r)
            {
                throw std::runtime_error(r.error().reason());
            }
        }
        else if constexpr (std::is_default_constructible_v<Element>)
        {
            Element fresh{};
            Expected<void> r = detail::described_write::assignFromDom(
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
    applyArrayWrite(dom::Value const& value, Element& dest)
    {
        if constexpr (mrdocs::specialization_of<Element, mrdocs::Polymorphic>)
        {
            return detail::described_write::buildPolymorphic<Element>(
                value, "array element",
                [&dest](Element&& v) { dest = std::move(v); });
        }
        else
        {
            return detail::described_write::assignFromDom(
                dest, "array element", value);
        }
    }
};

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_ARRAY_HPP
