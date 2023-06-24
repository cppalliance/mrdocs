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
#if 0
        // create prototype
        auto idx = duk_push_object(ctx);
        duk_push_string(ctx, "test");
        duk_push_c_function(ctx, &my_getter, 0);
        duk_def_prop(ctx, idx,
            DUK_DEFPROP_HAVE_GETTER |
            DUK_DEFPROP_SET_ENUMERABLE);
        auto result = duk_put_global_string(ctx, "Proto");
        MRDOX_ASSERT(result == 1);
#endif
    }
};

struct Access
{
    explicit Access(
        Context const& ctx) noexcept
        : impl_(ctx.impl_)
    {
    }

    explicit Access(
        Scope const& scope) noexcept
        : Access(scope.ctx_)
    {
    }

    operator duk_context*() const noexcept
    {
        return impl_->ctx;
    }

    void push(Param const& param) const
    {
        param.push(impl_->ctx);
    }

    static duk_idx_t idx(Value const& value) noexcept
    {
        return value.idx_;
    }

    static void addref(Scope& scope) noexcept
    {
        ++scope.refs_;
    }

    static void release(Scope& scope) noexcept
    {
        if(--scope.refs_ != 0)
            return;
        scope.reset();
    }

    static void swap(Value& v0, Value& v1) noexcept
    {
        std::swap(v0.scope_, v1.scope_);
        std::swap(v0.idx_, v1.idx_);
    }

    template<class T, class... Args>
    static T construct(Args&&... args)
    {
        return T(std::forward<Args>(args)...);
    }

private:
    Context::Impl* impl_;
};

//------------------------------------------------
//
// duktape wrappers
//
//------------------------------------------------

static std::string getStringProp(
    std::string_view name, Scope& scope)
{
    Access A(scope);
    MRDOX_ASSERT(duk_get_type(A, -1) == DUK_TYPE_OBJECT);
    if(! duk_get_prop_lstring(A, -1, name.data(), name.size()))
        throw Error("missing property {}", name);
    char const* s;
    if(duk_get_type(A, -1) != DUK_TYPE_STRING)
        duk_to_string(A, -1);
    duk_size_t len;
    s = duk_get_lstring(A, -1, &len);
    MRDOX_ASSERT(s);
    std::string result = std::string(s, len);
    duk_pop(A);
    return result;
}

static Error popError(Scope& scope)
{
    Access A(scope);
    Error err("{} (\"{}\" line {})",
        getStringProp("message", scope),
        getStringProp("fileName", scope),
        getStringProp("lineNumber", scope));
    duk_pop(A);
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

static void obj_construct(
    duk_context*, duk_idx_t, dom::Object*);

//------------------------------------------------

void
Param::
push(void* ctx_) const
{
    auto ctx = static_cast<duk_context*>(ctx_);
    switch(kind_)
    {
    case Kind::Undefined:
        duk_push_undefined(ctx);
        break;
    case Kind::Null:
        duk_push_null(ctx);
        break;
    case Kind::Boolean:
        duk_push_boolean(ctx, b_);
        break;
    case Kind::Integer:
        duk_push_int(ctx, i_);
        break;
    case Kind::Unsigned:
        duk_push_uint(ctx, u_);
        break;
    case Kind::Double:
        duk_push_number(ctx, d_);
        break;
    case Kind::String:
        duk_push_lstring(
            ctx, s_.data(), s_.size());
        break;
    case Kind::Value:
        duk_dup(ctx, idx_);
        break;
    case Kind::DomObject:
        duk_push_object(ctx);
        obj_construct(ctx, -1, obj_.get());
        break;
    default:
        MRDOX_UNREACHABLE();
    }
}

Param::
Param(
    Param&& other) noexcept
{
    kind_ = other.kind_;
    switch(other.kind_)
    {
    case Kind::Undefined:
    case Kind::Null:
        break;
    case Kind::Boolean:
        b_ = other.b_;
        break;
    case Kind::Integer:
        i_ = other.i_;
        break;
    case Kind::Unsigned:
        u_ = other.u_;
        break;
    case Kind::Double:
        d_ = other.d_;
        break;
    case Kind::String:
        s_ = other.s_;
        break;
    case Kind::Value:
        idx_ = other.idx_;
        break;
    case Kind::DomObject:
        obj_ = other.obj_;
        break;
    }
}

Param::
Param(
    dom::ObjectPtr const& obj) noexcept
    : kind_(Kind::DomObject)
    , obj_(obj)
{
}

Param::
~Param()
{
    switch(kind_)
    {
    case Kind::Undefined:
    case Kind::Null:
    case Kind::Boolean:
    case Kind::Integer:
    case Kind::Unsigned:
    case Kind::Double:
        break;
    case Kind::String:
        s_.~basic_string_view();
        break;
    case Kind::Value:
        break;
    case Kind::DomObject:
        obj_.~Pointer();
        break;
    default:
        MRDOX_UNREACHABLE();
    }
}

Param::
Param() noexcept = default;

Param::
Param(
    std::nullptr_t) noexcept
    : kind_(Kind::Null)
{
}

Param::
Param(
    bool b) noexcept
    : kind_(Kind::Boolean)
    , b_(b)
{
}

Param::
Param(
    int i) noexcept
    : kind_(Kind::Integer)
    , i_(i)
{
}

Param::
Param(
    unsigned int u) noexcept
    : kind_(Kind::Unsigned)
    , u_(u)
{
}

Param::
Param(
    double d) noexcept
    : kind_(Kind::Double)
    , d_(d)
{
}

Param::
Param(
    std::string_view s) noexcept
    : kind_(Kind::String)
    , s_(s)
{
}

Param::
Param(
    Value const& value) noexcept
    : kind_(Kind::Value)
    , idx_(Access::idx(value))
{
}

Param::
Param(
    dom::Value const& value) noexcept
    : Param(
        [&value]() -> Param
        {
            switch(value.kind())
            {
            case dom::Kind::Null:
                return nullptr;
            case dom::Kind::Boolean:
                return value.getBool();
            case dom::Kind::Integer:
                return static_cast<int>(
                    value.getInteger());
            case dom::Kind::String:
                return value.getString();
            case dom::Kind::Array:
                return nullptr;
            case dom::Kind::Object:
                return value.getObject();
            default:
                MRDOX_UNREACHABLE();
            }
        }())
       
{
}

//------------------------------------------------

void
Scope::
reset()
{
    Access A(ctx_);
    duk_pop_n(A, duk_get_top(A) - top_);
}

Scope::
Scope(
    Context const& ctx) noexcept
    : ctx_(ctx)
    , refs_(0)
    , top_(duk_get_top(Access(ctx)))
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
    Access A(*this);
    auto failed = duk_peval_lstring(
        A, jsCode.data(), jsCode.size());
    if(failed)
        return popError(*this);
    MRDOX_ASSERT(duk_get_type(A, -1) == DUK_TYPE_UNDEFINED);
    duk_pop(A); // result
    return Error::success();
}

Object
Scope::
getGlobalObject()
{
    Access A(*this);
    duk_push_global_object(A);
    return A.construct<Object>(-1, *this);
}

Expected<Object>
Scope::
tryGetGlobal(
    std::string_view name)
{
    Access A(*this);
    if(! duk_get_global_lstring(
        A, name.data(), name.size()))
    {
        duk_pop(A); // undefined
        return Error("global property {} not found", name);
    }
    return A.construct<Object>(duk_get_top_index(A), *this);
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
{
    Access A(*scope_);
    idx_ = duk_require_normalize_index(A, idx);
    A.addref(*scope_);
}

Value::
~Value()
{
    if( ! scope_)
        return;
    Access A(*scope_);
    if(idx_ == duk_get_top(A) - 1)
        duk_pop(A);
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

    Access A(*scope_);
    duk_dup(A, other.idx_);
    idx_ = duk_normalize_index(A, -1);
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
    Access::swap(*this, temp);
    return *this;
}

Value&
Value::
operator=(Value&& other) noexcept
{
    Value temp(std::move(other));
    Access::swap(*this, temp);
    return *this;
}

Type
Value::
type() const noexcept
{
    if(! scope_)
        return Type::undefined;
    Access A(*scope_);
    switch(duk_get_type(A, idx_))
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
    Access A(*scope_);
    return
        isObject() &&
        duk_is_array(A, idx_);
}

Expected<Value>
Value::
callImpl(
    Param const* data,
    std::size_t size) const
{
    Access A(*scope_);
    duk_dup(A, idx_);
    for(std::size_t i = 0; i < size; ++i)
        A.push(data[i]);
    auto result = duk_pcall(A, size);
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
    Access A(*scope_);
    duk_size_t size;
    auto data = duk_get_lstring(
        A, idx_, &size);
    return { data, size };
}

//------------------------------------------------
//
// Array
//
//------------------------------------------------

static duk_ret_t arr_finalizer(duk_context*);
static duk_ret_t arr_get_prop(duk_context*);

static
void
arr_construct(
    duk_context* ctx,
    duk_idx_t idx,
    dom::Array* arr)
{
    idx = duk_normalize_index(ctx, idx);
    duk_push_pointer(ctx, arr->addref());
    duk_put_prop_string(ctx, idx, "ptr");

    duk_push_c_function(ctx, &arr_finalizer, 0);
    duk_set_finalizer(ctx, idx);

    for(std::size_t i = 0; i < arr->length(); ++i)
    {
        duk_push_int(ctx, i);
        duk_push_c_function(ctx, &arr_get_prop, 1);
        duk_def_prop(ctx, idx,
            DUK_DEFPROP_HAVE_GETTER |
            DUK_DEFPROP_SET_ENUMERABLE);
    }
}

static
dom::Array*
arr_get_impl(
    duk_context* ctx, duk_idx_t idx)
{
    duk_require_object(ctx, idx);
    [[maybe_unused]] auto found =
        duk_get_prop_string(ctx, idx, "ptr");
    MRDOX_ASSERT(found == 1);
    auto impl = reinterpret_cast<dom::Array*>(
        duk_get_pointer(ctx, -1));
    duk_pop(ctx);
    return impl;
}

static
duk_ret_t
arr_finalizer(duk_context* ctx)
{
    duk_push_this(ctx);
    arr_get_impl(ctx, -1)->release();
    return 0;
}

static
duk_ret_t
arr_get_prop(duk_context* ctx)
{
    auto index = duk_to_int(ctx, -1);

    duk_push_this(ctx);
    auto arr = arr_get_impl(ctx, -1);

    auto value = arr->get(index);
    switch(value.kind())
    {
    case dom::Kind::Object:
    {
        duk_push_object(ctx);
        obj_construct(ctx, -1, value.getObject().get());
        break;
    }
    case dom::Kind::Array:
    {
        duk_push_array(ctx);
        arr_construct(ctx, -1, value.getArray().get());
        break;
    }
    case dom::Kind::String:
    {
        auto const s = value.getString();
        duk_push_lstring(ctx, s.data(), s.size());
        break;
    }
    case dom::Kind::Integer:
        duk_push_int(ctx, static_cast<
            duk_int_t>(value.getInteger()));
        break;
    case dom::Kind::Boolean:
        duk_push_boolean(ctx, value.getBool());
        break;
    case dom::Kind::Null:
        duk_push_null(ctx);
        break;
    }
    duk_swap_top(ctx, -3);
    duk_pop_2(ctx);
    return 1;
}

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
    : Value(duk_push_array(Access(scope)), scope)
{
}

std::size_t
Array::
size() const
{
    Access A(*scope_);
    return duk_get_length(A, idx_);
}

void
Array::
push_back(
    Param value) const
{
    Access A(*scope_);
    auto len = duk_get_length(A, idx_);
    A.push(value);
    if(! duk_put_prop_index(A, idx_, len))
    {
        // VFALCO What?
    }
}

//------------------------------------------------
//
// Object
//
//------------------------------------------------

static duk_ret_t obj_finalizer(duk_context*);
static duk_ret_t obj_get_prop(duk_context*);

static
void
obj_construct(
    duk_context* ctx,
    duk_idx_t idx,
    dom::Object* obj)
{
    idx = duk_normalize_index(ctx, idx);

    duk_push_pointer(ctx, obj->addref());
    duk_put_prop_string(ctx, idx, "ptr");

    duk_push_c_function(ctx, &obj_finalizer, 0);
    duk_set_finalizer(ctx, idx);

    for(auto const& prop : obj->props())
    {
        duk_push_lstring(ctx, prop.data(), prop.size());
        duk_push_c_function(ctx, &obj_get_prop, 1);
        duk_def_prop(ctx, idx,
            DUK_DEFPROP_HAVE_GETTER |
            DUK_DEFPROP_SET_ENUMERABLE);
    }
}

static
dom::Object*
obj_get_impl(
    duk_context* ctx, duk_idx_t idx)
{
    duk_require_object(ctx, idx);
    [[maybe_unused]] auto found =
        duk_get_prop_string(ctx, idx, "ptr");
    MRDOX_ASSERT(found == 1);
    auto impl = reinterpret_cast<dom::Object*>(
        duk_get_pointer(ctx, -1));
    duk_pop(ctx);
    return impl;
}

static
duk_ret_t
obj_finalizer(duk_context* ctx)
{
    duk_push_this(ctx);
    obj_get_impl(ctx, -1)->release();
    return 0;
}

static
duk_ret_t
obj_get_prop(duk_context* ctx)
{
    duk_size_t size;
    auto const data =
        duk_get_lstring(ctx, -1, &size);
    std::string_view key(data, size);

    duk_push_this(ctx);
    auto obj = obj_get_impl(ctx, -1);

    auto value = obj->get(key);
    switch(value.kind())
    {
    case dom::Kind::Object:
    {
        duk_push_object(ctx);
        obj_construct(ctx, -1, value.getObject().get());
        break;
    }
    case dom::Kind::Array:
    {
        duk_push_array(ctx);
        arr_construct(ctx, -1, value.getArray().get());
        break;
    }
    case dom::Kind::String:
    {
        auto const s = value.getString();
        duk_push_lstring(ctx, s.data(), s.size());
        break;
    }
    case dom::Kind::Integer:
        duk_push_int(ctx, static_cast<
            duk_int_t>(value.getInteger()));
        break;
    case dom::Kind::Boolean:
        duk_push_boolean(ctx, value.getBool());
        break;
    case dom::Kind::Null:
        duk_push_null(ctx);
        break;
    }
    duk_swap_top(ctx, -3);
    duk_pop_2(ctx);
    return 1;
}

Object::
Object(
    Scope& scope,
    dom::ObjectPtr const& obj)
    : Value(duk_push_object(Access(scope)), scope)
{
    Access A(*scope_);
    obj_construct(A, idx_, obj.get());
}

Object::
Object(
    int idx,
    Scope& scope) noexcept
    : Value(idx, scope)
{
    if(! scope_)
        return;
    Access A(*scope_);
    duk_require_object(A, idx_);
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
    : Value(duk_push_object(Access(scope)), scope)
{
}

void
Object::
insert(
    std::string_view name, Param value) const
{
    Access A(*scope_);
    A.push(value);
    if(! duk_put_prop_lstring(A,
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
    Param const* data,
    std::size_t size) const
{
    Access A(*scope_);
    if(! duk_get_prop_lstring(A,
            idx_, name.data(), name.size()))
        return Error("method {} not found", name);
    duk_dup(A, idx_);
    for(std::size_t i = 0; i < size; ++i)
        A.push(data[i]);
    auto rc = duk_pcall_method(A, size);
    if(rc == DUK_EXEC_ERROR)
    {
        Error err = popError(*scope_);
        duk_pop(A); // method
        return err;
    }
    return A.construct<Value>(-1, *scope_);
}

} // js
} // mrdox
} // clang
