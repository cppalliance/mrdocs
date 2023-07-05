//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Support/Dom.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/RangeFor.hpp>
#include <algorithm>
#include <atomic>
#include <memory>
#include <new>
#include <ranges>

namespace clang {
namespace mrdox {
namespace dom {

static_assert(std::random_access_iterator<Object::iterator>);
static_assert(std::ranges::random_access_range<Object>);

//------------------------------------------------
//
// String
//
//------------------------------------------------

namespace {

// Variable-length integer
template<std::unsigned_integral U>
class varint
{
    using Digit = std::uint8_t;
    static_assert(CHAR_BIT == 8);
    static_assert(sizeof(Digit) == 1);
    static_assert(std::unsigned_integral<Digit>);
    static constexpr auto N = (sizeof(U) * 8 + 6) / 7;
    static constexpr Digit Bits = 8 * sizeof(Digit) - 1;
    static constexpr Digit EndBit = 1 << Bits;
    static constexpr Digit DigMask = (1 << Bits) - 1;
    char buf_[N];
    std::size_t n_ = 0;

public:
    explicit varint(U u) noexcept
    {
        auto p = buf_;
        for(;;)
        {
            ++n_;
            auto dig = u & DigMask;
            u >>= Bits;
            if(u == 0)
            {
                // hi bit set means stop
                dig |= EndBit;
                *p = static_cast<char>(dig);
                break;
            }
            *p++ = static_cast<char>(dig);
        }
    }

    std::string_view get() const noexcept
    {
        return { buf_, n_ };
    }

    static U read(char const*& p) noexcept
    {
        Digit dig = *p++;
        if(dig & EndBit)
            return dig & DigMask;
        U u = dig;
        unsigned Shift = Bits;
        for(;;)
        {
            auto dig = *p++;
            if(dig & EndBit)
            {
                dig &= DigMask;
                u |= dig << Shift;
                return u;
            }
            u |= dig << Shift;
            Shift += Bits;
        }
    }
};

static constinit char sz_empty[1] = { '\0' };

} // (anon)

// An allocated string is stored in one buffer
// laid out thusly:
//
// Impl
// size     varint
// chars    char[]
//
struct String::Impl
{
    std::atomic<std::size_t> refs;

    explicit Impl(
        std::string_view s,
        varint<std::size_t> const& uv) noexcept
        : refs(1)
    {
        auto p = reinterpret_cast<char*>(this+1);
        auto vs = uv.get();
        std::memcpy(p, vs.data(), vs.size());
        p += vs.size();
        std::memcpy(p, s.data(), s.size());
        p[s.size()] = '\0';
    }

    std::string_view get() const noexcept
    {
        auto p = reinterpret_cast<char const*>(this+1);
        auto const size = varint<std::size_t>::read(p);
        return { p, size };
    }
};

auto
String::
allocate(
    std::string_view s) ->
    Impl*
{
    std::allocator<Impl> alloc;
    varint<std::size_t> uv(s.size());   
    auto n =
        sizeof(Impl) +      // header
        uv.get().size() +   // size (varint)
        s.size() +          // string data
        1 +                 // null term '\0'
        (sizeof(Impl) - 1); // round up to nearest sizeof(Impl)
    return new(alloc.allocate(n / sizeof(Impl))) Impl(s, uv);
}

void
String::
deallocate(
    Impl* impl) noexcept
{
    std::allocator<Impl> alloc;
    auto const s = impl->get();
    varint<std::size_t> uv(s.size());
    auto n =
        sizeof(Impl) +      // header
        uv.get().size() +   // size (varint)
        s.size() +          // string data
        1 +                 // null term '\0'
        (sizeof(Impl) - 1); // round up to nearest sizeof(Impl)
    std::destroy_at(impl);
    alloc.deallocate(impl, n / sizeof(Impl));
}

//------------------------------------------------

String::
~String()
{
    if(! impl_)
        return;
    if(--impl_->refs > 0)
        return;
    deallocate(impl_);
}

String::
String() noexcept
    : psz_(&sz_empty[0])
{
}

String::
String(
    String&& other) noexcept
    : String()
{
    swap(other);
}

String::
String(
    String const& other) noexcept
    : impl_(other.impl_)
    , psz_(other.psz_)
{
    if(impl_)
        ++impl_->refs;
}

String::
String(
    std::string_view s)
    : impl_(allocate(s))
{
}

String&
String::
operator=(
    String&& other) noexcept
{
    String temp(std::move(other));
    swap(temp);
    return *this;
}

String&
String::
operator=(
    String const& other) noexcept
{
    String temp(other);
    swap(temp);
    return *this;
}

bool
String::
empty() const noexcept
{
    if(impl_)
        return impl_->get().size() == 0;
    return psz_[0] == '\0';
}

std::string_view
String::
get() const noexcept
{
    if(psz_)
        return std::string_view(psz_);
    return impl_->get();
}

//------------------------------------------------
//
// Array
//
//------------------------------------------------

Array::
~Array() = default;

Array::
Array()
    : impl_(std::make_shared<DefaultArrayImpl>())
{
}

Array::
Array(
    Array&& other)
    : Array()
{
    swap(other);
}

Array::
Array(
    Array const& other)
    : impl_(other.impl_)
{
}

Array&
Array::
operator=(
    Array&& other)
{
    Array temp;
    swap(temp);
    swap(other);
    return *this;
}

std::string
toString(
    Array const& arr)
{
    if(arr.empty())
        return "[]";
    std::string s = "[";
    {
        auto const n = arr.size();
        auto insert = std::back_inserter(s);
        for(std::size_t i = 0; i < n; ++i)
        {
            if(i != 0)
                fmt::format_to(insert,
                    ", {}",
                    toString(arr.at(i)));
            else
                fmt::format_to(insert,
                    " {}",
                    toStringChild(arr.at(i)));
        }
    }
    s += " ]";
    return s;
}

//------------------------------------------------
//
// ArrayImpl
//
//------------------------------------------------

ArrayImpl::
~ArrayImpl() = default;

void
ArrayImpl::
emplace_back(
    value_type value)
{
    Error("Array is const").Throw();
}

//------------------------------------------------
//
// DefaultArrayImpl
//
//------------------------------------------------

auto
DefaultArrayImpl::
size() const ->
    size_type
{
    return elements_.size();
}

auto
DefaultArrayImpl::
get(
    size_type i) const ->
        value_type
{
    MRDOX_ASSERT(i < elements_.size());
    return elements_[i];
}

void
DefaultArrayImpl::
emplace_back(
    value_type value)
{
    elements_.emplace_back(std::move(value));
}

//------------------------------------------------
//
// Object
//
//------------------------------------------------

Object::
~Object() = default;

Object::
Object()
    : impl_(std::make_shared<DefaultObjectImpl>())
{
}

Object::
Object(
    Object&& other)
    : Object()
{
    swap(other);
}

Object::
Object(
    Object const& other) noexcept
    : impl_(other.impl_)
{
}

Object::
Object(
    storage_type list)
    : impl_(std::make_shared<
        DefaultObjectImpl>(std::move(list)))
{
}

Object&
Object::
operator=(Object&& other)
{
    Object temp;
    swap(temp);
    swap(other);
    return *this;
}

bool
Object::
exists(std::string_view key) const
{
    for(auto const& kv : *this)
        if(kv.key == key)
            return true;

    return false;
}

std::string
toString(
    Object const& obj)
{
    if(obj.empty())
        return "{}";
    std::string s = "{";
    {
        auto insert = std::back_inserter(s);
        for(auto const& kv : RangeFor(obj))
        {
            if(! kv.first)
                s.push_back(',');
            fmt::format_to(insert,
                " {} : {}",
                kv.value.key,
                toStringChild(kv.value.value));
        }
    }
    s += " }";
    return s;
}

//------------------------------------------------
//
// ObjectImpl
//
//------------------------------------------------

ObjectImpl::
~ObjectImpl() = default;

//------------------------------------------------
//
// DefaultObjectImpl
//
//------------------------------------------------

DefaultObjectImpl::
DefaultObjectImpl() noexcept = default;

DefaultObjectImpl::
DefaultObjectImpl(
    storage_type entries) noexcept
    : entries_(std::move(entries))
{
}

std::size_t
DefaultObjectImpl::
size() const
{
    return entries_.size();
}

auto
DefaultObjectImpl::
get(std::size_t i) const ->
    reference
{
    MRDOX_ASSERT(i < entries_.size());
    return entries_[i];
}

Value
DefaultObjectImpl::
find(std::string_view key) const
{
    auto it = std::find_if(
        entries_.begin(), entries_.end(),
        [key](auto const& kv)
        {
            return kv.key == key;
        });
    if(it == entries_.end())
        return nullptr;
    return it->value;
}

void
DefaultObjectImpl::
set(String key, Value value)
{
    auto it = std::find_if(
        entries_.begin(), entries_.end(),
        [key](auto const& kv)
        {
            return kv.key == key;
        });
    if(it == entries_.end())
        entries_.emplace_back(
            key, std::move(value));
    else
        it->value = std::move(value);
}

//------------------------------------------------
//
// LazyObjectImpl
//
//------------------------------------------------

ObjectImpl&
LazyObjectImpl::
obj() const
{
    auto impl = sp_.load();
    if(impl)
        return *impl;
    impl_type expected = nullptr;
    if(sp_.compare_exchange_strong(
            expected, construct().impl()))
        return *sp_.load();
    return *expected;
}

std::size_t
LazyObjectImpl::
size() const
{
    return obj().size();
}

auto
LazyObjectImpl::
get(std::size_t i) const ->
    reference
{
    return obj().get(i);
}

Value
LazyObjectImpl::
find(std::string_view key) const
{
    return obj().find(key);
}

void
LazyObjectImpl::
set(String key, Value value)
{
    obj().set(std::move(key), value);
}

//------------------------------------------------
//
// Value
//
//------------------------------------------------

Value::
~Value()
{
    switch(kind_)
    {
    case Kind::Null:
    case Kind::Boolean:
    case Kind::Integer:
        break;
    case Kind::String:
        std::destroy_at(&str_);
        break;
    case Kind::Array:
        std::destroy_at(&arr_);
        break;
    case Kind::Object:
        std::destroy_at(&obj_);
        break;
    default:
        MRDOX_UNREACHABLE();
    }
}

Value::
Value() noexcept
    : kind_(Kind::Null)
{
}

Value::
Value(
    Value const& other)
    : kind_(other.kind_)
{
    switch(kind_)
    {
    case Kind::Null:
        break;
    case Kind::Boolean:
        std::construct_at(&b_, other.b_);
        break;
    case Kind::Integer:
        std::construct_at(&i_, other.i_);
        break;
    case Kind::String:
        std::construct_at(&str_, other.str_);
        break;
    case Kind::Array:
        std::construct_at(&arr_, other.arr_);
        break;
    case Kind::Object:
        std::construct_at(&obj_, other.obj_);
        break;
    default:
        MRDOX_UNREACHABLE();
    }
}

Value::
Value(
    Value&& other) noexcept
    : kind_(other.kind_)
{
    switch(kind_)
    {
    case Kind::Null:
        break;
    case Kind::Boolean:
        std::construct_at(&b_, other.b_);
        break;
    case Kind::Integer:
        std::construct_at(&i_, other.i_);
        break;
    case Kind::String:
        std::construct_at(&str_, std::move(other.str_));
        std::destroy_at(&other.str_);
        break;
    case Kind::Array:
        std::construct_at(&arr_, std::move(other.arr_));
        std::destroy_at(&other.arr_);
        break;
    case Kind::Object:
        std::construct_at(&obj_, std::move(other.obj_));
        std::destroy_at(&other.obj_);
        break;
    default:
        MRDOX_UNREACHABLE();
    }
    other.kind_ = Kind::Null;
}

Value::
Value(
    std::nullptr_t) noexcept
    : kind_(Kind::Null)
{
}

Value::
Value(
    std::int64_t i) noexcept
    : kind_(Kind::Integer)
    , i_(i)
{
}

Value::
Value(
    String str) noexcept
    : kind_(Kind::String)
    , str_(std::move(str))
{
}

Value::
Value(
    Array arr) noexcept
    : kind_(Kind::Array)
    , arr_(std::move(arr))
{
}

Value::
Value(
    Object obj) noexcept
    : kind_(Kind::Object)
    , obj_(std::move(obj))
{
}

Value&
Value::
operator=(
    Value&& other) noexcept
{
    Value temp(std::move(other));
    swap(temp);
    return *this;
}

Value&
Value::
operator=(
    Value const& other)
{
    Value temp(other);
    swap(temp);
    return *this;
}

dom::Kind
Value::
kind() const noexcept
{
    switch(kind_)
    {
    case Kind::Null:        return dom::Kind::Null;
    case Kind::Boolean:     return dom::Kind::Boolean;
    case Kind::Integer:     return dom::Kind::Integer;
    case Kind::String:      return dom::Kind::String;
    case Kind::Array:       return dom::Kind::Array;
    case Kind::Object:      return dom::Kind::Object;
    default:
        MRDOX_UNREACHABLE();
    }
}

bool
Value::
isTruthy() const noexcept
{
    switch(kind_)
    {
    case Kind::Null: return false;
    case Kind::Boolean: return b_;
    case Kind::Integer: return i_ != 0;
    case Kind::String: return str_.size() > 0;
    case Kind::Array: return ! arr_.empty();
    case Kind::Object:
        // this can call construct()
        return ! obj_.empty();
    default:
        MRDOX_UNREACHABLE();
    }
}

Array const&
Value::
getArray() const
{
    if(kind_ == Kind::Array)
        return arr_;
    Error("not an Array").Throw();
}

Object const&
Value::
getObject() const
{
    if(kind_ == Kind::Object)
        return obj_;
    Error("not an Object").Throw();
}

void
Value::
swap(
    Value& other) noexcept
{
    Value temp(std::move(*this));
    std::destroy_at(this);
    std::construct_at(this, std::move(other));
    std::destroy_at(&other);
    std::construct_at(&other, std::move(temp));
}

std::string
toString(
    Value const& value)
{
    switch(value.kind_)
    {
    case Value::Kind::Array:
        return toString(value.arr_);
    case Value::Kind::Object:
        return toString(value.obj_);
    default:
        return toStringChild(value);
    }
}

std::string
toStringChild(
    Value const& value)
{
    switch(value.kind_)
    {
    case Value::Kind::Null:
        return "null";
    case Value::Kind::Boolean:
        return value.b_ ? "true" : "false";
    case Value::Kind::Integer:
        return std::to_string(value.i_);
    case Value::Kind::String:
        return fmt::format("\"{}\"", value.str_);
    case Value::Kind::Array:
    {
        if(! value.arr_.empty())
            return "[...]";
        return "[]";
    }
    case Value::Kind::Object:
    {
        if(! value.obj_.empty())
            return "{...}";
        return "{}";
    }
    default:
        MRDOX_UNREACHABLE();
    }
}

//------------------------------------------------

} // dom
} // mrdox
} // clang
