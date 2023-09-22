//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_OBJECT_HPP
#define MRDOX_API_DOM_OBJECT_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>
#include <iterator>
#include <memory>
#include <vector>

namespace clang {
namespace mrdox {
namespace dom {

class ObjectImpl;
class String;
class Value;

/** A container of key and value pairs.
*/
class MRDOX_DECL
    Object final
{
    std::shared_ptr<ObjectImpl> impl_;

public:
    /** The type of an element.

        Elements of this container are key and value
        pairs where the key is a string. This type is
        a copyable, movable value type.
    */
    struct value_type;

    /** A reference to an element.

        This is a read-only reference to an element.
    */
    struct reference;

    /** A reference to an element.

        This is a read-only reference to an element.
    */
    struct const_reference;

    /** A pointer to an element.
    */
    using pointer = reference;

    /** A pointer to an element.
    */
    using const_pointer = reference;

    /** An unsigned integral type used for indexes and sizes.
    */
    using size_type = std::size_t;

    /** A signed integral type.
    */
    using difference_type = std::ptrdiff_t;

    /** A constant iterator referencing an element in an Object.
    */
    class iterator;

    /** A constant iterator referencing an element in an Object.
    */
    using const_iterator = iterator;

    /** The type of storage used by the default implementation.
    */
    using storage_type = std::vector<value_type>;

    /** The implementation type.
    */
    using impl_type = std::shared_ptr<ObjectImpl>;

    //--------------------------------------------

    /** Destructor.
    */
    ~Object() = default;

    /** Constructor.

        Default-constructed objects refer to a new,
        empty container which is distinct from every
        other empty container.
    */
    Object();

    /** Constructor.

        Ownership of the contents is transferred
        to the new object. The moved-from object
        will behave as if default-constructed.
    */
    Object(Object&& other);

    /** Constructor.

        The newly constructed object will contain
        copies of the scalars in other, and
        references to its structured data.
    */
    Object(Object const& other) noexcept;

    /** Assignment.

        Ownership of the object is transferred
        to this, and ownership of the previous
        contents is released. The moved-from
        object behaves as if default constructed.
    */
    Object& operator=(Object&&);

    /** Assignment.

        Shared ownership and copies of elements in
        others are acquired by this. Ownership of
        the previous contents is released.
    */
    Object& operator=(Object const&) noexcept;

    //--------------------------------------------

    /** Constructor.

        This constructs an object from an existing
        implementation, with shared ownership. The
        pointer cannot not be null.
    */
    explicit
    Object(
        impl_type impl) noexcept
        : impl_(std::move(impl))
    {
        MRDOX_ASSERT(impl_);
    }

    /** Constructor.

        Upon construction, the object will retain
        ownership of a shallow copy of the specified
        list. In particular, dynamic objects will
        be acquired with shared ownership.

        @param list The initial list of values.
    */
    explicit Object(storage_type list);

    /** Return the implementation used by this object.
    */
    auto
    impl() const noexcept ->
        impl_type const&
    {
        return impl_;
    }

    /** Return the type key.
    */
    char const* type_key() const noexcept;

    /** Return true if the container is empty.
    */
    bool empty() const;

    /** Return the number of elements.
    */
    std::size_t size() const;

    /** Return the i-th element, without bounds checking.

        @param i The zero-based index of the element.
    */
    reference get(std::size_t i) const;

    /** Return the i-th element, without bounds checking.
    */
    reference operator[](size_type i) const;

    /** Return the element for a given key.
    */
    dom::Value operator[](std::string_view key) const;

    /** Return the i-th element.

        @throw Exception `i >= size()`
    */
    reference at(size_type i) const;

    /** Return true if a key exists.
    */
    bool exists(std::string_view key) const;

    /** Return the value for a given key.

        If the key does not exist, a null value
        is returned.

        @return The value, or null.

        @param key The key.
    */
    Value find(std::string_view key) const;

    /** Set or replace the value for a given key.

        This function inserts a new key or changes
        the value for the existing key if it is
        already present.

        @param key The key.

        @param value The value to set.
    */
    void set(String key, Value value) const;

    /** Return an iterator to the beginning of the range of elements.
    */
    iterator begin() const;

    /** Return an iterator to the end of the range of elements.
    */
    iterator end() const;

    /** Swap two objects.
    */
    void swap(Object& other) noexcept
    {
        std::swap(impl_, other.impl_);
    }

    /** Swap two objects.
    */
    friend void swap(Object& lhs, Object& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    /** Compare two objects for equality.
     */
    friend
    bool
    operator==(Object const& a, Object const& b) noexcept;

    /** Compare two objects for precedence.
     */
    friend
    std::strong_ordering
    operator<=>(Object const& a, Object const& b) noexcept
    {
        if (a.impl_ == b.impl_)
        {
            return std::strong_ordering::equal;
        }
        return std::strong_ordering::equivalent;
    }

    //--------------------------------------------

    /** Return a diagnostic string.
    */
    friend std::string toString(Object const&);
};

//------------------------------------------------
//
// ObjectImpl
//
//------------------------------------------------

/** Abstract object interface.

    This interface is used by Object types.
*/
class MRDOX_DECL
    ObjectImpl
{
public:
    /// @copydoc Object::storage_type
    using storage_type = Object::storage_type;

    /// @copydoc Object::reference
    using reference = Object::reference;

    /// @copydoc Object::iterator
    using iterator = Object::iterator;

    /** Destructor.
    */
    virtual ~ObjectImpl();

    /** Return the type key of the implementation.
    */
    virtual char const* type_key() const noexcept;

    /** Return the number of key/value pairs in the object.
    */
    virtual std::size_t size() const = 0;

    /** Return the i-th key/value pair, without bounds checking.
    */
    virtual reference get(std::size_t i) const = 0;

    /** Return the value for the specified key, or null.
    */
    virtual Value find(std::string_view key) const = 0;

    /** Insert or set the given key/value pair.
    */
    virtual void set(String key, Value value) = 0;
};

/** Return a new object using a custom implementation.
*/
template<class T, class... Args>
requires std::derived_from<T, ObjectImpl>
Object
newObject(Args&&... args)
{
    return Object(std::make_shared<T>(
        std::forward<Args>(args)...));
}

//------------------------------------------------
//
// DefaultObjectImpl
//
//------------------------------------------------

/** The default Object implementation.
*/
class MRDOX_DECL
    DefaultObjectImpl : public ObjectImpl
{
public:
    DefaultObjectImpl() noexcept;

    explicit DefaultObjectImpl(
        storage_type entries) noexcept;
    std::size_t size() const override;
    reference get(std::size_t) const override;
    Value find(std::string_view) const override;
    void set(String, Value) override;

private:
    storage_type entries_;
};

//------------------------------------------------
//
// LazyObjectImpl
//
//------------------------------------------------

/** A lazy Object implementation.
*/
class MRDOX_DECL
    LazyObjectImpl : public ObjectImpl
{
    std::atomic<std::shared_ptr<ObjectImpl>> mutable sp_;

    using impl_type = Object::impl_type;

    ObjectImpl& obj() const;

protected:
    /** Return the constructed object.

        Subclasses override this.
        The function is invoked just in time.
    */
    virtual Object construct() const = 0;

public:
    std::size_t size() const override;
    reference get(std::size_t i) const override;
    Value find(std::string_view key) const override;
    void set(String key, Value value) override;
};

} // dom
} // mrdox
} // clang

// This is here because of circular references
#include <mrdox/Dom/Value.hpp>

#endif
