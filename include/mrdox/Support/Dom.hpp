//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_DOM_HPP
#define MRDOX_API_SUPPORT_DOM_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/ADT/Optional.hpp>
#include <mrdox/Support/SharedPtr.hpp>
#include <fmt/format.h>
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {
namespace dom {

class Array;
class Object;
class Value;

/** The type of data in a Value.
*/
enum class Kind
{
    Null,
    Boolean,
    Integer,
    String,
    Array,
    Object
};

//------------------------------------------------
//
// Array
//
//------------------------------------------------

/** Abstract array interface.

    This interface is used by Array types.
*/
class MRDOX_DECL
    ArrayImpl
{
public:
    virtual ~ArrayImpl() = 0;
    virtual std::size_t size() const noexcept = 0;
    virtual Value at(std::size_t) const = 0;
};

/** An array of values.
*/
class MRDOX_DECL
    Array final
{
    std::shared_ptr<ArrayImpl> impl_;

public:
    using impl_type = std::shared_ptr<ArrayImpl>;

    /** Destructor.
    */
    ~Array();

    /** Constructor.

        Default-constructed arrays refer to a new,
        empty array which is distinct from every
        other empty array.
    */
private:
    Array();
public:

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
    Array(std::shared_ptr<ArrayImpl> impl) noexcept
        : impl_(std::move(impl))
    {
        MRDOX_ASSERT(impl_);
    }

    /** Return the implementation used by this object.
    */
    auto
    impl() const noexcept ->
        impl_type const&
    {
        return impl_;
    }

    /** Return true if the array is empty.
    */
    bool empty() const noexcept
    {
        return impl_->size() == 0;
    }

    /** Return the number of elements in the array.
    */
    std::size_t size() const noexcept
    {
        return impl_->size();
    }

    /** Return the zero-based element from the array.

        @throw std::out_of_range `index >= this->size()`
    */
    Value at(std::size_t index) const;

    /** Return a diagnostic string.
    */
    friend
    std::string
    toString(Array const&);
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

//------------------------------------------------
//
// Object
//
//------------------------------------------------

/** Abstract object interface.

    This interface is used by Object types.
*/
class MRDOX_DECL
    ObjectImpl
{
public:
    using value_type = std::pair<std::string_view, Value>;
    using entries_type = std::vector<value_type>;

    virtual ~ObjectImpl() = 0;
    virtual std::size_t size() const noexcept = 0;
    virtual bool exists(std::string_view key) const = 0;
    virtual Value get(std::string_view key) const = 0;
    virtual void set(std::string_view key, Value value) = 0;
    virtual entries_type entries() const = 0;
};

/** A container of key and value pairs.
*/
class MRDOX_DECL
    Object final
{
    std::shared_ptr<ObjectImpl> impl_;

public:
    /** The type of an element in this container.
    */
    using value_type = ObjectImpl::value_type;

    /** The underlying, iterable range used by this container.
    */
    using entries_type = ObjectImpl::entries_type;

    /** The type of implementation.
    */
    using impl_type = std::shared_ptr<ObjectImpl>;

    /** Destructor.
    */
    ~Object();

    /** Constructor.

        Default-constructed objects refer to a new,
        empty array which is distinct from every
        other empty object.
    */
    Object();

    /** Constructor.

        The newly constructed array will contain
        copies of the scalars in other, and
        references to its structured data.
    */
    Object(Object const& other) noexcept;

    /** Constructor.

        Upon construction, the object will retain
        ownership of a shallow copy of the specified
        list. In particular, dynamic objects will
        be acquired with shared ownership.

        @param list The initial list of values.
    */
    explicit Object(entries_type list);

    /** Constructor.

        This constructs an object from an existing
        implementation, with shared ownership. The
        pointer cannot not be null.
    */
    explicit
    Object(impl_type impl) noexcept
        : impl_(std::move(impl))
    {
        MRDOX_ASSERT(impl_);
    }

    /** Return the implementation used by this object.
    */
    auto
    impl() const noexcept ->
        impl_type const&
    {
        return impl_;
    }

    /** Return true if the container is empty.
    */
    bool empty() const noexcept;

    /** Return the number of elements.
    */
    std::size_t size() const noexcept;

    /** Return true if a key exists.
    */
    bool exists(std::string_view key) const;

    /** Return the value for a given key.

        If the key does not exist, a null value
        is returned.

        @return The value, or null.

        @param key The key.
    */
    Value get(std::string_view key) const;

    /** Set or replace the value for a given key.

        This function inserts a new key or changes
        the value for the existing key if it is
        already present.

        @param key The key.

        @param value The value to set.
    */
    void set(std::string_view key, Value value) const;

    /** Return the range of contained values.
    */
    entries_type entries() const noexcept;

    //--------------------------------------------

    /** Return a diagnostic string.
    */
    friend
    std::string
    toString(Object const&);
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
    entries_type entries_;

public:
    DefaultObjectImpl() noexcept;
    explicit DefaultObjectImpl(entries_type entries) noexcept;
    std::size_t size() const noexcept override;
    bool exists(std::string_view key) const override;
    Value get(std::string_view key) const override;
    void set(std::string_view key, Value value) override;
    entries_type entries() const override;
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
    virtual Object construct() const = 0;

public:
    std::size_t size() const noexcept override;
    bool exists(std::string_view key) const override;
    Value get(std::string_view key) const override;
    void set(std::string_view key, Value value) override;
    entries_type entries() const override;
};

//------------------------------------------------

struct literal
{
    std::string_view s;

    literal(std::string_view s_)
        : s(s_)
    {
    }
};

/** A variant container for any kind of Dom value.
*/
class MRDOX_DECL
    Value
{
    enum class Kind
    {
        Null,
        Boolean,
        Integer,
        String,
        Array,
        Object
    };

    Kind kind_;

    union
    {
        bool                b_;
        std::int64_t        i_;
        std::string         str_;
        Array               arr_;
        Object              obj_;
    };

    friend class Array;
    friend class Object;

public:
    ~Value();
    Value() noexcept;
    Value(Value const&);
    Value(Value&&) noexcept;
    Value(std::nullptr_t) noexcept;
    Value(std::int64_t) noexcept;
    Value(std::string s) noexcept;
    Value(literal const& lit) noexcept;
    Value(Array arr) noexcept;
    Value(Object obj) noexcept;
    Value& operator=(Value&&) noexcept;
    Value& operator=(Value const&);

    template<class Boolean>
    requires std::is_same_v<Boolean, bool>
    Value(Boolean const& b) noexcept
        : kind_(Kind::Boolean)
        , b_(b)
    {
    }

    Value(char const* s)
        : Value(std::string(s))
    {
    }

    template<class String>
    requires std::is_convertible_v<
        String, std::string_view>
    Value(String const& s)
        : Value(std::string(s))
    {
    }

    template<class Enum>
    requires std::is_enum_v<Enum>
    Value(Enum v) noexcept
        : Value(static_cast<
            std::underlying_type_t<Enum>>(v))
    {
    }

    template<class T>
    requires std::constructible_from<Value, T>
    Value(std::optional<T> const& opt)
        : Value(opt ? Value(*opt) : Value())
    {
    }

    template<class T>
    requires std::constructible_from<Value, T>
    Value(Optional<T> const& opt)
        : Value(opt ? Value(*opt) : Value())
    {
    }

    /** Return the type of value contained.
    */
    dom::Kind kind() const noexcept;

    /** Return true if this is null.
    */
    bool isNull() const noexcept
    {
        return kind_ == Kind::Null;
    }

    /** Return true if this is a boolean.
    */
    bool isBoolean() const noexcept
    {
        return kind_ == Kind::Boolean;
    }

    /** Return true if this is an integer.
    */
    bool isInteger() const noexcept
    {
        return kind_ == Kind::Integer;
    }

    /** Return true if this is a string.
    */
    bool isString() const noexcept
    {
        return kind_ == Kind::String;
    }

    /** Return true if this is an array.
    */
    bool isArray() const noexcept
    {
        return kind_ == Kind::Array;
    }

    /** Return true if this is an object.
    */
    bool isObject() const noexcept
    {
        return kind_ == Kind::Object;
    }

    bool isTruthy() const noexcept;

    bool getBool() const noexcept
    {
        MRDOX_ASSERT(isBoolean());
        return b_ != 0;
    }

    std::int64_t
    getInteger() const noexcept
    {
        MRDOX_ASSERT(isInteger());
        return i_;
    }

    std::string_view
    getString() const noexcept
    {
        MRDOX_ASSERT(isString());
        return str_;
    }

    /** Return the array.

        @throw Error `! isArray()`
    */
    Array const&
    getArray() const;

    /** Return the object.

        @throw Error `! isObject()`
    */
    Object const&
    getObject() const;

    /** Swap two values.
    */
    void
    swap(Value& other) noexcept;

    /** Swap two values.
    */
    friend
    void
    swap(Value& v0, Value& v1) noexcept
    {
        v0.swap(v1);
    }

    /** Return a diagnostic string.
    */
    friend
    std::string
    toString(Value const&);

    /** Return a diagnostic string.

        This function will not traverse children.
    */
    friend
    std::string
    toStringChild(Value const&);
};

//------------------------------------------------

inline Value Array::at(std::size_t index) const
{
    return impl_->at(index);
}

inline bool Object::empty() const noexcept
{
    return impl_->size() == 0;
}

inline std::size_t Object::size() const noexcept
{
    return impl_->size();
}

inline bool Object::exists(std::string_view key) const
{
    return impl_->exists(key);
}

inline Value Object::get(std::string_view key) const
{
    return impl_->get(key);
}

inline void Object::set(std::string_view key, Value value) const
{
    impl_->set(key, std::move(value));
}

inline auto Object::entries() const noexcept -> entries_type
{
    return impl_->entries();
}

//------------------------------------------------

/** Return a non-empty string, or a null.
*/
inline
Value
stringOrNull(
    std::string_view s)
{
    if(! s.empty())
        return s;
    return nullptr;
}

} // dom
} // mrdox
} // clang

//------------------------------------------------

template<>
struct fmt::formatter<clang::mrdox::dom::Object>
    : fmt::formatter<std::string>
{
    auto format(
        clang::mrdox::dom::Object const& value,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string>::format(
            toString(value), ctx);
    }
};

template<>
struct fmt::formatter<clang::mrdox::dom::Value>
    : fmt::formatter<std::string>
{
    auto format(
        clang::mrdox::dom::Value const& value,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string>::format(
            toString(value), ctx);
    }
};

#endif
