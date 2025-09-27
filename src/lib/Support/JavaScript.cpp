//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/Report.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Handlebars.hpp>
#include <mrdocs/Support/JavaScript.hpp>
#include <llvm/Support/raw_ostream.h>
#include <duktape.h>
#include <format>
#include <utility>
#include <variant>


namespace mrdocs {
namespace js {

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

/* Access to the underlying duktape context in
   Context and Scope.
 */
struct Access
{
    duk_context* ctx_ = nullptr;
    Context::Impl* impl_ = nullptr;

    // Access from an original duktape context
    explicit Access(duk_context* ctx) noexcept
        : ctx_(ctx)
    {
    }

    // Access from a Context
    explicit Access(Context const& ctx) noexcept
        : ctx_(ctx.impl_->ctx)
        , impl_(ctx.impl_)
    {
    }

    // Access from a Scope
    explicit Access(Scope const& scope) noexcept
        : Access(scope.ctx_)
    {
    }

    // Implicit conversion to a duktape context
    // for use with the duktape C API
    operator duk_context*() const noexcept
    {
        return ctx_;
    }

    // Access to a value idx in its Scope
    static duk_idx_t idx(Value const& value) noexcept
    {
        return value.idx_;
    }

    // Mark a scope as referenced by another
    // scope or Value
    // This is used to keep the scope alive
    // while it is being used and to
    // destroy it when it is no longer needed
    static void addref(Scope& scope) noexcept
    {
        ++scope.refs_;
    }

    // Mark a scope as referenced by one less
    // scope or Value
    // This is used to keep the scope alive
    // while it is being used and to
    // destroy it when it is no longer needed
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
    MRDOCS_ASSERT(duk_get_type(A, idx) == DUK_TYPE_STRING);
    duk_size_t size;
    char const* const data =
        duk_get_lstring(A, idx, &size);
    return {data, size};
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
    std::string_view name, Access const& A)
{
    MRDOCS_ASSERT(duk_get_type(A, -1) == DUK_TYPE_OBJECT);
    if(! duk_get_prop_lstring(A, -1, name.data(), name.size()))
        formatError("missing property {}", name).Throw();
    char const* s;
    if(duk_get_type(A, -1) != DUK_TYPE_STRING)
        duk_to_string(A, -1);
    duk_size_t len;
    s = duk_get_lstring(A, -1, &len);
    MRDOCS_ASSERT(s);
    std::string result = std::string(s, len);
    duk_pop(A);
    return result;
}

// return an Error from a JavaScript Error on the stack
static
Error
dukM_popError(Access const& A)
{
    auto err = formatError(
        "{} (\"{}\" line {})",
            dukM_get_prop_string("message", A),
            dukM_get_prop_string("fileName", A),
            dukM_get_prop_string("lineNumber", A));
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
    MRDOCS_ASSERT(refs_ == 0);
    reset();
}

Expected<void>
Scope::
script(
    std::string_view jsCode)
{
    Access A(*this);
    duk_int_t failed = duk_peval_lstring(
        A, jsCode.data(), jsCode.size());
    if (failed)
    {
        return Unexpected(dukM_popError(A));
    }
    // pop implicit expression result from the stack
    duk_pop(A);
    return {};
}

Expected<Value>
Scope::
eval(
    std::string_view jsCode)
{
    Access A(*this);
    duk_int_t failed = duk_peval_lstring(
        A, jsCode.data(), jsCode.size());
    if (failed)
    {
        return Unexpected(dukM_popError(A));
    }
    return Access::construct<Value>(duk_get_top_index(A), *this);
}

Expected<Value>
Scope::
compile_script(
    std::string_view jsCode)
{
    Access A(*this);
    duk_int_t failed = duk_pcompile_lstring(
        A, 0, jsCode.data(), jsCode.size());
    if (failed)
    {
        return Unexpected(dukM_popError(A));
    }
    return Access::construct<Value>(-1, *this);
}

Expected<Value>
Scope::
compile_function(
    std::string_view jsCode)
{
    Access A(*this);
    duk_int_t failed = duk_pcompile_lstring(
        A, DUK_COMPILE_FUNCTION, jsCode.data(), jsCode.size());
    if (failed)
    {
        return Unexpected(dukM_popError(A));
    }
    return Access::construct<Value>(-1, *this);
}

Value
Scope::
getGlobalObject()
{
    Access A(*this);
    duk_push_global_object(A);
    return Access::construct<Value>(-1, *this);
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
        return Unexpected(formatError("global property {} not found", name));
    }
    return Access::construct<Value>(duk_get_top_index(A), *this);
}

void
Scope::
setGlobal(
    std::string_view name, dom::Value const& value)
{
    this->getGlobalObject().set(name, value);
}

Value
Scope::
pushInteger(std::int64_t value)
{
    Access A(*this);
    duk_push_int(A, value);
    return Access::construct<Value>(-1, *this);
}

Value
Scope::
pushDouble(double value)
{
    Access A(*this);
    duk_push_number(A, value);
    return Access::construct<Value>(-1, *this);
}

Value
Scope::
pushBoolean(bool value)
{
    Access A(*this);
    duk_push_boolean(A, value);
    return Access::construct<Value>(-1, *this);
}

Value
Scope::
pushString(std::string_view value)
{
    Access A(*this);
    duk_push_lstring(A, value.data(), value.size());
    return Access::construct<Value>(-1, *this);
}

Value
Scope::
pushObject()
{
    Access A(*this);
    duk_push_object(A);
    return Access::construct<Value>(-1, *this);
}

Value
Scope::
pushArray()
{
    Access A(*this);
    duk_push_array(A);
    return Access::construct<Value>(-1, *this);
}

//------------------------------------------------
//
// JS -> C++ dom::Value bindings
//
//------------------------------------------------

namespace {

class JSObjectImpl : public dom::ObjectImpl
{
    Access A_;
    duk_idx_t idx_;
    std::shared_ptr<Scope> scope_;

public:
    ~JSObjectImpl() override
    {
        if (scope_)
        {
            Access::release(*scope_);
        }
    }

    JSObjectImpl(
        Scope& scope, duk_idx_t idx) noexcept
        : A_(scope)
        , idx_(idx)
    {
        MRDOCS_ASSERT(duk_is_object(A_, idx_));
    }

    JSObjectImpl(
        Access& A, duk_idx_t idx) noexcept
        : A_(A)
        , idx_(idx)
    {
        MRDOCS_ASSERT(duk_is_object(A_, idx_));
    }

    char const* type_key() const noexcept override
    {
        return "JSObject";
    }

    // Get an object property as a dom::Value
    dom::Value get(std::string_view key) const override;

    // Set an object enumerable property
    void set(dom::String key, dom::Value value) override;

    // Visit all enumerable properties
    bool visit(std::function<bool(dom::String, dom::Value)> visitor) const override;

    // Get number of enumerable properties in the object
    std::size_t size() const override;

    // Check if object contains the property
    bool exists(std::string_view key) const override;

    Access const&
    access() const noexcept
    {
        return A_;
    }

    duk_idx_t
    idx() const noexcept
    {
        return idx_;
    }

    // Set a shared pointer to the Scope so that it
    // can temporarily outlive the variable
    void
    setScope(std::shared_ptr<Scope> scope) noexcept
    {
        MRDOCS_ASSERT(scope);
        MRDOCS_ASSERT(Access(*scope.get()).ctx_ == A_.ctx_);
        scope_ = std::move(scope);
        Access::addref(*scope_);
    }
};

class JSArrayImpl : public dom::ArrayImpl
{
    Access A_;
    duk_idx_t idx_;
    std::shared_ptr<Scope> scope_;

public:
    ~JSArrayImpl() override
    {
        if (scope_)
        {
            Access::release(*scope_);
        }
    }

    JSArrayImpl(
        Scope& scope, duk_idx_t idx) noexcept
        : A_(scope)
        , idx_(idx)
    {
        MRDOCS_ASSERT(duk_is_array(A_, idx_));
    }

    JSArrayImpl(
        Access& A, duk_idx_t idx) noexcept
        : A_(A)
        , idx_(idx)
    {
        MRDOCS_ASSERT(duk_is_array(A_, idx_));
    }

    char const* type_key() const noexcept override
    {
        return "JSArray";
    }

    // Get an array value as a dom::Value
    value_type get(size_type i) const override;

    // Set an array value
    void set(size_type, dom::Value) override;

    // Push a value onto the array
    void emplace_back(dom::Value value) override;

    // Get number of enumerable properties in the object
    size_type size() const override;

    Access const&
    access() const noexcept
    {
        return A_;
    }

    duk_idx_t
    idx() const noexcept
    {
        return idx_;
    }

    // Set a shared pointer to the Scope so that it
    // can temporarily outlive the variable
    void
    setScope(std::shared_ptr<Scope> scope) noexcept
    {
        MRDOCS_ASSERT(scope);
        MRDOCS_ASSERT(Access(*scope.get()).ctx_ == A_.ctx_);
        scope_ = std::move(scope);
        Access::addref(*scope_);
    }
};

// A JavaScript function defined in the scope as a dom::Function
class JSFunctionImpl : public dom::FunctionImpl
{
    Access A_;
    duk_idx_t idx_;
    std::shared_ptr<Scope> scope_;

public:
    ~JSFunctionImpl() override
    {
        if (scope_)
        {
            Access::release(*scope_);
        }
    }

    JSFunctionImpl(
        Scope& scope, duk_idx_t idx) noexcept
        : A_(scope)
        , idx_(idx)
    {
        MRDOCS_ASSERT(duk_is_function(A_, idx_));
    }

    JSFunctionImpl(
        Access& A, duk_idx_t idx) noexcept
        : A_(A)
        , idx_(idx)
    {
        MRDOCS_ASSERT(duk_is_function(A_, idx_));
    }

    char const* type_key() const noexcept override
    {
        return "JSFunction";
    }

    Expected<dom::Value> call(dom::Array const& args) const override;

    Access const&
    access() const noexcept
    {
        return A_;
    }

    duk_idx_t
    idx() const noexcept
    {
        return idx_;
    }

    // Set a shared pointer to the Scope so that it
    // can temporarily outlive the variable
    void
    setScope(std::shared_ptr<Scope> scope) noexcept
    {
        MRDOCS_ASSERT(scope);
        MRDOCS_ASSERT(Access(*scope.get()).ctx_ == A_.ctx_);
        scope_ = std::move(scope);
        Access::addref(*scope_);
    }
};

} // (anon)

//------------------------------------------------
//
// C++ dom::Value -> JS bindings
//
//------------------------------------------------

template <class T>
T*
domHiddenGet(
    duk_context* ctx, duk_idx_t idx)
{
    // ... [idx target] ... -> ... [idx target] ... [buffer]
    duk_get_prop_string(ctx, idx, DUK_HIDDEN_SYMBOL("dom"));
    // ... [idx target] ... [buffer]
    void* data;
    switch(duk_get_type(ctx, -1))
    {
    case DUK_TYPE_POINTER:
        data = duk_get_pointer(ctx, -1);
        break;
    case DUK_TYPE_BUFFER:
        data = duk_get_buffer_data(ctx, -1, nullptr);
        break;
    default:
        return nullptr;
    }
    // ... [idx target] ... [buffer] -> ... [idx target] ...
    duk_pop(ctx);
    return static_cast<T*>(data);
}

static
dom::Value
domValue_get(Access& A, duk_idx_t idx);

static
void
domValue_push(
    Access& A, dom::Value const& value);

void
domFunction_push(
    Access& A, dom::Function const& fn)
{
    dom::FunctionImpl* ptr = fn.impl().get();
    auto impl = dynamic_cast<JSFunctionImpl*>(ptr);

    // Underlying function is also a JS function
    if (impl && A.ctx_ == impl->access().ctx_)
    {
        duk_dup(A, impl->idx());
        return;
    }

    // Underlying function is a C++ function pointer
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        Access A(ctx);

        // Get the original function from
        // the JS function's hidden property
        duk_push_current_function(ctx);
        auto* fn = domHiddenGet<dom::Function>(ctx, -1);
        duk_pop(ctx);

        // Construct an array of dom::Value from the
        // duktape argments
        dom::Array args;
        duk_idx_t n = duk_get_top(ctx);
        for (duk_idx_t i = 0; i < n; ++i)
        {
            args.push_back(domValue_get(A, i));
        }

        // Call the dom::Function
        auto exp = fn->call(args);
        if (!exp)
        {
            dukM_push_string(A, exp.error().message());
            return duk_throw(ctx);
        }
        dom::Value result = exp.value();

        // Push the result onto the stack
        domValue_push(A, result);
        return 1;
    }, duk_get_top(A));

    // Create a buffer to store the dom::Function in the
    // JS function's hidden property
    // [...] [fn] [buf]
    void* data = duk_push_fixed_buffer(A, sizeof(dom::Function));
    // [...] [fn] [buf] -> [fn]
    dukM_put_prop_string(A, -2, DUK_HIDDEN_SYMBOL("dom"));

    // Create a function finalizer to destroy the dom::Function
    // from the buffer whenever the JS function is garbage
    // collected
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // Push the function buffer to the stack
        // The object being finalized is the first argument
        auto* fn = domHiddenGet<dom::Function>(ctx, 0);
        // Destroy the dom::Function stored at data
        std::destroy_at(fn);
        return 0;
    }, 1);
    duk_set_finalizer(A, -2);

    // Construct the dom::Function in the buffer
    auto data_ptr = static_cast<dom::Function*>(data);
    std::construct_at(data_ptr, fn);
}

void
domObject_push(
    Access& A, dom::Object const& obj)
{
    dom::ObjectImpl* ptr = obj.impl().get();
    auto impl = dynamic_cast<JSObjectImpl*>(ptr);

    // Underlying function is also a JS function
    if (impl && A.ctx_ == impl->access().ctx_)
    {
        duk_dup(A, impl->idx());
        return;
    }

    // Underlying object is a C++ dom::Object
    // https://wiki.duktape.org/howtovirtualproperties#ecmascript-e6-proxy-subset
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Proxy
    // ... [target]
    duk_push_object(A);
    // ... [target] [buffer]
    void* data = duk_push_fixed_buffer(A, sizeof(dom::Object));
    // ... [target] [buffer] -> [target]
    dukM_put_prop_string(A, -2, DUK_HIDDEN_SYMBOL("dom"));
    // Create a function finalizer to destroy the dom::Object
    // from the buffer whenever the JS object is garbage
    // collected
    // ... [target] [finalizer]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // Destroy the dom::Object stored at data
        auto* obj = domHiddenGet<dom::Object>(ctx, 0);
        std::destroy_at(obj);
        return 0;
    }, 1);
    // ... [target] [finalizer] -> ... [target]
    duk_set_finalizer(A, -2);

    // Construct the dom::Object in the buffer
    auto data_ptr = static_cast<dom::Object*>(data);
    std::construct_at(data_ptr, obj);

    // Create a Proxy handler object
    // ... [target] [handler]
    duk_push_object(A);

    // Store a pointer to the dom::Object also in
    // the handler, so it knows where to find
    // the dom::Object
    // ... [target] [handler] [dom::Object*]
    duk_push_pointer(A, data_ptr);
    // ... [target] [handler] [dom::Object*] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, DUK_HIDDEN_SYMBOL("dom"));

    // ... [target] [handler] -> ... [target] [handler] [get]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // [target] [key] [recv]
        Access A(ctx);
        auto* obj = domHiddenGet<dom::Object>(ctx, 0);
        std::string_view key = dukM_get_string(A, 1);
        dom::Value value = obj->get(key);
        domValue_push(A, value);
        return 1;
    }, 3);
    // ... [target] [handler] [get] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, "get");

    // ... [target] [handler] -> ... [target] [handler] [has]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // [target] [key]
        Access A(ctx);
        auto* obj = domHiddenGet<dom::Object>(ctx, 0);
        std::string_view key = dukM_get_string(A, 1);
        bool value = obj->exists(key);
        duk_push_boolean(A, value);
        return 1;
    }, 2);
    // ... [target] [handler] [has] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, "has");

    // ... [target] [handler] -> ... [target] [handler] [set]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // [target] [key] [value] [recv]
        Access A(ctx);
        auto* obj = domHiddenGet<dom::Object>(ctx, 0);
        std::string_view key = dukM_get_string(A, 1);
        dom::Value value = domValue_get(A, 2);
        obj->set(key, value);
        duk_push_boolean(A, true);
        return 1;
    }, 4);
    // ... [target] [handler] [set] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, "set");

    // ... [target] [handler] -> ... [target] [handler] [ownKeys]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // [target]
        Access A(ctx);
        auto* obj = domHiddenGet<dom::Object>(ctx, 0);
        // Get the object keys
        duk_uarridx_t i = 0;
        // [target] -> [target] [array]
        duk_idx_t arr_idx = duk_push_array(ctx);
        obj->visit([&](dom::String const& key, dom::Value const&)
        {
            // [target] [array] -> [target] [array] [key]
            dukM_push_string(A, key);
            // [target] [array] [key] -> [target] [array]
            duk_put_prop_index(A, arr_idx, i++);
        });
        return 1;
    }, 1);
    // ... [target] [handler] [ownKeys] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, "ownKeys");

    // ... [target] [handler] -> ... [target] [handler] [deleteProperty]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // [target] [key]
        Access A(ctx);
        auto* obj = domHiddenGet<dom::Object>(ctx, 0);
        std::string_view key = dukM_get_string(A, 1);
        bool const exists = obj->exists(key);
        if (exists)
        {
            obj->set(key, dom::Value(dom::Kind::Undefined));
        }
        duk_push_boolean(A, exists);
        return 1;
    }, 2);
    // ... [target] [handler] [deleteProperty] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, "deleteProperty");

    // ... [target] [handler] -> ... [proxy]
    duk_push_proxy(A, 0);
}

/* Get a value in the stack as an index

   If the value is a number, it is returned as an index.
   If the value is a string, it is parsed as a number and
   returned as an index.
   If the value is a string, and it cannot be parsed as a
   number, the original string is returned.
   If the value is not a number or a string, an empty
   string is returned.

*/
std::variant<std::size_t, std::string_view>
domM_get_index(
    duk_context* ctx, duk_idx_t idx)
{
    switch (duk_get_type(ctx, idx))
    {
        case DUK_TYPE_NUMBER:
        {
            duk_int_t i = duk_get_int(ctx, idx);
            return static_cast<std::size_t>(i);
        }
        case DUK_TYPE_STRING:
        {
            std::string_view key = duk_get_string(ctx, idx);
            std::size_t i;
            auto res = std::from_chars(
                key.data(), key.data() + key.size(), i);
            if (res.ec != std::errc())
            {
                return key;
            }
            return i;
        }
        default:
        {
            return std::string_view();
        }
    }
}

void
domArray_push(
    Access& A, dom::Array const& arr)
{
    dom::ArrayImpl* ptr = arr.impl().get();
    auto impl = dynamic_cast<JSArrayImpl*>(ptr);

    // Underlying function is also a JS function
    if (impl && A.ctx_ == impl->access().ctx_)
    {
        duk_dup(A, impl->idx());
        return;
    }

    // Underlying object is a C++ dom::Array
    // https://wiki.duktape.org/howtovirtualproperties#ecmascript-e6-proxy-subset
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Proxy
    // ... [target]
    duk_push_array(A);
    // ... [target] [buffer]
    void* data = duk_push_fixed_buffer(A, sizeof(dom::Array));
    // ... [target] [buffer] -> [target]
    dukM_put_prop_string(A, -2, DUK_HIDDEN_SYMBOL("dom"));
    // Create a function finalizer to destroy the dom::Array
    // from the buffer whenever the JS array is garbage
    // collected
    // ... [target] [finalizer]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // Destroy the dom::Array stored at data
        auto* arr = domHiddenGet<dom::Array>(ctx, 0);
        std::destroy_at(arr);
        return 0;
    }, 1);
    // ... [target] [finalizer] -> ... [target]
    duk_set_finalizer(A, -2);

    // Construct the dom::Array in the buffer
    auto data_ptr = static_cast<dom::Array*>(data);
    std::construct_at(data_ptr, arr);

    // Create a Proxy handler object
    // ... [target] [handler]
    duk_push_object(A);

    // Store a pointer to the dom::Array also in
    // the handler, so it knows where to find
    // the dom::Array
    // ... [target] [handler] [dom::Array*]
    duk_push_pointer(A, data_ptr);
    // ... [target] [handler] [dom::Array*] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, DUK_HIDDEN_SYMBOL("dom"));

    // ... [target] [handler] -> ... [target] [handler] [get]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // [target] [key] [recv]
        Access A(ctx);
        auto* arr = domHiddenGet<dom::Array>(ctx, 0);
        auto key = domM_get_index(ctx, 1);
        if (std::holds_alternative<std::string_view>(key))
        {
            std::string_view key_str = std::get<std::string_view>(key);
            if (key_str == "length")
            {
                duk_push_number(
                    A, static_cast<double>(arr->size()));
            }
            else
            {
                duk_push_undefined(A);
            }
            return 1;
        }
        std::size_t key_idx = std::get<std::size_t>(key);
        dom::Value value = arr->get(key_idx);
        domValue_push(A, value);
        return 1;
    }, 3);
    // ... [target] [handler] [get] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, "get");

    // ... [target] [handler] -> ... [target] [handler] [has]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // [target] [key]
        Access A(ctx);
        auto* arr = domHiddenGet<dom::Array>(ctx, 0);
        auto key = domM_get_index(ctx, 1);
        if (std::holds_alternative<std::string_view>(key))
        {
            std::string_view key_str = std::get<std::string_view>(key);
            duk_push_boolean(A, key_str == "length");
            return 1;
        }
        std::size_t key_idx = std::get<std::size_t>(key);
        bool result = key_idx < arr->size();
        duk_push_boolean(A, result);
        return 1;
    }, 2);
    // ... [target] [handler] [has] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, "has");

    // ... [target] [handler] -> ... [target] [handler] [set]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // [target] [key] [value] [recv]
        Access A(ctx);
        auto* arr = domHiddenGet<dom::Array>(ctx, 0);
        auto key = domM_get_index(ctx, 1);
        if (std::holds_alternative<std::string_view>(key))
        {
            duk_push_boolean(A, false);
            return 1;
        }
        std::size_t key_idx = std::get<std::size_t>(key);
        dom::Value value = domValue_get(A, 2);
        if (key_idx < arr->size())
        {
            arr->set(key_idx, value);
        }
        else
        {
            std::size_t diff = key_idx - arr->size();
            for (std::size_t i = 0; i < diff; ++i)
            {
                arr->emplace_back(dom::Kind::Undefined);
            }
            arr->emplace_back(value);
        }
        duk_push_boolean(A, false);
        return 1;
    }, 4);
    // ... [target] [handler] [set] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, "set");

    // ... [target] [handler] -> ... [target] [handler] [ownKeys]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // [target]
        Access A(ctx);
        auto* arr = domHiddenGet<dom::Array>(ctx, 0);
        // Get the array keys (list of indices)
        // [target] -> [target] [array]
        duk_idx_t arr_idx = duk_push_array(ctx);
        for (std::size_t i = 0; i < arr->size(); ++i)
        {
            // [target] [array] -> [target] [array] [key]
            std::string key = std::to_string(i);
            dukM_push_string(A, key);
            // [target] [array] [key] -> [target] [array]
            duk_put_prop_index(ctx, arr_idx, i);
        }
        return 1;
    }, 1);
    // ... [target] [handler] [ownKeys] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, "ownKeys");

    // ... [target] [handler] -> ... [target] [handler] [deleteProperty]
    duk_push_c_function(A,
    [](duk_context* ctx) -> duk_ret_t
    {
        // [target] [key]
        Access A(ctx);
        auto* arr = domHiddenGet<dom::Array>(ctx, 0);
        auto key = domM_get_index(ctx, 1);
        if (std::holds_alternative<std::string_view>(key))
        {
            duk_push_boolean(A, false);
            return 1;
        }
        std::size_t key_idx = std::get<std::size_t>(key);
        if (key_idx < arr->size())
        {
            arr->set(key_idx, dom::Kind::Undefined);
            duk_push_boolean(ctx, true);
        }
        else
        {
            duk_push_boolean(ctx, false);
        }
        return 1;
    }, 2);
    // ... [target] [handler] [deleteProperty] -> ... [target] [handler]
    dukM_put_prop_string(A, -2, "deleteProperty");

    // ... [target] [handler] -> ... [proxy array]
    duk_push_proxy(A, 0);
}

// return a dom::Value from a stack element
static
dom::Value
domValue_get(
    Access& A, duk_idx_t idx)
{
    idx = duk_require_normalize_index(A, idx);
    switch (duk_get_type(A, idx))
    {
    case DUK_TYPE_UNDEFINED:
        return dom::Kind::Undefined;
    case DUK_TYPE_NULL:
        return nullptr;
    case DUK_TYPE_BOOLEAN:
        return static_cast<bool>(duk_get_boolean(A, idx));
    case DUK_TYPE_NUMBER:
        return duk_get_number(A, idx);
    case DUK_TYPE_STRING:
        return dukM_get_string(A, idx);
    case DUK_TYPE_OBJECT:
    {
        if (duk_is_array(A, idx))
        {
            duk_dup(A, idx);
            return {dom::newArray<JSArrayImpl>(A, duk_get_top_index(A))};
        }
        if (duk_is_function(A, idx))
        {
            duk_dup(A, idx);
            return {dom::newFunction<JSFunctionImpl>(A, duk_get_top_index(A))};
        }
        if (duk_is_object(A, idx))
        {
            duk_dup(A, idx);
            return {dom::newObject<JSObjectImpl>(A, duk_get_top_index(A))};
        }
        return nullptr;
    }
    default:
        return dom::Kind::Undefined;
    }
    return dom::Kind::Undefined;
}

// Push a dom::Value onto the JS stack
// Objects are pushed as proxies
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
        domArray_push(A, value.getArray());
        return;
    case dom::Kind::Object:
        domObject_push(A, value.getObject());
        return;
    case dom::Kind::Function:
        domFunction_push(A, value.getFunction());
        return;
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::Value
JSObjectImpl::
get(std::string_view key) const
{
    Access A(A_);
    MRDOCS_ASSERT(duk_is_object(A, idx_));
    // Put value on top of the stack
    duk_get_prop_lstring(A, idx_, key.data(), key.size());
    // Convert to dom::Value
    return domValue_get(A, -1);
}

// Set an object enumerable property
void
JSObjectImpl::
set(dom::String key, dom::Value value)
{
    Access A(A_);
    MRDOCS_ASSERT(duk_is_object(A, idx_));
    dukM_push_string(A, key);
    domValue_push(A, value);
    duk_put_prop(A, idx_);
}

// Visit all enumerable properties
bool
JSObjectImpl::
visit(std::function<bool(dom::String, dom::Value)> visitor) const
{
    Access A(A_);
    MRDOCS_ASSERT(duk_is_object(A, idx_));

    // Enumerate only the object's own properties
    // The enumerator is pushed on top of the stack
    duk_enum(A, idx_, DUK_ENUM_OWN_PROPERTIES_ONLY);

    // Iterates over each property of the object
    while (duk_next(A, -1, 1))
    {
        // key and value are on top of the stack
        dom::Value key = domValue_get(A, -2);
        dom::Value value = domValue_get(A, -1);
        if (!visitor(key.getString(), value)) {
            return false;
        }
        // Pop both key and value
        duk_pop_2(A);
    }

    // Pop the enum property
    duk_pop(A);
    return true;
}

// Get number of enumerable properties in the object
std::size_t
JSObjectImpl::
size() const
{
    MRDOCS_ASSERT(duk_is_object(A_, idx_));
    int numProperties = 0;

    // Create an enumerator for the object
    duk_enum(A_, idx_, DUK_ENUM_OWN_PROPERTIES_ONLY);

    while (duk_next(A_, -1, 0))
    {
        // Iterates each enumerable property of the object
        numProperties++;

        // Pop the key from the stack
        duk_pop(A_);
    }

    // Pop the enumerator from the stack
    duk_pop(A_);

    return numProperties;
}

// Check if object contains the property
bool
JSObjectImpl::
exists(std::string_view key) const
{
    MRDOCS_ASSERT(duk_is_object(A_, idx_));
    return duk_has_prop_lstring(A_, idx_, key.data(), key.size());
}


JSArrayImpl::value_type
JSArrayImpl::
get(size_type i) const
{
    Access A(A_);
    MRDOCS_ASSERT(duk_is_array(A, idx_));
    // Push result to top of the stack
    duk_get_prop_index(A, idx_, i);
    // Convert to dom::Value
    return domValue_get(A, -1);
}

void
JSArrayImpl::
set(size_type idx, dom::Value value)
{
    MRDOCS_ASSERT(duk_is_array(A_, idx_));
    // Push value to top of the stack
    domValue_push(A_, value);
    // Push to array
    duk_put_prop_index(A_, idx_, idx);
}

void
JSArrayImpl::
emplace_back(dom::Value value)
{
    MRDOCS_ASSERT(duk_is_array(A_, idx_));
    // Push value to top of the stack
    domValue_push(A_, value);
    // Push to array
    duk_put_prop_index(A_, idx_, duk_get_length(A_, idx_));
}

JSArrayImpl::size_type
JSArrayImpl::
size() const
{
    Access A(A_);
    auto t = duk_get_type(A, idx_);
    if (t != DUK_TYPE_OBJECT && scope_)
    {
        auto top = duk_get_top(A);
        MRDOCS_ASSERT(top > 0);
    }
    MRDOCS_ASSERT(t == DUK_TYPE_OBJECT);
    MRDOCS_ASSERT(duk_is_array(A, idx_));
    return duk_get_length(A, idx_);
}

Expected<dom::Value>
JSFunctionImpl::
call(dom::Array const& args) const
{
    Access A(A_);
    MRDOCS_ASSERT(duk_is_function(A, idx_));
    duk_dup(A, idx_);
    for (auto const& arg : args)
    {
        domValue_push(A, arg);
    }
    auto result = duk_pcall(A, static_cast<duk_idx_t>(args.size()));
    if(result == DUK_EXEC_ERROR)
    {
        return Unexpected(dukM_popError(A));
    }
    return domValue_get(A, -1);
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
    Access::addref(*scope_);
}

Value::
~Value()
{
    if( ! scope_)
        return;
    Access A(*scope_);
    if (idx_ == duk_get_top(A) - 1)
        duk_pop(A);
    Access::release(*scope_);
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
    Access::addref(*scope_);
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
    case DUK_TYPE_OBJECT:
    {
        if (duk_is_function(A, idx_))
            return Type::function;
        if (duk_is_array(A, idx_))
            return Type::array;
        return Type::object;
    }
    case DUK_TYPE_LIGHTFUNC:
        return Type::function;
    default:
        return Type::undefined;
    }
}

bool
Value::
isInteger() const noexcept
{
    if (isNumber())
    {
        Access A(*scope_);
        auto n = duk_get_number(A, idx_);
        return n == (double)(int)n;
    }
    return false;
}

bool
Value::
isDouble() const noexcept
{
    return isNumber() && !isInteger();
}

bool
Value::
isTruthy() const noexcept
{
    Access A(*scope_);
    switch(type())
    {
    using enum Type;
    case boolean:
        return getBool();
    case number:
        return getDouble() != 0;
    case string:
        return !getString().empty();
    case array:
    case object:
    case function:
        return true;
    case null:
    case undefined:
    default:
        return false;
    }
}

std::string_view
Value::
getString() const
{
    MRDOCS_ASSERT(isString());
    Access A(*scope_);
    return dukM_get_string(A, idx_);
}

bool
Value::
getBool() const noexcept
{
    MRDOCS_ASSERT(isBoolean());
    Access A(*scope_);
    return duk_get_boolean(A, idx_) != 0;
}

std::int64_t
Value::
getInteger() const noexcept
{
    MRDOCS_ASSERT(isNumber());
    Access A(*scope_);
    return duk_get_int(A, idx_);
}

double
Value::
getDouble() const noexcept
{
    MRDOCS_ASSERT(isNumber());
    Access A(*scope_);
    return duk_get_number(A, idx_);
}

dom::Object
Value::
getObject() const noexcept
{
    MRDOCS_ASSERT(isObject());
    return dom::newObject<JSObjectImpl>(*scope_, idx_);
}

dom::Array
Value::
getArray() const noexcept
{
    MRDOCS_ASSERT(isArray());
    return dom::newArray<JSArrayImpl>(*scope_, idx_);
}

dom::Function
Value::
getFunction() const noexcept
{
    MRDOCS_ASSERT(isFunction());
    return dom::newFunction<JSFunctionImpl>(*scope_, idx_);
}

dom::Value
js::Value::
getDom() const
{
    Access A(*scope_);
    return domValue_get(A, idx_);
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
    for (auto const& arg : args)
        domValue_push(A, arg);
    auto const n = static_cast<duk_int_t>(args.size());
    auto result = duk_pcall(A, n);
    if(result == DUK_EXEC_ERROR)
        return Unexpected(dukM_popError(A));
    return Access::construct<Value>(-1, *scope_);
}

Expected<Value>
Value::
callImpl(std::span<dom::Value> args) const
{
    Access A(*scope_);
    duk_dup(A, idx_);
    for (auto const& arg : args)
        domValue_push(A, arg);
    auto const n = static_cast<duk_int_t>(args.size());
    auto result = duk_pcall(A, n);
    if(result == DUK_EXEC_ERROR)
        return Unexpected(dukM_popError(A));
    return Access::construct<Value>(-1, *scope_);
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
        return Unexpected(formatError("method {} not found", prop));
    duk_dup(A, idx_);
    for(auto const& arg : args)
        domValue_push(A, arg);
    auto rc = duk_pcall_method(
        A, static_cast<duk_idx_t>(args.size()));
    if(rc == DUK_EXEC_ERROR)
    {
        Error err = dukM_popError(A);
        duk_pop(A); // method
        return Unexpected(err);
    }
    return Access::construct<Value>(-1, *scope_);
}

Value
Value::
get(std::string_view key) const
{
    Access A(*scope_);
    // Push the key for the value we want to retrieve
    duk_push_lstring(A, key.data(), key.size());
    // Get the value associated with the key
    duk_get_prop(A, idx_);
    // Return value or `undefined`
    return Access::construct<Value>(-1, *scope_);
}

Value
Value::
get(std::size_t i) const
{
    Access A(*scope_);
    duk_get_prop_index(A, idx_, i);
    return Access::construct<Value>(-1, *scope_);
}


Value
Value::
get(dom::Value const& i) const
{
    if (i.isInteger())
    {
        return get(static_cast<std::size_t>(i.getInteger()));
    }
    if (i.isString() || i.isSafeString())
    {
        return get(i.getString().get());
    }
    return {};
}

Value
Value::
lookup(std::string_view keys) const
{
    Value cur = *this;
    std::size_t pos = keys.find('.');
    std::string_view key = keys.substr(0, pos);
    while (pos != std::string_view::npos)
    {
        cur = cur.get(key);
        if (cur.isUndefined())
        {
            return cur;
        }
        keys = keys.substr(pos + 1);
        pos = keys.find('.');
        key = keys.substr(0, pos);
    }
    return cur.get(key);
}

void
Value::
set(
    std::string_view key,
    Value const& value) const
{
    Access A(*scope_);
    // Push the key and value onto the stack
    duk_push_lstring(A, key.data(), key.size());
    duk_dup(A, value.idx_);
    // Insert the key-value pair into the object
    duk_put_prop(A, idx_);
}

void
Value::
set(
    std::string_view key,
    dom::Value const& value) const
{
    Access A(*scope_);
    // Push the key and value onto the stack
    duk_push_lstring(A, key.data(), key.size());
    domValue_push(A, value);
    // Insert the key-value pair into the object
    duk_put_prop(A, idx_);
}

bool
Value::
exists(std::string_view key) const
{
    Access A(*scope_);
    duk_push_lstring(A, key.data(), key.size());
    return duk_has_prop(A, idx_);
}

bool
Value::
empty() const
{
    switch(type())
    {
    using enum Type;
    case undefined:
    case null:
        return true;
    case boolean:
    case number:
        return false;
    case string:
        return getString().empty();
    case array:
        return getArray().empty();
    case object:
        return getObject().empty();
    case function:
        return false;
    default:
        MRDOCS_UNREACHABLE();
    }
}

std::size_t
Value::
size() const
{
    switch(type())
    {
    using enum Type;
    case undefined:
    case null:
        return 0;
    case boolean:
    case number:
        return 1;
    case string:
        return getString().size();
    case array:
        return getArray().size();
    case object:
        return getObject().size();
    case function:
        return 1;
    default:
        MRDOCS_UNREACHABLE();
    }
}

void
Value::
swap(Value& other) noexcept
{
    std::swap(scope_, other.scope_);
    std::swap(idx_, other.idx_);
}

std::string
toString(
    Value const& value)
{
    Access A(*value.scope_);
    duk_dup(A, value.idx_);
    std::string s = duk_to_string(A, -1);
    duk_pop(A);
    return s;
}

bool
operator==(
    Value const& lhs,
    Value const& rhs) noexcept
{
    if (lhs.isUndefined() || rhs.isUndefined())
    {
        return lhs.isUndefined() && rhs.isUndefined();
    }
    return duk_strict_equals(
        Access(*lhs.scope_), lhs.idx_, rhs.idx_);
}

std::strong_ordering
operator<=>(Value const& lhs, Value const& rhs) noexcept
{
    using kind_t = std::underlying_type_t<Type>;
    if (static_cast<kind_t>(lhs.type()) < static_cast<kind_t>(rhs.type()))
    {
        return std::strong_ordering::less;
    }
    if (static_cast<kind_t>(rhs.type()) < static_cast<kind_t>(lhs.type()))
    {
        return std::strong_ordering::greater;
    }
    switch (lhs.type())
    {
    using enum Type;
    case undefined:
    case null:
        return std::strong_ordering::equivalent;
    case boolean:
        return lhs.getBool() <=> rhs.getBool();
    case number:
        if (lhs.getDouble() < rhs.getDouble())
        {
            return std::strong_ordering::less;
        }
        if (rhs.getDouble() < lhs.getDouble())
        {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    case string:
        return lhs.getString() <=> rhs.getString();
    default:
        if (duk_strict_equals(
            Access(*lhs.scope_), lhs.idx_, rhs.idx_))
        {
            return std::strong_ordering::equal;
        }
        else
        {
            return std::strong_ordering::equivalent;
        }
    }
    return std::strong_ordering::equivalent;
}

Value
operator||(Value const& lhs, Value const& rhs)
{
    if (lhs.isTruthy())
    {
        return lhs;
    }
    return rhs;
}

Value
operator&&(Value const& lhs, Value const& rhs)
{
    if (!lhs.isTruthy())
    {
        return lhs;
    }
    return rhs;
}

Expected<void, Error>
registerHelper(
    mrdocs::Handlebars& hbs,
    std::string_view name,
    Context& ctx,
    std::string_view script)
{
    // Register the compiled helper function in the global scope
    constexpr auto global_helpers_key = DUK_HIDDEN_SYMBOL("MrDocsHelpers");
    {
        Scope s(ctx);
        Value g = s.getGlobalObject();
        MRDOCS_ASSERT(g.isObject());
        if (!g.exists(global_helpers_key))
        {
            Value obj = s.pushObject();
            MRDOCS_ASSERT(obj.isObject());
            g.set(global_helpers_key, obj);
        }
        Value helpers = g.get(global_helpers_key);
        MRDOCS_ASSERT(helpers.isObject());
        MRDOCS_TRY(Value JSFn, s.compile_function(script));
        if (!JSFn.isFunction())
        {
          return Unexpected(
              Error(std::format("helper \"{}\" is not a function", name)));
        }
        helpers.set(name, JSFn);
    }

    // Register C++ helper that retrieves the helper
    // from the global object, converts the arguments,
    // and invokes the JS function.
    hbs.registerHelper(name, dom::makeVariadicInvocable(
        [&ctx, global_helpers_key, name=std::string(name)](
            dom::Array const& args) -> Expected<dom::Value>
        {
            // Get function from global scope
            auto s = std::make_shared<Scope>(ctx);
            Value g = s->getGlobalObject();
            MRDOCS_ASSERT(g.isObject());
            MRDOCS_ASSERT(g.exists(global_helpers_key));
            Value helpers = g.get(global_helpers_key);
            MRDOCS_ASSERT(helpers.isObject());
            Value fn = helpers.get(name);
            MRDOCS_ASSERT(fn.isFunction());

            // Call function
            std::vector<dom::Value> arg_span;
            arg_span.reserve(args.size());
            for (auto const& arg : args)
            {
                arg_span.push_back(arg);
            }
            auto JSResult = fn.apply(arg_span);
            if (!JSResult)
            {
                return dom::Kind::Undefined;
            }

            // Convert result to dom::Value
            dom::Value result = JSResult->getDom();
            const bool isPrimitive =
                !result.isObject() &&
                !result.isArray() &&
                !result.isFunction();
            if (isPrimitive)
            {
                return result;
            }

            // Non-primitive values need to keep the
            // JS scope alive until the value is used
            // by the Handlebars engine.
            auto setScope = [&s](auto& result, auto TI)
            {
                using T = typename std::decay_t<decltype(TI)>::type;
                auto* impl = dynamic_cast<T*>(result.impl().get());
                MRDOCS_ASSERT(impl);
                impl->setScope(s);
            };
            if (result.isObject())
            {
                setScope(
                    result.getObject(),
                    std::type_identity<JSObjectImpl>{});
            }
            else if (result.isArray())
            {
                setScope(
                    result.getArray(),
                    std::type_identity<JSArrayImpl>{});
            }
            else if (result.isFunction())
            {
                setScope(
                    result.getFunction(),
                    std::type_identity<JSFunctionImpl>{});
            }
            return result;
        }));
    return {};
}

} // js
} // mrdocs

