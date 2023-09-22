//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "lib/Support/Error.hpp"
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/Path.hpp>
#include <mrdox/Support/JavaScript.hpp>
#include <llvm/Support/MemoryBuffer.h>
#include <duktape.h>
#include <utility>

#include <llvm/Support/raw_ostream.h>

namespace clang {
namespace mrdox {
namespace js {

/*  Proxy Traps

    has                 [[HasProperty]]
    get                 [[Get]]

    ownKeys             [[OwnPropertyKeys]]
    enumerate           [[Enumerate]]
    deleteProperty      [[Delete]]
    apply               [[Call]]
    defineProperty      [[DefineOwnProperty]]
    getPrototypeOf      [[GetPrototypeOf]]
    setPrototypeOf      [[SetPrototypeOf]]
    isExtensible        [[IsExtensible]]
    preventExtensions   [[PreventExtensions]]
    construct           [[Construct]]
    getOwnPropertyDescriptor    [[GetOwnProperty]]

    https://www.digitalocean.com/community/tutorials/js-proxy-traps
*/

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

struct Access
{
    duk_context* ctx_ = nullptr;
    Context::Impl* impl_ = nullptr;;

    explicit Access(duk_context* ctx) noexcept
        : ctx_(ctx)
    {
    }

    explicit Access(Context const& ctx) noexcept
        : ctx_(ctx.impl_->ctx)
        , impl_(ctx.impl_)
    {
    }

    explicit Access(Scope const& scope) noexcept
        : Access(scope.ctx_)
    {
    }

    operator duk_context*() const noexcept
    {
        return ctx_;
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
};

//------------------------------------------------
//
// Duktape helpers
//
//------------------------------------------------

// return string_view at stack idx
static
std::string_view
dukM_get_string(
    Access& A, duk_idx_t idx)
{
    duk_size_t size;
    auto const data =
        duk_get_lstring(A, idx, &size);
    return std::string_view(data, size);
}

// push string onto stack
static
void
dukM_push_string(
    Access& A, std::string_view s)
{
    duk_push_lstring(A, s.data(), s.size());
}

// set an object's property
static
void
dukM_put_prop_string(
    Access& A, duk_idx_t idx, std::string_view s)
{
    duk_put_prop_lstring(A, idx, s.data(), s.size());
}

// get the property of an object as a string
static
std::string
dukM_get_prop_string(
    std::string_view name, Scope& scope)
{
    Access A(scope);
    MRDOX_ASSERT(duk_get_type(A, -1) == DUK_TYPE_OBJECT);
    if(! duk_get_prop_lstring(A, -1, name.data(), name.size()))
        formatError("missing property {}", name).Throw();
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

// return an Error from a JavaScript Error on the stack
static
Error
dukM_popError(Scope& scope)
{
    Access A(scope);
    auto err = formatError(
        "{} (\"{}\" line {})",
            dukM_get_prop_string("message", scope),
            dukM_get_prop_string("fileName", scope),
            dukM_get_prop_string("lineNumber", scope));
    duk_pop(A);
    return err;
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
        return dukM_popError(*this);
    MRDOX_ASSERT(duk_get_type(A, -1) == DUK_TYPE_UNDEFINED);
    duk_pop(A); // result
    return Error::success();
}

Value
Scope::
getGlobalObject()
{
    Access A(*this);
    duk_push_global_object(A);
    return A.construct<Value>(-1, *this);
}

Expected<Value>
Scope::
getGlobal(
    std::string_view name)
{
    Access A(*this);
    if(! duk_get_global_lstring(
        A, name.data(), name.size()))
    {
        duk_pop(A); // undefined
        return formatError("global property {} not found", name);
    }
    return A.construct<Value>(duk_get_top_index(A), *this);
}

//------------------------------------------------
//
// Array, Object
//
//------------------------------------------------

struct ArrayBase
{
    static dom::Array* get(Access& A, duk_idx_t idx);
};

// Uses ES6 Getters/Setters
struct ArrayGetSet : ArrayBase
{
    static void push(Access& A, dom::Array const& arr);
};

// Uses ES6 Proxy
struct ArrayProxy : ArrayBase
{
    static void push(Access& A, dom::Array const& arr);

};

struct ObjectBase
{
    static dom::Object* get(Access& A, duk_idx_t idx);
};

// Uses ES6 Getters/Setters
struct ObjectGetSet : ObjectBase
{
    static void push(Access& A, dom::Object const& obj);
};

// Uses ES6 Proxy
struct ObjectProxy : ObjectBase
{
    static void push(Access& A, dom::Object const& obj);
};

#if 1
using ArrayRep  = ArrayProxy; //ArrayGetSet;
using ObjectRep = ObjectGetSet;
#else
using ArrayRep  = ArrayProxy;
using ObjectRep = ObjectProxy;
#endif

static void domValue_push(Access& A, dom::Value const& value);

//------------------------------------------------

dom::Array*
ArrayBase::
get(Access& A, duk_idx_t idx)
{
    duk_get_prop_string(A, idx, DUK_HIDDEN_SYMBOL("dom"));
    void* data;
    switch(duk_get_type(A, -1))
    {
    case DUK_TYPE_POINTER:
        data = duk_get_pointer(A, -1);
        break;
    case DUK_TYPE_BUFFER:
        data = duk_get_buffer_data(A, -1, nullptr);
        break;
    case DUK_TYPE_UNDEFINED:
        return nullptr;
    default:
        MRDOX_UNREACHABLE();
    }
    duk_pop(A);
    return static_cast<dom::Array*>(data);
}

void
ArrayGetSet::
push(
    Access& A, dom::Array const& obj)
{
}

void
ArrayProxy::
push(
    Access& A, dom::Array const& arr)
{
    duk_push_array(A);
    auto& arr_ = *static_cast<dom::Array*>(
        duk_push_fixed_buffer(A, sizeof(dom::Array)));
    dukM_put_prop_string(A, -2, DUK_HIDDEN_SYMBOL("dom"));

    // Effects:     ~ArrayPtr
    // Signature    ()
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        Access A(ctx);
        duk_push_this(ctx);
        std::destroy_at(get(A, -1));
        return 0;
    }, 0);
    duk_set_finalizer(A, -2);
    std::construct_at(&arr_, arr);

    // Proxy

    duk_push_object(A);

    // store a pointer so we can
    // get to it from the proxy.
    duk_push_pointer(A, &arr_);
    dukM_put_prop_string(A, -2, DUK_HIDDEN_SYMBOL("dom"));

    // Trap:        [[HasProperty]]
    // Effects:     return a[i] != undefined
    // Signature:   (a, i)
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        Access A(ctx);
        duk_push_this(A);
        auto& arr = *get(A, -1);
        auto i = duk_to_number(A, 1);
        duk_push_boolean(A, i < arr.size());
        return 1;
    }, 2);
    dukM_put_prop_string(A, -2, "has");

    // Trap:        [[Get]]
    // Effects:     return a[i]
    // Signature:   (a, i)
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        Access A(ctx);
        duk_push_this(A);
        auto& arr = *get(A, -1);
        switch(duk_get_type(A, 1))
        {
        case DUK_TYPE_NUMBER:
        {
            auto i = duk_get_int(A, 1);
            duk_pop_n(A, duk_get_top(A));
            if(i < static_cast<duk_int_t>(arr.size()))
                domValue_push(A, arr.at(i));
            else
                duk_push_undefined(A);
            break;
        }
        case DUK_TYPE_STRING:
        {
            auto prop = dukM_get_string(A, 1);
            if(prop == "length")
            {
                duk_pop_n(A, duk_get_top(A));
                duk_push_number(A, arr.size());
            }
            else
            {
                duk_pop_n(A, duk_get_top(A));
                duk_push_undefined(A);
            }
            break;
        }
        default:
            MRDOX_UNREACHABLE();
        }
        return 1;
    }, 2);
    dukM_put_prop_string(A, -2, "get");

#if 1
    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "ownKeys");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "enumerate");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "deleteProperty");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "apply");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "defineProperty");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "getPrototypeOf");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "setPrototypeOf");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "isExtensible");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "preventExtensions");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "construct");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "getOwnPropertyDescriptor");
#endif

    duk_push_proxy(A, 0);
}

//------------------------------------------------

dom::Object*
ObjectBase::
get(Access& A, duk_idx_t idx)
{
    duk_get_prop_string(A, idx, DUK_HIDDEN_SYMBOL("dom"));
    void* data;
    switch(duk_get_type(A, -1))
    {
    case DUK_TYPE_POINTER:
        data = duk_get_pointer(A, -1);
        break;
    case DUK_TYPE_BUFFER:
        data = duk_get_buffer_data(A, -1, nullptr);
        break;
    case DUK_TYPE_UNDEFINED:
        return nullptr;
    default:
        MRDOX_UNREACHABLE();
    }
    duk_pop(A);
    return static_cast<dom::Object*>(data);
}

void
ObjectGetSet::
push(
    Access& A, dom::Object const& obj)
{
    duk_push_object(A);
    auto idx = duk_normalize_index(A, -1);
    auto& obj_ = *static_cast<dom::Object*>(
        duk_push_fixed_buffer(A, sizeof(dom::Object)));
    dukM_put_prop_string(A, idx, DUK_HIDDEN_SYMBOL("dom"));

    // Effects:     ~ObjectPtr
    // Signature    ()
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        Access A(ctx);
        duk_push_this(ctx);
        std::destroy_at(get(A, -1));
        return 0;
    }, 0);
    duk_set_finalizer(A, idx);
    std::construct_at(&obj_, obj);

    for(auto const& kv : obj)
    {
        dukM_push_string(A, kv.key);

        // Method:      Getter
        // Effects:     return obj[key]
        // Signature:   (key)
        duk_push_c_function(A,
        [](duk_context* ctx) -> duk_ret_t
        {
            Access A(ctx);
            auto key = dukM_get_string(A, 0);
            duk_push_this(A);
            // get obj by value because
            // duk_pop_n would invalidate it
            dom::Object obj = *get(A, 1);
            duk_pop_n(A, duk_get_top(A));
            domValue_push(A, obj.find(key));
            return 1;
        }, 1);
        duk_def_prop(A, idx,
            DUK_DEFPROP_HAVE_GETTER |
            DUK_DEFPROP_SET_ENUMERABLE);
    }
}

void
ObjectProxy::
push(
    Access& A, dom::Object const& obj)
{
    duk_push_object(A);
    auto& obj_ = *static_cast<dom::Object*>(
        duk_push_fixed_buffer(A, sizeof(dom::Object)));
    dukM_put_prop_string(A, -2, DUK_HIDDEN_SYMBOL("dom"));

    // Effects:     ~ObjectPtr
    // Signature    ()
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        Access A(ctx);
        duk_push_this(ctx);
        std::destroy_at(get(A, -1));
        return 0;
    }, 0);
    duk_set_finalizer(A, -2);
    std::construct_at(&obj_, obj);

    // Proxy

    duk_push_object(A);

    // store a pointer so we can
    // get to it from the proxy.
    duk_push_pointer(A, &obj_);
    dukM_put_prop_string(A, -2, DUK_HIDDEN_SYMBOL("dom"));

    // Trap:        [[Get]]
    // Effects:     return target[prop]
    // Signature:   (target, prop, receiver)
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        Access A(ctx);
        duk_push_this(A); // the proxy
        auto& obj = *get(A, -1);
        auto key = dukM_get_string(A, 1);
        auto const& v = obj.find(key);
        duk_pop_n(A, duk_get_top(A));
        domValue_push(A, v);
        return 1;
    }, 3);
    dukM_put_prop_string(A, -2, "get");

    // Trap:        [[HasProperty]]
    // Effects:     return t[k] != null
    // Signature:   (t, k, r)
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        Access A(ctx);
        duk_push_this(A);
        auto& obj = *get(A, -1);
        auto key = dukM_get_string(A, 1);
        auto const& v = obj.find(key);
        duk_pop_n(A, duk_get_top(A));
        // VFALCO should add dom::Object::exists(k) for this
        duk_push_boolean(A, ! v.isNull());
        return 1;
    }, 3);
    dukM_put_prop_string(A, -2, "has");

#if 1
    // Trap:        [[OwnPropertyKeys]]
    // Effects:     return range(Object())
    // Signature:   ()
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        Access A(ctx);
        duk_push_this(A);
        auto& obj = *get(A, -1);
        duk_pop(A);
        duk_push_array(A);
        for(auto const& kv : obj)
        {
            dukM_push_string(A, kv.key);
            domValue_push(A, kv.value);
            duk_put_prop(A, -3);
        }
        return 1;
    }, 0);
    dukM_put_prop_string(A, -2, "ownKeys");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "enumerate");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "deleteProperty");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "apply");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "defineProperty");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "getPrototypeOf");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "setPrototypeOf");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "isExtensible");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "preventExtensions");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "construct");

    duk_push_c_function(A, [](duk_context* ctx) -> duk_ret_t {
        return 0;
    }, 0); dukM_put_prop_string(A, -2, "getOwnPropertyDescriptor");
#endif

    duk_push_proxy(A, 0);
}

//------------------------------------------------
//
// Value
//
//------------------------------------------------

// return a dom::Value from a stack element
static
dom::Value
domValue_get(
    Scope& scope, duk_idx_t idx)
{
    Access A(scope);
    idx = duk_require_normalize_index(A, idx);
    switch(duk_get_type(A, idx))
    {
    case DUK_TYPE_UNDEFINED:
    case DUK_TYPE_NULL:
        return nullptr;
    case DUK_TYPE_BOOLEAN:
        return duk_get_boolean(A, idx);
    case DUK_TYPE_NUMBER:
        return duk_get_number(A, idx);
    case DUK_TYPE_STRING:
        return dukM_get_string(A, idx);
    case DUK_TYPE_OBJECT:
    {
        return nullptr;
    }
    case DUK_TYPE_BUFFER:
    case DUK_TYPE_POINTER:
    case DUK_TYPE_LIGHTFUNC:
    default:
        MRDOX_UNREACHABLE();
    }
    A.addref(scope);
}

static
void
domValue_push(
    Access& A, dom::Value const& value)
{
    switch(value.kind())
    {
    case dom::Kind::Null:
        duk_push_null(A);
        return;
    case dom::Kind::Undefined:
        duk_push_undefined(A);
        return;
    case dom::Kind::Boolean:
        duk_push_boolean(A, value.getBool());
        return;
    case dom::Kind::Integer:
        duk_push_int(A, static_cast<
            duk_int_t>(value.getInteger()));
        return;
    case dom::Kind::String:
    case dom::Kind::SafeString:
        dukM_push_string(A, value.getString());
        return;
    case dom::Kind::Array:
        ArrayRep::push(A, value.getArray());
        return;
    case dom::Kind::Object:
        ObjectRep::push(A, value.getObject());
        return;
    default:
        MRDOX_UNREACHABLE();
    }
}

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

std::string
Value::
getString() const
{
    Access A(*scope_);
    return std::string(
        dukM_get_string(A, idx_));
}

void
Value::
setlog()
{
    Access A(*scope_);
    // Effects:
    // Signature    (level, message)
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        Access A(ctx);
        auto level = duk_get_uint(ctx, 0);
        std::string s(dukM_get_string(A, 1));
        report::print(report::getLevel(level), s);
        return 0;
    }, 2);
    dukM_put_prop_string(A, idx_, "log");
}

Expected<Value>
Value::
callImpl(
    std::initializer_list<dom::Value> args) const
{
    Access A(*scope_);
    duk_dup(A, idx_);
    for(auto const& arg : args)
        domValue_push(A, arg);
    auto result = duk_pcall(A, args.size());
    if(result == DUK_EXEC_ERROR)
        return dukM_popError(*scope_);
    return A.construct<Value>(-1, *scope_);
}

Expected<Value>
Value::
callPropImpl(
    std::string_view prop,
    std::initializer_list<dom::Value> args) const
{
    Access A(*scope_);
    if(! duk_get_prop_lstring(A,
            idx_, prop.data(), prop.size()))
        return formatError("method {} not found", prop);
    duk_dup(A, idx_);
    for(auto const& arg : args)
        domValue_push(A, arg);
    auto rc = duk_pcall_method(A, args.size());
    if(rc == DUK_EXEC_ERROR)
    {
        Error err = dukM_popError(*scope_);
        duk_pop(A); // method
        return err;
    }
    return A.construct<Value>(-1, *scope_);
}

} // js
} // mrdox
} // clang
