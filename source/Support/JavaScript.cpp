//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Path.hpp>
#include <mrdox/Support/JavaScript.hpp>
#include <llvm/Support/MemoryBuffer.h>
#include <duktape.h>
//#include <duk_module_duktape.h>
#include <utility>

namespace clang {
namespace mrdox {
namespace js {

//------------------------------------------------

struct Context::Impl
{
    std::size_t refs;
    duk_context* ctx;

    ~Impl()
    {
        duk_destroy_heap(ctx);
    }

    Impl()
        : refs(1)
        , ctx(duk_create_heap_default())
    {
    }
};

struct Access
{
    duk_context* operator()(Context const& ctx) const noexcept
    {
        return ctx.impl_->ctx;
    }

    duk_context* operator()(Scope const& scope) const noexcept
    {
        return (*this)(scope.ctx_);
    }

    duk_idx_t operator()(Value const& val) const noexcept
    {
        return val.idx_;
    }

    void addref(Scope& scope) const noexcept
    {
        ++scope.refs_;
    }

    void release(Scope& scope) const noexcept
    {
        if(--scope.refs_ != 0)
            return;
        scope.reset();
    }

    void swap(Value& v0, Value& v1) const noexcept
    {
        std::swap(v0.scope_, v1.scope_);
        std::swap(v0.idx_, v1.idx_);
    }

    template<class T, class... Args>
    T construct(Args&&... args) const
    {
        return T(std::forward<Args>(args)...);
    }
};

constexpr Access A{};

//------------------------------------------------
//
// duktape wrappers
//
//------------------------------------------------

static std::string getStringProp(
    std::string_view name, Scope& scope)
{
    MRDOX_ASSERT(duk_get_type(A(scope), -1) == DUK_TYPE_OBJECT);
    if(! duk_get_prop_lstring(A(scope), -1, name.data(), name.size()))
        throw Error("missing property {}", name);
    char const* s;
    if(duk_get_type(A(scope), -1) != DUK_TYPE_STRING)
        duk_to_string(A(scope), -1);
    duk_size_t len;
    s = duk_get_lstring(A(scope), -1, &len);
    MRDOX_ASSERT(s);
    std::string result = std::string(s, len);
    duk_pop(A(scope));
    return result;
}

static Error popError(Scope& scope)
{
    Error err("{} (\"{}\" line {})",
        getStringProp("message", scope),
        getStringProp("fileName", scope),
        getStringProp("lineNumber", scope));
    duk_pop(A(scope));
    return err;
}

//------------------------------------------------

Context::
~Context()
{
    if(--impl_->refs == 0)
        delete impl_;
}

Context::
Context()
    : impl_(new Impl)
{
}

Context::
Context(
    Context const& other) noexcept
    : impl_(other.impl_)
{
    ++impl_->refs;
}

//------------------------------------------------

Arg::
Arg(Value const& value) noexcept
    : push(&push_Value)
    , index(A(value))
{
}

void
Arg::
push_bool(
    Arg const& arg, Scope& scope)
{
    duk_push_boolean(A(scope), arg.number != 0);
}

void
Arg::
push_int(
    Arg const& arg, Scope& scope)
{
    duk_push_int(A(scope), arg.number);
}

void
Arg::
push_uint(
    Arg const& arg, Scope& scope)
{
    duk_push_uint(A(scope), arg.u_number);
}

void
Arg::
push_lstring(
    Arg const& arg, Scope& scope)
{
    duk_push_lstring(A(scope), arg.data, arg.size);
}

void
Arg::
push_Value(
    Arg const& arg, Scope& scope)
{
    duk_dup(A(scope), arg.index);
}

//------------------------------------------------

void
Scope::
reset()
{
    duk_pop_n(A(ctx_), duk_get_top(A(ctx_)) - top_);
}

Scope::
Scope(
    Context const& ctx) noexcept
    : ctx_(ctx)
    , refs_(0)
    , top_(duk_get_top(A(ctx)))
{
}

Scope::
~Scope()
{
    MRDOX_ASSERT(refs_ == 0);
    reset();
}

Error
Scope::
script(
    std::string_view jsCode)
{
    auto failed = duk_peval_lstring(
        A(*this), jsCode.data(), jsCode.size());
    if(failed)
        return popError(*this);
    MRDOX_ASSERT(duk_get_type(A(*this), -1) == DUK_TYPE_UNDEFINED);
    duk_pop(A(*this)); // result
    return Error::success();
}

Expected<Object>
Scope::
tryGetGlobal(
    std::string_view name)
{
    if(! duk_get_global_lstring(
        A(*this), name.data(), name.size()))
    {
        duk_pop(A(*this)); // undefined
        return Error("global property {} not found", name);
    }
    return A.construct<Object>(duk_get_top_index(A(*this)), *this);
}

Object
Scope::
getGlobal(
    std::string_view name)
{
    auto object = tryGetGlobal(name);
    if(! object)
        throw object.getError();
    return *object;
}

//------------------------------------------------
//
// Value
//
//------------------------------------------------

Value::
Value(
    int idx,
    Scope& scope) noexcept
    : scope_(&scope)
    , idx_(duk_require_normalize_index(A(scope), idx))
{
    A.addref(*scope_);
}

Value::
~Value()
{
    if( ! scope_)
        return;
    if(idx_ == duk_get_top(A(*scope_)) - 1)
        duk_pop(A(*scope_));
    A.release(*scope_);
}

// construct an empty value
Value::
Value() noexcept
    : scope_(nullptr)
    , idx_(DUK_INVALID_INDEX)
{
}

Value::
Value(
    Value const& other)
    : scope_(other.scope_)
{
    if(! scope_)
    {
        idx_ = DUK_INVALID_INDEX;
        return;
    }

    duk_dup(A(*scope_), other.idx_);
    idx_ = duk_normalize_index(A(*scope_), -1);
    A.addref(*scope_);
}

Value::
Value(
    Value&& other) noexcept
    : scope_(other.scope_)
    , idx_(other.idx_)
{
    other.scope_ = nullptr;
    other.idx_ = DUK_INVALID_INDEX;
}

Value&
Value::
operator=(Value const& other)
{
    Value temp(other);
    A.swap(*this, temp);
    return *this;
}

Value&
Value::
operator=(Value&& other) noexcept
{
    Value temp(std::move(other));
    A.swap(*this, temp);
    return *this;
}

Type
Value::
type() const noexcept
{
    if(! scope_)
        return Type::undefined;
    switch(duk_get_type(A(*scope_), idx_))
    {
    case DUK_TYPE_UNDEFINED: return Type::undefined;
    case DUK_TYPE_NULL:      return Type::null;
    case DUK_TYPE_BOOLEAN:   return Type::boolean;
    case DUK_TYPE_NUMBER:    return Type::number;
    case DUK_TYPE_STRING:    return Type::string;
    case DUK_TYPE_OBJECT:    return Type::object;
    case DUK_TYPE_NONE:
    default:
        // unknown type
        MRDOX_UNREACHABLE();
    }
}

bool
Value::
isArray() const noexcept
{
    return
        isObject() &&
        duk_is_array(A(*scope_), idx_);
}

Expected<Value>
Value::
callImpl(
    Arg const* data,
    std::size_t size) const
{
    duk_dup(A(*scope_), idx_);
    for(std::size_t i = 0; i < size; ++i)
        data[i].push(data[i], *scope_);
    auto result = duk_pcall(A(*scope_), size);
    if(result == DUK_EXEC_ERROR)
        return popError(*scope_);
    return A.construct<Value>(-1, *scope_);
}

//------------------------------------------------
//
// String
//
//------------------------------------------------

String::
String(
    int idx,
    Scope& scope) noexcept
    : Value(idx, scope)
{
}

String::
String(
    Value value)
{
    if(! value.isString())
        throw Error("not a string");
    static_cast<Value&>(*this) = std::move(value);
}

String&
String::
operator=(
    Value value)
{
    if(! value.isString())
        throw Error("not a string");
    static_cast<Value&>(*this) = std::move(value);
    return *this;
}

std::string_view
String::
get() const noexcept
{
    if(! scope_)
        return {};
    duk_size_t size;
    auto data = duk_get_lstring(
        A(*scope_), idx_, &size);
    return { data, size };
}

//------------------------------------------------
//
// Array
//
//------------------------------------------------

Array::
Array(
    int idx,
    Scope& scope) noexcept
    : Value(idx, scope)
{
}

Array::
Array(
    Value value)
{
    if(! value.isArray())
        throw Error("not an Array");
    static_cast<Value&>(*this) = std::move(value);
}

Array&
Array::
operator=(
    Value value)
{
    if(! value.isArray())
        throw Error("not an Array");
    static_cast<Value&>(*this) = std::move(value);
    return *this;
}

Array::
Array(Scope& scope)
    : Value(duk_push_array(A(scope)), scope)
{
}

std::size_t
Array::
size() const
{
    return duk_get_length(A(*scope_), idx_);
}

void
Array::
push_back(
    Arg value) const
{
    auto len = duk_get_length(A(*scope_), idx_);
    value.push(value, *scope_);
    if(! duk_put_prop_index(A(*scope_), idx_, len))
    {
        // VFALCO What?
    }
}

//------------------------------------------------
//
// Object
//
//------------------------------------------------

Object::
Object(
    int idx,
    Scope& scope) noexcept
    : Value(idx, scope)
{
    if(scope_)
        duk_require_object(A(*scope_), idx_);
}

Object::
Object(
    Value value)
{
    if(! value.isObject())
        throw Error("not an Object");
    static_cast<Value&>(*this) = std::move(value);
}

Object&
Object::
operator=(
    Value value)
{
    if(! value.isObject())
        throw Error("not an Object");
    static_cast<Value&>(*this) = std::move(value);
    return *this;
}

Object::
Object(Scope& scope)
    : Value(duk_push_object(A(scope)), scope)
{
}

void
Object::
insert(
    std::string_view name, Arg value) const
{
    value.push(value, *scope_);
    if(! duk_put_prop_lstring(A(*scope_),
        idx_, name.data(), name.size()))
    {
        // VFALCO What?
    }
}

//------------------------------------------------

Expected<Value>
Object::
callImpl(
    std::string_view name,
    Arg const* data,
    std::size_t size) const
{
    if(! duk_get_prop_lstring(A(*scope_),
            idx_, name.data(), name.size()))
        return Error("method {} not found", name);
    duk_dup(A(*scope_), idx_);
    for(std::size_t i = 0; i < size; ++i)
        data[i].push(data[i], *scope_);
    auto rc = duk_pcall_method(A(*scope_), size);
    if(rc == DUK_EXEC_ERROR)
    {
        Error err = popError(*scope_);
        duk_pop(A(*scope_)); // method
        return err;
    }
    return A.construct<Value>(-1, *scope_);
}

} // js
} // mrdox
} // clang
