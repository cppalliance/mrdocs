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
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/String.hpp>
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

class ArrayImpl;

/** An array of values.
*/
class MRDOX_DECL
    Array final
{
    std::shared_ptr<ArrayImpl> impl_;

public:
    using value_type = Value;
    using size_type = std::size_t;
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
    Array(impl_type impl) noexcept;

    /** Return the implementation used by this object.
    */
    auto impl() const noexcept -> impl_type const&;

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

    /** Return the i-th element, without bounds checking.
    */
    value_type operator[](size_type i) const;

    /** Return the i-th element.

        @throw Exception `i >= size()`
    */
    value_type at(size_type i) const;

    /** Return a diagnostic string.
    */
    friend
    std::string
    toString(Array const&);
};

//------------------------------------------------
//
// ArrayImpl
//
//------------------------------------------------

/** Abstract array interface.

    This interface is used by Array types.
*/
class MRDOX_DECL
    ArrayImpl
{
public:
    /// @copydoc Array::value_type
    using value_type = Array::value_type;

    /// @copydoc Array::size_type
    using size_type = Array::size_type;

    /** Destructor.
    */
    virtual ~ArrayImpl() = 0;

    /** Return the number of elements in the array.
    */
    virtual size_type size() const = 0;

    /** Return the i-th element, without bounds checking.
    */
    virtual value_type get(size_type i) const = 0;
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

class ObjectImpl;

/** A container of key and value pairs.
*/
class MRDOX_DECL
    Object final
{
    std::shared_ptr<ObjectImpl> impl_;

public:
    /** A key/value pair.

        This is a copyable, movable value type.
    */
    struct value_type;

    /** A key/value pair.

        This is a read-only reference to the underlying data.
    */
    struct reference;

    using difference_type = std::ptrdiff_t;
    using size_type = std::size_t;
    using pointer = reference;

    /** A constant iterator referencing an element in an Object.
    */
    class iterator;

    /** A constant iterator referencing an element in an Object.
    */
    using const_iterator = iterator;

    /** The type of storage used by the default implementation.
    */
    using storage_type = std::vector<value_type>;

    /** The type of implementation.
    */
    using impl_type = std::shared_ptr<ObjectImpl>;

    //--------------------------------------------

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
    explicit Object(storage_type list);

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
    void set(std::string_view key, Value value) const;

    /** Return an iterator to the beginning of the range of elements.
    */
    iterator begin() const;

    /** Return an iterator to the end of the range of elements.
    */
    iterator end() const;

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
    virtual ~ObjectImpl() = 0;

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
    virtual void set(std::string_view key, Value value) = 0;
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
    void set(std::string_view, Value) override;

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
    void set(std::string_view key, Value value) override;
};

//------------------------------------------------
//
// Value
//
//------------------------------------------------

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

        @throw Exception `! isArray()`
    */
    Array const&
    getArray() const;

    /** Return the object.

        @throw Exception `! isObject()`
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
//
// Object::value_type
//
//------------------------------------------------

struct Object::value_type
{
    std::string_view key;
    Value value;

    value_type() = default;

    value_type(
        std::string_view const& key_,
        Value value_) noexcept
        : key(key_)
        , value(std::move(value_))
    {
    }

    template<class U>
    requires std::constructible_from<
        std::pair<std::string_view, Value>, U>
    value_type(
        U&& u)
        : value_type(
            [&u]
            {
                std::pair<std::string_view, Value> p(
                    std::forward<U>(u));
                return value_type(
                    p.first, std::move(p).second);
            })
    {
    }

    value_type* operator->() noexcept
    {
        return this;
    }

    value_type const* operator->() const noexcept
    {
        return this;
    }
};

//------------------------------------------------
//
// Object::reference
//
//------------------------------------------------

struct Object::reference
{
    std::string_view const key;
    Value const& value;

    reference(reference const&) = default;

    reference(
        std::string_view const& key_,
        Value const& value_) noexcept
        : key(key_)
        , value(value_)
    {
    }

    reference(
        value_type const& kv) noexcept
        : key(kv.key)
        , value(kv.value)
    {
    }

    operator value_type() const
    {
        return value_type(key, value);
    }

    reference const* operator->() noexcept
    {
        return this;
    }

    reference const* operator->() const noexcept
    {
        return this;
    }
};

//------------------------------------------------
//
// Object::iterator
//
//------------------------------------------------

class MRDOX_DECL
    Object::iterator
{
    ObjectImpl const* obj_ = nullptr;
    std::size_t i_ = 0;

    friend class Object;

    iterator(
        ObjectImpl const& obj,
        std::size_t i) noexcept
        : obj_(&obj)
        , i_(i)
    {
    }

public:
    using value_type = Object::value_type;
    using reference = Object::reference;
    using difference_type = std::ptrdiff_t;
    using size_type  = std::size_t;
    using pointer = reference;
    using iterator_category =
        std::random_access_iterator_tag;

    iterator() = default;

    reference operator*() const noexcept
    {
        return obj_->get(i_);
    }

    pointer operator->() const noexcept
    {
        return obj_->get(i_);
    }

    reference operator[](difference_type n) const noexcept
    {
        return obj_->get(i_ + n);
    }

    iterator& operator--() noexcept
    {
        --i_;
        return *this;
    }

    iterator operator--(int) noexcept
    {
        auto temp = *this;
        --*this;
        return temp;
    }

    iterator& operator++() noexcept
    {
        ++i_;
        return *this;
    }

    iterator operator++(int) noexcept
    {
        auto temp = *this;
        ++*this;
        return temp;
    }

    auto operator<=>(iterator const& other) const noexcept
    {
        MRDOX_ASSERT(obj_ == other.obj_);
        return i_ <=> other.i_;
    }

#if 1
    // VFALCO Why does ranges need these? Isn't <=> enough?
    bool operator==(iterator const& other) const noexcept
    {
        MRDOX_ASSERT(obj_ == other.obj_);
        return i_ == other.i_;
    }

    bool operator!=(iterator const& other) const noexcept
    {
        MRDOX_ASSERT(obj_ == other.obj_);
        return i_ != other.i_;
    }
#endif

    iterator& operator-=(difference_type n) noexcept
    {
        i_ -= n;
        return *this;
    }

    iterator& operator+=(difference_type n) noexcept
    {
        i_ += n;
        return *this;
    }

    iterator operator-(difference_type n) const noexcept
    {
        return iterator(*obj_, i_ - n);
    }

    iterator operator+(difference_type n) const noexcept
    {
        return iterator(*obj_, i_ + n);
    }

    difference_type operator-(iterator other) const noexcept
    {
        MRDOX_ASSERT(obj_ == other.obj_);
        return static_cast<difference_type>(i_) - other.i_;
    }

    friend iterator operator+(difference_type n, iterator it) noexcept
    {
        return it + n;
    }
};

//------------------------------------------------

//
// implementation
//

inline Array::Array(impl_type impl) noexcept
    : impl_(std::move(impl))
{
    MRDOX_ASSERT(impl_);
}
inline auto Array::impl() const noexcept -> impl_type const&
{
    return impl_;
}

inline bool Array::empty() const noexcept
{
    return impl_->size() == 0;
}

inline auto Array::size() const noexcept -> size_type
{
    return impl_->size();
}

inline auto Array::get(std::size_t i) const -> value_type
{
    return impl_->get(i);
}

inline auto Array::operator[](std::size_t i) const -> value_type
{
    return get(i);
}

inline auto Array::at(std::size_t i) const -> value_type
{
    if(i < size())
        return get(i);
    Error("out of range").Throw();
}

//------------------------------------------------

inline bool Object::empty() const
{
    return size() == 0;
}

inline std::size_t Object::size() const
{
    return impl_->size();
}

inline auto Object::get(std::size_t i) const -> reference
{
    return impl_->get(i);
}

inline auto Object::operator[](size_type i) const -> reference
{
    return get(i);
}

inline auto Object::at(size_type i) const -> reference
{
    if(i < size())
        return get(i);
    Error("out of range").Throw();
}

inline Value Object::find(std::string_view key) const
{
    return impl_->find(key);
}

inline void Object::set(std::string_view key, Value value) const
{
    impl_->set(key, std::move(value));
}

inline auto Object::begin() const -> iterator
{
    return iterator(*impl_, 0);
}

inline auto Object::end() const -> iterator
{
    return iterator(*impl_, impl_->size());
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

// VFALCO These needs to be constrained to indicate
// that the common reference is always constant.

template<
    template <class> class TQual, 
    template <class> class UQual>
struct std::basic_common_reference<
    ::clang::mrdox::dom::Object::value_type,
    ::clang::mrdox::dom::Object::reference,
    TQual, UQual>
{
    using type = ::clang::mrdox::dom::Object::reference;
};

template<
    template <class> class TQual, 
    template <class> class UQual>
struct std::basic_common_reference<
    ::clang::mrdox::dom::Object::reference,
    ::clang::mrdox::dom::Object::value_type,
    TQual, UQual>
{
    using type = ::clang::mrdox::dom::Object::reference;
};

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
