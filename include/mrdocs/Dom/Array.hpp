//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_DOM_ARRAY_HPP
#define MRDOCS_API_DOM_ARRAY_HPP

#include <mrdocs/Platform.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace clang {
namespace mrdocs {
namespace dom {

class ArrayImpl;
class Value;

/** An array of values

    Arrays are a collection of indexed values. They
    are an extension of objects with a particular
    relationship between integer-keyed properties
    and some abstract length-property. Besides,
    they include convenient methods to manipulate
    these ordered sequences of values.

*/
class MRDOCS_DECL
    Array final
{
    std::shared_ptr<ArrayImpl> impl_;

public:
    /** The type of an element.
    */
    using value_type = Value;

    /** A reference to an element.

        This is a read-only reference to an element.
    */
    using reference = value_type;

    /** A reference to an element.

        This is a read-only reference to an element.
    */
    using const_reference = value_type;

    /** A pointer to an element.
    */
    using pointer = value_type const*;

    /** A pointer to an element.
    */
    using const_pointer = value_type const*;

    /** An unsigned integral type used for indexes and sizes.
    */
    using size_type = std::size_t;

    /** A signed integral type.
    */
    using difference_type = std::ptrdiff_t;

    /** A constant iterator referencing an element in an Array.
    */
    class iterator;

    /** A constant iterator referencing an element in an Array.
    */
    using const_iterator = iterator;

    /** The type of storage used by the default implementation.
    */
    using storage_type = std::vector<value_type>;

    /** The implementation type.
    */
    using impl_type = std::shared_ptr<ArrayImpl>;

    /** Destructor.
    */
    ~Array();

    /** Constructor.

        Default-constructed arrays refer to a new,
        empty array which is distinct from every
        other empty array.
    */
    Array();

    /** Constructor.

        Ownership of the contents is transferred
        to the new object. The moved-from array
        will behave as if default-constructed.
    */
    Array(Array&& other);

    /** Constructor.

        The newly constructed array will contain
        copies of the scalars in other, and
        references to its structured data.
    */
    Array(Array const& other);

    /** Constructor.

        This constructs an array from an existing
        implementation, with shared ownership. The
        pointer cannot not be null.
    */
    Array(impl_type impl) noexcept
        : impl_(std::move(impl))
    {
        MRDOCS_ASSERT(impl_);
    }

    /** Constructor.

        Upon construction, the array will retain
        ownership of a shallow copy of the specified
        elements. In particular, dynamic objects will
        be acquired with shared ownership.

        @param elements The elements to acquire.
    */
    Array(storage_type elements);

    /** Assignment.

        Ownership of the array is transferred
        to this, and ownership of the previous
        contents is released. The moved-from
        array behaves as if default constructed.
    */
    Array& operator=(Array&&);

    /** Assignment.

        This acquires shared ownership of the copied
        array, and ownership of the previous contents
        is released.
    */
    Array& operator=(Array const&) = default;

    //--------------------------------------------

    /** Return the implementation used by this object.
    */
    auto impl() const noexcept -> impl_type const&
    {
        return impl_;
    }

    /** Return the type key of the implementation.
    */
    char const* type_key() const noexcept;

    /** Return true if the array is empty.
    */
    bool empty() const noexcept;

    /** Return the number of elements in the array.
    */
    size_type size() const noexcept;

    /** Return the i-th element, without bounds checking.

        @param i The zero-based index of the element.
    */
    value_type get(size_type i) const;

    /** Set the i-th element, without bounds checking.

        @param i The zero-based index of the element.
        @param v The value to set.
    */
    void set(size_type i, Value v);

    /** Return the i-th element.

        @throw Exception `i >= size()`
    */
    value_type at(size_type i) const;

    /** Return the first element.

        @throw Exception `empty()`
    */
    value_type front() const;

    /** Return the last element.

        @throw Exception `empty()`
    */
    value_type back() const;

    /** Return an iterator to the beginning of the range of elements.
     */
    iterator begin() const;

    /** Return an iterator to the end of the range of elements.
     */
    iterator end() const;

    /** Append an element to the end of the array.

        If the array is read-only, an exception
        is thrown.
    */
    void push_back(value_type value);

    /** Append an element to the end of the array.

        If the array is read-only, an exception
        is thrown.
    */
    template< class... Args >
    void emplace_back(Args&&... args);

    /** Concatenate two arrays.
    */
    friend Array operator+(Array const& lhs, Array const& rhs);

    /// @overload
    template <std::convertible_to<Array> S>
    friend auto operator+(
        S const& lhs, Array const& rhs) noexcept
    {
        return Array(lhs) + rhs;
    }

    /// @overload
    template <std::convertible_to<Array> S>
    friend auto operator+(
        Array const& lhs, S const& rhs) noexcept
    {
        return lhs + Array(rhs);
    }


    /** Swap two arrays.
    */
    void swap(Array& other) noexcept
    {
        std::swap(impl_, other.impl_);
    }

    /** Swap two arrays.
    */
    friend void swap(Array& lhs, Array& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    /** Compare two arrays for equality.
     */
    friend
    bool
    operator==(Array const&, Array const&) noexcept;

    /** Compare two arrays for precedence.
     */
    friend
    std::strong_ordering
    operator<=>(Array const&, Array const&) noexcept;

    /** Return a diagnostic string.
    */
    friend
    std::string
    toString(Array const&);

    template<class T, class... Args>
    requires std::derived_from<T, ArrayImpl>
    friend Array newArray(Args&&... args);
};

//------------------------------------------------
//
// ArrayImpl
//
//------------------------------------------------

/** Abstract array interface.

    This interface is used by Array types.
*/
class MRDOCS_DECL
    ArrayImpl
{
public:
    /// @copydoc Array::value_type
    using value_type = Array::value_type;

    /// @copydoc Array::size_type
    using size_type = Array::size_type;

    /** Destructor.
    */
    virtual ~ArrayImpl();

    /** Return the type key of the implementation.
    */
    virtual char const* type_key() const noexcept;

    /** Return the number of elements in the array.
    */
    virtual size_type size() const = 0;

    /** Return the i-th element, without bounds checking.
    */
    virtual value_type get(size_type i) const = 0;

    /** Set the i-th element, without bounds checking.
    */
    virtual void set(size_type, Value);

    /** Append an element to the end of the array.

        The default implementation throws an exception,
        making the array effectively read-only.
    */
    virtual void emplace_back(value_type value);
};

//------------------------------------------------
//
// DefaultArrayImpl
//
//------------------------------------------------

/** The default array implementation.

    This implementation is backed by a simple
    vector and allows appending.
*/
class MRDOCS_DECL
    DefaultArrayImpl : public ArrayImpl
{
public:
    /// @copydoc Array::value_type
    using value_type = Array::value_type;

    /// @copydoc Array::size_type
    using size_type = Array::size_type;

    /// @copydoc Array::storage_type
    using storage_type = Array::storage_type;

    DefaultArrayImpl();
    explicit DefaultArrayImpl(
        storage_type elements) noexcept;
    size_type size() const override;
    value_type get(size_type i) const override;
    void set(size_type i, Value v) override;
    void emplace_back(value_type value) override;
    char const* type_key() const noexcept override;

private:
    std::vector<value_type> elements_;
};

/** Return a new array using a custom implementation.
*/
template<class T, class... Args>
requires std::derived_from<T, ArrayImpl>
Array
newArray(Args&&... args)
{
    return Array(std::make_shared<T>(
        std::forward<Args>(args)...));
}

} // dom
} // mrdocs
} // clang

// This is here because of circular references
#include <mrdocs/Dom/Value.hpp>

#endif
