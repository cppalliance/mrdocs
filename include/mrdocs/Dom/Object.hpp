//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_DOM_OBJECT_HPP
#define MRDOCS_API_DOM_OBJECT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Error.hpp>
#include <iterator>
#include <memory>
#include <vector>

namespace clang {
namespace mrdocs {
namespace dom {

class ObjectImpl;
class String;
class Value;

/** A container of key and value pairs.

    Objects are a collection of properties, which are
    equivalent to key-value pairs. Property values
    can be any type, including other Objects, allowing
    for the creation of arbitrarily complex data
    structures.

    An Object is a non-primitive (or reference) type,
    meaning that they are not copied when assigned or
    passed as a parameter. Instead, the reference is
    copied, and the original value is shared.

    These reference types are modeled after JavaScript
    "Objects". All non-primitive types (Object types)
    are derived from Object in JavaScript. This means
    types such as Array and Function represent a
    relevant selection of built-in types that would
    derive from Object in JavaScript.

    @par Properties
    @parblock
    Objects are a collection of properties, which are
    equivalent to key-value pairs. There are two
    kinds of properties:

    @li Data properties: Associates a key with a value.
    @li Accessor properties: Associates a key with
        one of two accessor functions (`get` and `set`),
        which are used to retrieve or set the value.

    The internal representation of objects can determine
    how properties are stored and the type of properties
    being represented.

    Properties can also be enumerable or non-enumerable.
    An enumerable property is one that is iterated by
    the `visit` function. Non-enumerable properties can
    only be accessed by name with the `get` and `set`
    functions.
    @endparblock
*/
class MRDOCS_DECL
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
    using reference = value_type;

    /** A reference to an element.

        This is a read-only reference to an element.
    */
    using const_reference = reference;

    /** A pointer to an element.
    */
    using pointer = value_type const*;

    /** A pointer to an element.
    */
    using const_pointer = pointer;

    /** An unsigned integral type used for indexes and sizes.
    */
    using size_type = std::size_t;

    /** A signed integral type.
    */
    using difference_type = std::ptrdiff_t;

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
        MRDOCS_ASSERT(impl_);
    }

    /** Constructor.

        Upon construction, the object will retain
        ownership of a shallow copy of the specified
        list. In particular, dynamic objects will
        be acquired with shared ownership.

        @param list The initial list of values.
    */
    explicit Object(storage_type list);

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

    /** Return the element with the specified key
    */
    Value get(std::string_view key) const;

    /// @copydoc get
    Value at(std::string_view i) const;

    /** Return true if a key exists.
    */
    bool exists(std::string_view key) const;

    /** Set or replace the value for a given key.

        This function inserts a new key or changes
        the value for the existing key if it is
        already present.

        @param key The key.

        @param value The value to set.
    */
    void set(String key, Value value) const;

    /** Invoke the visitor for each key/value pair

        The visitor function must return `true` to
        continue iteration, or `false` to stop
        iteration early.

        @return `true` if the visitor returned `true`
        for all elements, otherwise `false`.

     */
    template <class F>
    requires
        std::invocable<F, String, Value> &&
        std::same_as<std::invoke_result_t<F, String, Value>, bool>
    bool visit(F&& fn) const;

    /** Invoke the visitor for each key/value pair

        The visitor function must return `void` to
        continue iteration, or an `Unexpected<E>` to
        stop iteration early.

        If an error is returned, the iteration stops
        and the error is returned from this function.

        @return `void` if the visitor returned did not
        return an error for any element, otherwise
        `E`.

     */
    template <class F>
    requires
        std::invocable<F, String, Value> &&
        mrdocs::detail::isExpected<std::invoke_result_t<F, String, Value>>
    Expected<void, typename std::invoke_result_t<F, String, Value>::error_type>
    visit(F&& fn) const;

    /** Invoke the visitor for each key/value pair
     */
    template <class F>
    requires
        std::invocable<F, String, Value> &&
        std::same_as<std::invoke_result_t<F, String, Value>, void>
    void visit(F&& fn) const;

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
class MRDOCS_DECL
    ObjectImpl
{
public:
    /// @copydoc Object::storage_type
    using storage_type = Object::storage_type;

    /// @copydoc Object::reference
    using reference = Object::reference;

    /** Destructor.
    */
    virtual ~ObjectImpl();

    /** Return the type key of the implementation.
    */
    virtual char const* type_key() const noexcept;

    /** Return the value for the specified key, or null.
    */
    virtual Value get(std::string_view key) const = 0;

    /** Insert or set the given key/value pair.
    */
    virtual void set(String key, Value value) = 0;

    /** Invoke the visitor for each key/value pair.

        The visitor function must return `true` to
        continue iteration, or `false` to stop.

        The visit function returns `true` if the
        visitor returned `true` for all elements,
        otherwise `false`.

    */
    virtual bool visit(std::function<bool(String, Value)>) const = 0;

    /** Return the number of properties in the object.
     */
    virtual std::size_t size() const = 0;

    /** Determine if a key exists.
     */
    virtual bool exists(std::string_view key) const;
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
class MRDOCS_DECL
    DefaultObjectImpl : public ObjectImpl
{
public:
    DefaultObjectImpl() noexcept;

    explicit DefaultObjectImpl(
        storage_type entries) noexcept;
    std::size_t size() const override;
    Value get(std::string_view) const override;
    void set(String, Value) override;
    bool visit(std::function<bool(String, Value)>) const override;
    bool exists(std::string_view key) const override;

private:
    storage_type entries_;
};

} // dom
} // mrdocs
} // clang

// This is here because of circular references
#include <mrdocs/Dom/Value.hpp>

#endif
