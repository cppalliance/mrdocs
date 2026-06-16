//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/LuaHandlebars.hpp>
#include <mrdocs/Support/Handlebars.hpp>
#include <mrdocs/Support/Lua.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <llvm/Support/raw_ostream.h>
#include <format>
#include <print>

// Lua's upstream headers are C-only and ship without `extern "C"` guards.
// Wrap the includes here.
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace mrdocs {
namespace lua {

#define LUA_INVALID_INDEX 0

// light userdata key for storing Context::Impl
static char gImplKey{};

//------------------------------------------------
//
// API
//
//------------------------------------------------

static void domObject_push_metatable(Access& A);
static void domArray_push(Access& A, dom::Array const&);
static void domValue_push(Access& A, dom::Value const&);

// Convert a Lua value at the given stack index into a `dom::Value`.
// Used by `__newindex` so scripts can write structured values
// (tables become objects or arrays based on key shape).
static dom::Value luaValueToDom(lua_State* L, int index);

static dom::Value
luaTableToDom(lua_State* L, int absIdx)
{
    // A table with any string key becomes a `dom::Object`; otherwise
    // it is treated as a 1-based array, matching the convention Lua
    // scripts already use.
    bool hasStringKey = false;
    lua_pushnil(L);
    while (lua_next(L, absIdx) != 0)
    {
        if (lua_type(L, -2) == LUA_TSTRING)
        {
            hasStringKey = true;
            lua_pop(L, 2);
            break;
        }
        lua_pop(L, 1);
    }

    if (hasStringKey)
    {
        dom::Object obj;
        lua_pushnil(L);
        while (lua_next(L, absIdx) != 0)
        {
            if (lua_type(L, -2) == LUA_TSTRING)
            {
                std::size_t klen = 0;
                char const* kdata = lua_tolstring(L, -2, &klen);
                obj.set(
                    std::string_view(kdata, klen),
                    luaValueToDom(L, -1));
            }
            lua_pop(L, 1);
        }
        return dom::Value(std::move(obj));
    }

    dom::Array arr;
    lua_Unsigned const len = lua_rawlen(L, absIdx);
    for (lua_Unsigned i = 1; i <= len; ++i)
    {
        lua_rawgeti(L, absIdx, static_cast<lua_Integer>(i));
        arr.push_back(luaValueToDom(L, -1));
        lua_pop(L, 1);
    }
    return dom::Value(std::move(arr));
}

static dom::Value
luaValueToDom(lua_State* L, int index)
{
    int const absIdx = lua_absindex(L, index);
    switch (lua_type(L, absIdx))
    {
    case LUA_TNIL:
        return dom::Value(nullptr);
    case LUA_TBOOLEAN:
        return dom::Value(lua_toboolean(L, absIdx) != 0);
    case LUA_TNUMBER:
        if (lua_isinteger(L, absIdx))
        {
            return dom::Value(
                static_cast<std::int64_t>(lua_tointeger(L, absIdx)));
        }
        return dom::Value(
            static_cast<std::int64_t>(lua_tonumber(L, absIdx)));
    case LUA_TSTRING:
    {
        std::size_t len = 0;
        char const* data = lua_tolstring(L, absIdx, &len);
        return dom::Value(std::string(data, len));
    }
    case LUA_TTABLE:
        return luaTableToDom(L, absIdx);
    default:
        return dom::Value();
    }
}

//------------------------------------------------
//
// Context, Scope
//
//------------------------------------------------

struct Context::Impl
{
    lua_State* L = nullptr;

    int objMetaRef = LUA_NOREF;
    int arrMetaRef = LUA_NOREF;
    int funcMetaRef = LUA_NOREF;

    ~Impl();
    Impl();
};

Context::
Impl::
~Impl()
{
    lua_close(L);
}

Context::
Impl::
Impl()
    : L(luaL_newstate())
{
    luaL_openlibs(L);

    // Store `this` at G[&gImplKey]
    lua_pushglobaltable(L);
    lua_pushlightuserdata(L, &gImplKey);
    lua_pushlightuserdata(L, this);
    lua_settable(L, -3);
    lua_pop(L, 1);
}

//------------------------------------------------

Context::~Context() = default;

Context::
Context()
    : impl_(std::make_shared<Impl>())
{
}

Context::
Context(
    Context const& other) noexcept = default;

void*
Context::
nativeState() const noexcept
{
    return impl_->L;
}

void
Scope::
reset()
{
}

Scope::
Scope(
    Context const& ctx) noexcept
    : ctx_(ctx)
    , refs_(0)
    , top_(lua_gettop(ctx.impl_->L))
{
}

Scope::
~Scope()
{
    MRDOCS_ASSERT(refs_ == 0);
    reset();
}

//------------------------------------------------
//
// Access
//
//------------------------------------------------

struct Access
{
    lua_State* L_ = nullptr;
    Context::Impl* impl_ = nullptr;

    explicit
    Access(lua_State* L)
        : L_(L)
    {
    }

    explicit
    Access(Context const& ctx) noexcept
        : L_(ctx.impl_->L)
        , impl_(ctx.impl_.get())
    {
    }

    explicit
    Access(Scope& scope) noexcept
        : L_(scope.ctx_.impl_->L)
        , impl_(scope.ctx_.impl_.get())
    {
    }

    operator lua_State*() const noexcept
    {
        return L_;
    }

    Context::Impl*
    operator->() noexcept
    {
        if(! impl_)
        {
            // Recover Impl* from registry
            lua_pushglobaltable(L_);
            lua_pushlightuserdata(L_, &gImplKey);
            lua_rawget(L_, -2);
            impl_ = static_cast<Context::Impl*>(
                lua_touserdata(L_, -1));
            lua_pop(L_, 2);
        }
        return impl_;
    }

    //--------------------------------------------

    static int index(Value const& v) noexcept
    {
        return v.index_;
    }

    static void addref(Scope& scope) noexcept
    {
        ++scope.refs_;
    }

    static void release(Scope& scope) noexcept
    {
        if(--scope.refs_ > 0)
            return;
        scope.reset();
    }

    static void swap(Value& v0, Value& v1) noexcept
    {
        std::swap(v0.scope_, v1.scope_);
        std::swap(v0.index_, v1.index_);
    }

    static void push(
        Param const& param, Scope& scope)
    {
        param.push(scope);
    }

    template<class T, class... Args>
    static T construct(Args&&... args)
    {
        return T(std::forward<Args>(args)...);
    }
};

//------------------------------------------------
//
// Lua helpers
//
//------------------------------------------------

static
void
luaM_report(
    Error const& err,
    source_location loc =
        source_location::current())
{
    SourceLocation Loc(err.location());
    std::print("{}\n"
               //"    at {}({})\n",
               ,
               err.message(), Loc.file_name()
               //,Loc.line()
    );
}

// Pop the Lua error off the stack
// and return it as an Error object.
static
Error
luaM_popError(
    lua_State* L,
    source_location loc =
        source_location::current())
{
    std::size_t len;
    auto const data = lua_tolstring(L, -1, &len);
    Error err(std::string(data, len), loc);
    lua_pop(L, 1);
    return err;
}

// Return a string_view representing
// the string at the given stack index.
static
std::string_view
luaM_getstring(
    lua_State* L, int index)
{
    std::size_t size;
    auto data = lua_tolstring(L, index, &size);
    return { data, size };
}

// Shortcut to push a string_view as a string
static
void
luaM_pushstring(
    lua_State* L, std::string_view s)
{
    lua_pushlstring(L, s.data(), s.size());
}

//------------------------------------------------
//
// dom::Array
//
//------------------------------------------------

// Return a userdata as a dom::ArrayPtr&
static
dom::Array&
domArray_get(
    Access& A, int index)
{
    MRDOCS_ASSERT(
        lua_type(A, index) == LUA_TUSERDATA);
    return *static_cast<dom::Array*>(
        lua_touserdata(A, index));
}

// Push the domArray metatable onto the stack
static
void
domArray_push_metatable(
    Access& A)
{
    if(A->arrMetaRef != LUA_NOREF)
    {
        lua_rawgeti(A, LUA_REGISTRYINDEX, A->arrMetaRef);
        return;
    }

    lua_createtable(A, 0, 4);

    // Effect:      return t[i]
    // Signature:   (t, i)
    //
    // Lua-convention 1-indexed: `arr[1]` is the first element, and any
    // index outside `[1, #arr]` returns nil. Together with `__len` and
    // a 1-indexed `__pairs`, this is what makes the standard `ipairs`,
    // `pairs`, and `#` operator work.
    luaM_pushstring(A, "__index");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        if(! lua_isnumber(A, 2))
        {
            lua_pop(A, lua_gettop(A));
            lua_pushnil(A);
            return 1;
        }
        lua_Number raw = lua_tonumber(A, 2);
        auto const& arr = domArray_get(A, 1);
        lua_pop(A, lua_gettop(A));
        if(raw >= 1 &&
           static_cast<std::size_t>(raw) <= arr.size())
        {
            domValue_push(
                A,
                arr.at(static_cast<std::size_t>(raw) - 1));
        }
        else
        {
            lua_pushnil(A);
        }
        return 1;
    });
    lua_settable(A, -3);

#if 0
    // Effect:      t[k] = v
    // Signature:   (t, k, v)
    luaM_pushstring(A, "__newindex");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
    });
    lua_settable(A, -3);
#endif

    // Effect:      return #t
    // Signature:   (t)
    luaM_pushstring(A, "__len");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        auto const& arr = domArray_get(A, 1);
        lua_pushinteger(A, static_cast<lua_Integer>(arr.size()));
        return 1;
    });
    lua_settable(A, -3);

    // Effect:      return next(t [, index])
    // Signature:   (t [, index])
    //
    // The upvalue holds the next 1-indexed key to yield.
    static constexpr auto const next =
    [](lua_State* L)
    {
        Access A(L);
        auto const narg = lua_gettop(A);
        auto arr = domArray_get(A, 1);
        lua_pop(A, narg);
        if( arr.empty() ||
            lua_isnil(A, lua_upvalueindex(1)))
        {
            lua_pushnil(A);
            lua_pushnil(A);
            return 2;
        }
        lua_Number key = lua_tonumber(A, lua_upvalueindex(1));
        lua_pushnumber(A, key);
        domValue_push(
            A,
            arr.at(static_cast<std::size_t>(key) - 1));
        ++key;
        if(static_cast<std::size_t>(key) <= arr.size())
            lua_pushnumber(A, key);
        else
            lua_pushnil(A);
        lua_replace(A, lua_upvalueindex(1));
        return 2;
    };

    // Effect:      return pairs(t)
    // Signature:   (t)
    //
    // First key handed to `next` is 1 (Lua convention).
    luaM_pushstring(A, "__pairs");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        auto arr = domArray_get(A, 1);
        if(! arr.empty())
            lua_pushnumber(A, 1);
        else
            lua_pushnil(A);
        lua_pushcclosure(A, next, 1);
        lua_rotate(A, -2, 1);
        lua_pushnil(A);
        return 3;
    });
    lua_settable(A, -3);

    // Effect:      ~SharedPtr<dom::Array>
    // Signature:   (table)
    luaM_pushstring(A, "__gc");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        std::destroy_at(&domArray_get(A, 1));
        return 0;
    });
    lua_settable(A, -3);

    lua_pushvalue(A, -1);
    A->arrMetaRef = luaL_ref(A, LUA_REGISTRYINDEX);
}

// Push a dom::Array onto the stack
static
void
domArray_push(
    Access& A,
    dom::Array const& arr)
{
    auto& arr_ = *static_cast<
        dom::Array*>(lua_newuserdatauv(
            A, sizeof(dom::Array), 0));
    domArray_push_metatable(A);
    lua_setmetatable(A, -2);
    std::construct_at(&arr_, arr);
}

//------------------------------------------------
//
// dom::Object
//
//------------------------------------------------

// Return a userdata as a dom::ObjectPtr&
static
dom::Object&
domObject_get(
    Access& A, int index)
{
    MRDOCS_ASSERT(
        lua_type(A, index) == LUA_TUSERDATA);
    return *static_cast<dom::Object*>(
        lua_touserdata(A, index));
}

// Push the domObject metatable onto the stack
static
void
domObject_push_metatable(
    Access& A)
{
    if(A->objMetaRef != LUA_NOREF)
    {
        lua_rawgeti(A, LUA_REGISTRYINDEX, A->objMetaRef);
        return;
    }

    lua_createtable(A, 0, 4);

    // Effect:      return t[k]
    // Signature:   (t, k)
    luaM_pushstring(A, "__index");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        domValue_push(A,
            domObject_get(A, 1).get(
                luaM_getstring(A, 2)));
        return 1;
    });
    lua_settable(A, -3);

    // Effect:      t[k] = v
    // Signature:   (t, k, v)
    //
    // Routes the assignment through `dom::Object::set` on the
    // underlying holder. The default `dom::Object` writes to its own
    // overlay; the symbol-proxy implementation used by corpus
    // extensions overrides `set` to mutate the live C++ object
    // instead. A `std::exception` from that override propagates back
    // here and is rethrown as a Lua error so the script sees a real
    // failure instead of a silent assignment.
    luaM_pushstring(A, "__newindex");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        // `lua_error` longjmps to the enclosing `pcall` and skips any
        // pending C++ destructor on this frame, so scope the locals and
        // stage the error message before raising.
        bool raised = false;
        {
            auto& obj = domObject_get(A, 1);
            auto key = luaM_getstring(A, 2);
            dom::Value value = luaValueToDom(L, 3);
            try
            {
                obj.set(key, std::move(value));
            }
            catch (std::exception const& ex)
            {
                luaL_where(A, 1);
                lua_pushstring(A, ex.what());
                lua_concat(A, 2);
                raised = true;
            }
        }
        return raised ? lua_error(A) : 0;
    });
    lua_settable(A, -3);

    // Effect:      return pairs(t)
    // Signature:   (t)
    luaM_pushstring(A, "__pairs");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        auto& obj = domObject_get(A, 1);
        if(! obj.empty())
            lua_pushnumber(A, 0);
        else
            lua_pushnil(A);
        // Effect:      return next(t [, index])
        // Signature:   (t [, index])
        lua_pushcclosure(A,
        [](lua_State* L)
        {
            Access A(L);
            auto const narg = lua_gettop(A);
            auto& obj = domObject_get(A, 1);
            lua_pop(A, narg);
            if( obj.empty() ||
                lua_isnil(A, lua_upvalueindex(1)))
            {
                lua_pushnil(A);
                lua_pushnil(A);
                return 2;
            }
            auto index = lua_tonumber(A, lua_upvalueindex(1));
            // Visit obj and get the index-th element and key
            std::size_t i = 0;
            obj.visit([&](dom::String key, dom::Value value)
            {
                if (i == index) {
                    luaM_pushstring(A, key);
                    domValue_push(A, value);
                    return false;
                }
                ++i;
                return true;
            });
            ++index;
            if(index < obj.size())
                lua_pushnumber(A, index);
            else
                lua_pushnil(A);
            lua_replace(A, lua_upvalueindex(1));
            return 2;
        }, 1);
        lua_rotate(A, -2, 1);
        lua_pushnil(A);
        return 3;
    });
    lua_settable(A, -3);

    // Effect:      ~SharedPtr<dom::Object>
    // Signature:   (table)
    luaM_pushstring(A, "__gc");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        std::destroy_at(&domObject_get(A, 1));
        return 0;
    });
    lua_settable(A, -3);

    lua_pushvalue(A, -1);
    A->objMetaRef = luaL_ref(A, LUA_REGISTRYINDEX);
}

// Push a dom::Object onto the stack
static
void
domObject_push(
    Access& A,
    dom::Object const& obj)
{
    auto& obj_ = *static_cast<
        dom::Object*>(lua_newuserdatauv(
            A, sizeof(dom::Object), 0));
    domObject_push_metatable(A);
    lua_setmetatable(A, -2);
    std::construct_at(&obj_, obj);
}

//------------------------------------------------
//
// dom::Function
//
//------------------------------------------------

static
dom::Function&
domFunction_get(Access& A, int index)
{
    return *static_cast<dom::Function*>(
        lua_touserdata(A, index));
}

// Push the domFunction metatable onto the stack
static
void
domFunction_push_metatable(
    Access& A)
{
    if(A->funcMetaRef != LUA_NOREF)
    {
        lua_rawgeti(A, LUA_REGISTRYINDEX, A->funcMetaRef);
        return;
    }

    lua_createtable(A, 0, 2);

    // Effect:      collect Lua args, dispatch to dom::Function::call,
    //              push the resulting dom::Value.
    // Signature:   (userdata, args...)
    //
    // The userdata at index 1 is the function holder; real call args
    // start at index 2. Errors from `call` are turned into Lua errors
    // so a script can recover with `pcall`.
    luaM_pushstring(A, "__call");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        int const top = lua_gettop(A);
        // `lua_error` longjmps to the enclosing `pcall` and skips any
        // pending C++ destructor on this frame, so stage the outcome -
        // the result value, or a location-prefixed error message, on the
        // Lua stack - and let every local here be destroyed before
        // raising.
        bool raised = false;
        {
            dom::Array args;
            for (int i = 2; i <= top; ++i)
            {
                args.push_back(luaValueToDom(L, i));
            }
            Expected<dom::Value> result = domFunction_get(A, 1).call(args);
            if (result)
            {
                domValue_push(A, *result);
            }
            else
            {
                luaL_where(A, 1);
                lua_pushstring(A, result.error().reason().c_str());
                lua_concat(A, 2);
                raised = true;
            }
        }
        return raised ? lua_error(A) : 1;
    });
    lua_settable(A, -3);

    // Effect:      ~dom::Function
    // Signature:   (userdata)
    luaM_pushstring(A, "__gc");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        std::destroy_at(&domFunction_get(A, 1));
        return 0;
    });
    lua_settable(A, -3);

    lua_pushvalue(A, -1);
    A->funcMetaRef = luaL_ref(A, LUA_REGISTRYINDEX);
}

// Push a dom::Function onto the stack
static
void
domFunction_push(
    Access& A,
    dom::Function fn)
{
    auto& slot = *static_cast<dom::Function*>(
        lua_newuserdatauv(A, sizeof(dom::Function), 0));
    domFunction_push_metatable(A);
    lua_setmetatable(A, -2);
    std::construct_at(&slot, std::move(fn));
}

//------------------------------------------------
//
// dom::Value
//
//------------------------------------------------

static void
domValue_push(
    Access& A,
    dom::Value const& value)
{
    switch(value.kind())
    {
    case dom::Kind::Null:
        return lua_pushnil(A);
    case dom::Kind::Undefined:
        // Lua has a single nullary value, so a missing field maps to
        // `nil` just as `Null` does. A read of an absent field (for
        // example the global namespace's name) yields `Undefined` and
        // must not abort.
        return lua_pushnil(A);
    case dom::Kind::Boolean:
        return lua_pushboolean(A, value.getBool());
    case dom::Kind::Integer:
        return lua_pushnumber(A, value.getInteger());
    case dom::Kind::String:
    case dom::Kind::SafeString:
        // A `SafeString` is a string already marked safe for an output
        // format; to a Lua script it is just its bytes.
        return luaM_pushstring(A, value.getString());
    case dom::Kind::Array:
        return domArray_push(A, value.getArray());
    case dom::Kind::Object:
        return domObject_push(A, value.getObject());
    case dom::Kind::Function:
        return domFunction_push(A, value.getFunction());
    default:
        MRDOCS_UNREACHABLE();
    }
}

//------------------------------------------------

static
char const*
Reader(
    lua_State *L,
    void* data,
    size_t* size)
{
    std::string_view& s = *static_cast<
        std::string_view*>(data);
    if(! s.empty())
    {
        *size = s.size();
        auto data_ = s.data();
        s = {};
        return data_;
    }
    *size = 0;
    return nullptr;
}

Expected<Function>
Scope::
loadChunk(
    std::string_view luaChunk,
    zstring chunkName,
    source_location loc)
{
    Access A(*this);
    auto rc = lua_load(A,
        &Reader, &luaChunk, chunkName.c_str(), nullptr);
    if(rc != LUA_OK)
        return Unexpected(luaM_popError(A, loc));
    return A.construct<Function>(-1, *this);
}

Expected<Function>
Scope::
loadChunk(
    std::string_view luaChunk,
    source_location loc)
{
    SourceLocation Loc(loc);
    return loadChunk(luaChunk,
                     std::format("{}({})", Loc.file_name(), Loc.line()), loc);
}

Expected<Function>
Scope::
loadChunkFromFile(
    std::string_view fileName,
    source_location loc)
{
    MRDOCS_TRY(auto luaChunk, files::getFileText(fileName));
    return loadChunk(luaChunk, fileName, loc);
}

Table
Scope::
getGlobalTable()
{
    Access A(*this);
    lua_pushglobaltable(A);
    return A.construct<Table>(-1, *this);
}

Expected<Value>
Scope::
getGlobal(
    std::string_view key,
    source_location loc)
{
    Access A(*this);
    lua_pushglobaltable(A);
    luaM_pushstring(A, key);
    auto type = lua_gettable(A, -2);
    lua_replace(A, -2);
    if(type == LUA_TNIL)
    {
        MRDOCS_ASSERT(lua_isnil(A, -1));
        lua_pop(A, 1);
        return Unexpected(formatError("global key '{}' not found", key));
    }
    return A.construct<Value>(-1, *this);
}

Value
Scope::
pushDom(dom::Value const& value)
{
    Access A(*this);
    domValue_push(A, value);
    return A.construct<Value>(-1, *this);
}

//------------------------------------------------
//
// Param
//
//------------------------------------------------

void
Param::
push(Scope& scope) const
{
    Access A(scope);
    switch(kind_)
    {
    case Kind::nil:
        return lua_pushnil(A);
    case Kind::boolean:
        return lua_pushboolean(A, b_);
    case Kind::integer:
        return lua_pushinteger(A, i_);
    case Kind::string:
        return luaM_pushstring(A, s_);
    case Kind::value:
        return lua_pushvalue(A, index_);
    case Kind::domArray:
        domArray_push(A, arr_);
        return;
    case Kind::domObject:
        domObject_push(A, obj_);
        return;
    default:
        MRDOCS_UNREACHABLE();
    }
}

Param::
Param(
    Param&& other) noexcept
    : kind_(other.kind_)
{
    switch(kind_)
    {
    case Kind::nil:
        return;
    case Kind::boolean:
        std::construct_at(&b_, other.b_);
        return;
    case Kind::integer:
        std::construct_at(&i_, other.i_);
        return;
    case Kind::string:
        std::construct_at(&s_, other.s_);
        return;
    case Kind::value:
        std::construct_at(&index_, other.index_);
        return;
    case Kind::domArray:
        std::construct_at(&arr_, other.arr_);
        return;
    case Kind::domObject:
        std::construct_at(&obj_, other.obj_);
        return;
    default:
        MRDOCS_UNREACHABLE();
    }
}

Param::
~Param()
{
    switch(kind_)
    {
    case Kind::nil:
    case Kind::boolean:
    case Kind::integer:
        return;
    case Kind::string:
        std::destroy_at(&s_);
        return;
    case Kind::value:
        return;
    case Kind::domArray:
        std::destroy_at(&arr_);
        return;
    case Kind::domObject:
        std::destroy_at(&obj_);
        return;
    default:
        MRDOCS_UNREACHABLE();
    }
}

Param::
Param(
    std::nullptr_t) noexcept
    : kind_(Kind::nil)
{
}

Param::
Param(
    std::int64_t i) noexcept
    : kind_(Kind::integer)
    , i_(static_cast<lua_Integer>(i))
{
}

Param::
Param(
    std::string_view s) noexcept
    : kind_(Kind::string)
    , s_(s)
{
}

Param::
Param(
    Value const& value) noexcept
    : kind_(Kind::value)
    , index_(Access::index(value))
{
}

Param::
Param(
    dom::Array arr) noexcept
    : kind_(Kind::domArray)
    , arr_(std::move(arr))
{
}

Param::
Param(
    dom::Object obj) noexcept
    : kind_(Kind::domObject)
    , obj_(std::move(obj))
{
}

Param::
Param(
    dom::Value const& value) noexcept
    : Param(
        [&value]
        {
            switch(value.kind())
            {
            case dom::Kind::Null:
                return Param(nullptr);
            case dom::Kind::Boolean:
                return Param(value.getBool());
            case dom::Kind::Integer:
                return Param(static_cast<lua_Integer>(
                    value.getInteger()));
            case dom::Kind::String:
                return Param(value.getString());
            case dom::Kind::Array:
                return Param(value.getArray());
            case dom::Kind::Object:
                return Param(value.getObject());
            default:
                MRDOCS_UNREACHABLE();
            }
        }())
{
}

//------------------------------------------------
//
// Value
//
//------------------------------------------------

Value::
Value(
    int index,
    Scope& scope) noexcept
    : scope_(&scope)
    , index_(lua_absindex(Access(scope)->L, index))
{
    Access::addref(*scope_);
}

Value::
~Value()
{
    if( ! scope_)
        return;
    Access A(*scope_);
    if(index_ == lua_gettop(A) - 1)
        lua_pop(A, 1);
    Access::release(*scope_);
}

// construct an empty value
Value::
Value() noexcept
    : scope_(nullptr)
    , index_(0)
{
}

Value::
Value(
    Value&& other) noexcept
    : scope_(other.scope_)
    , index_(other.index_)
{
    other.scope_ = nullptr;
    other.index_ = LUA_INVALID_INDEX;
}

Value::
Value(
    Value const& other)
    : scope_(other.scope_)
{
    if(! scope_)
    {
        index_ = LUA_INVALID_INDEX;
        return;
    }

    Access A(*scope_);
    lua_pushvalue(A, other.index_);
    index_ = lua_absindex(A, -1);
    A.addref(*scope_);
}

Type
Value::
type() const noexcept
{
    if(! scope_)
        return Type::nil;
    Access A(*scope_);
    auto const ty = lua_type(A, index_);
    switch(ty)
    {
    case LUA_TNIL:      return Type::nil;
    case LUA_TBOOLEAN:  return Type::boolean;
    case LUA_TLIGHTUSERDATA:
                        MRDOCS_UNREACHABLE();
    case LUA_TNUMBER:   return Type::number;
    case LUA_TSTRING:   return Type::string;
    case LUA_TTABLE:    return Type::table;
    case LUA_TFUNCTION: return Type::function;
    case LUA_TUSERDATA: MRDOCS_UNREACHABLE();
    case LUA_TTHREAD:   MRDOCS_UNREACHABLE();
    default:
        MRDOCS_UNREACHABLE();
    }
}

std::string
Value::
displayString() const
{
    Access A(*scope_);
    switch(lua_type(A, index_))
    {
    case LUA_TNIL:
        return "nil";
    case LUA_TBOOLEAN:
        if(lua_toboolean(A, index_))
            return "true";
        return "false";
    case LUA_TLIGHTUSERDATA:
        return "[luserdata]";
    case LUA_TNUMBER:
        return std::to_string(
            lua_tonumber(A, index_));
    case LUA_TSTRING:
        return std::string(
            luaM_getstring(A, index_));
    case LUA_TTABLE:
        return "[table]";
    case LUA_TFUNCTION:
        return "[function]";
    case LUA_TUSERDATA:
        return "[userdata]";
    case LUA_TTHREAD:
        return "[thread]";
    default:
        MRDOCS_UNREACHABLE();
    }
}

Expected<Value>
Value::
callImpl(
    Param const* args,
    std::size_t narg)
{
    Access A(*scope_);
    lua_pushvalue(A, index_);
    for(std::size_t i = 0; i < narg; ++i)
        Access::push(args[i], *scope_);
    auto result = lua_pcall(A, narg, 1, 0);
    if(result != LUA_OK)
        return Unexpected(luaM_popError(A));
    return A.construct<Value>(-1, *scope_);
}

//------------------------------------------------
//
// String
//
//------------------------------------------------

String::
String(
    int index,
    Scope& scope) noexcept
    : Value(index, scope)
{
}

String::
String(
    Value value)
    : Value(std::move(value))
{
    Access A(*scope_);
    switch(lua_type(A, index_))
    {
    case LUA_TNUMBER:
        lua_tostring(A, index_);
        break;
    case LUA_TSTRING:
        break;
    default:
        Error("not a string").Throw();
    }
}

std::string_view
String::
get() const noexcept
{
    if(! scope_)
        return {};
    Access A(*scope_);
    return luaM_getstring(A, index_);
}

//------------------------------------------------
//
// Function
//
//------------------------------------------------

Function::
Function(
    int index,
    Scope& scope) noexcept
    : Value(index, scope)
{
}

Function::
Function(
    Value value)
    : Value(std::move(value))
{
    Access A(*scope_);
    if(lua_type(A, index_) != LUA_TFUNCTION)
        Error("not a function").Throw();
}

//------------------------------------------------
//
// Table
//
//------------------------------------------------

Table::
Table(
    Scope& scope,
    dom::Object const& obj)
    : Value(
        [&]
        {
            Access A(scope);
            domObject_push(A, obj);
            return A.construct<Value>(-1, scope);
        }())
{
}

Table::
Table(
    int index,
    Scope& scope)
    : Value(index, scope)
{
    if(! scope_)
        return;
    Access A(*scope_);
    if(lua_type(A, index_) != LUA_TTABLE)
        Error("not a Table").Throw();
}

Table::
Table(
    Value value)
    : Value(std::move(value))
{
    Access A(*scope_);
    if(lua_type(A, index_) != LUA_TTABLE)
        Error("not a Table").Throw();
}

Table::
Table(
    Scope& scope)
    : Value(
        [&]
        {
            Access A(scope);
            lua_newtable(A);
            return -1;
        }(), scope)
{
}

Value
Table::
get(
    std::string_view key) const
{
    Access A(*scope_);
    luaM_pushstring(A, key);
    lua_gettable(A, index_);
    return A.construct<Value>(-1, *scope_);
}

void
Table::
set(
    std::string_view key,
    Param value) const
{
    Access A(*scope_);
    luaM_pushstring(A, key);
    Access::push(value, *scope_);
    lua_settable(A, index_);
}

Expected<Value>
Table::
callImpl(
    std::string_view key,
    Param const* data,
    std::size_t size) const
{
    Access A(*scope_);
    luaM_pushstring(A, key);
    lua_gettable(A, index_);
    if(lua_isnil(A, -1))
        return Unexpected(formatError("method {} not found", key));
    if(! lua_isfunction(A, -1))
        return Unexpected(formatError("table key '{}' is not a function", key));
    for(std::size_t i = 0; i < size; ++i)
        A.push(data[i], *scope_);
    auto rc = lua_pcall(A, size, 1, 0);
    if(rc != LUA_OK)
        return Unexpected(luaM_popError(A));
    return A.construct<Value>(-1, *scope_);
}

//------------------------------------------------
//
// registerHelper
//
//------------------------------------------------

// Convert the Lua value at the given stack index to a dom::Value.
// Used to marshal a helper's return value back to Handlebars.
//
// Userdata wrapping our own dom::Object/dom::Array are unwrapped in place
// (preserving identity); raw Lua tables are converted to a dom::Object using
// string keys (non-string keys are skipped). Non-representable types
// (function, thread, light userdata) become null.
static
dom::Value
luaToDom(Access& A, int idx)
{
    int const t = lua_type(A, idx);
    switch(t)
    {
    case LUA_TNIL:
        return dom::Value();
    case LUA_TBOOLEAN:
        return dom::Value(lua_toboolean(A, idx) != 0);
    case LUA_TNUMBER:
        if (lua_isinteger(A, idx))
            return dom::Value(static_cast<std::int64_t>(
                lua_tointeger(A, idx)));
        // dom::Value has no double kind; truncate floats. Helpers that need
        // sub-integer precision should return strings.
        return dom::Value(static_cast<std::int64_t>(
            lua_tonumber(A, idx)));
    case LUA_TSTRING:
    {
        std::size_t len;
        char const* data = lua_tolstring(A, idx, &len);
        return dom::Value(std::string(data, len));
    }
    case LUA_TTABLE:
    {
        dom::Object obj;
        int const absIdx = lua_absindex(A, idx);
        lua_pushnil(A);
        while (lua_next(A, absIdx) != 0)
        {
            if (lua_type(A, -2) == LUA_TSTRING)
            {
                std::size_t klen;
                char const* kdata = lua_tolstring(A, -2, &klen);
                obj.set(
                    std::string_view(kdata, klen),
                    luaToDom(A, -1));
            }
            lua_pop(A, 1); // pop value, keep key for next iteration
        }
        return dom::Value(std::move(obj));
    }
    case LUA_TUSERDATA:
    {
        if (! lua_getmetatable(A, idx))
            return dom::Value();
        int const metaIdx = lua_absindex(A, -1);
        dom::Value result;
        bool matched = false;

        if (A->objMetaRef != LUA_NOREF)
        {
            lua_rawgeti(A, LUA_REGISTRYINDEX, A->objMetaRef);
            if (lua_rawequal(A, metaIdx, -1))
            {
                result = dom::Value(*static_cast<dom::Object*>(
                    lua_touserdata(A, idx)));
                matched = true;
            }
            lua_pop(A, 1);
        }
        if (! matched && A->arrMetaRef != LUA_NOREF)
        {
            lua_rawgeti(A, LUA_REGISTRYINDEX, A->arrMetaRef);
            if (lua_rawequal(A, metaIdx, -1))
            {
                result = dom::Value(*static_cast<dom::Array*>(
                    lua_touserdata(A, idx)));
            }
            lua_pop(A, 1);
        }
        lua_pop(A, 1); // pop metatable
        return result;
    }
    default:
        return dom::Value();
    }
}

namespace detail {

// Registry-anchored handle that owns a Lua function's lifetime independently
// of any Scope. The function lives in LUA_REGISTRYINDEX until the handle is
// destroyed, so the Handlebars helper closure can keep firing across renders.
struct LuaHelperHandle
{
    Context ctx;
    int ref;

    LuaHelperHandle(Context c, int r) noexcept
        : ctx(std::move(c)), ref(r)
    {
    }
    LuaHelperHandle(LuaHelperHandle const&) = delete;
    LuaHelperHandle& operator=(LuaHelperHandle const&) = delete;
    ~LuaHelperHandle()
    {
        Access A(ctx);
        luaL_unref(A, LUA_REGISTRYINDEX, ref);
    }
};

// Strip the trailing Handlebars options object (matching the JS helper
// semantics), push positional args to Lua, run the helper, and return the
// converted result. Errors from lua_pcall surface as Unexpected.
static
Expected<dom::Value, Error>
invokeHelperRef(
    std::shared_ptr<LuaHelperHandle> const& handle,
    dom::Array const& args)
{
    if (args.empty())
    {
        return Unexpected(Error(
            "Handlebars helper called without arguments; "
            "expected options object as last argument"));
    }
    dom::Value const& options = args.back();
    if (! options.isObject())
    {
        return Unexpected(Error(
            "Handlebars helper options must be an object; "
            "ensure the helper is called from a template context"));
    }

    Scope scope(handle->ctx);
    Access A(scope);

    lua_rawgeti(A, LUA_REGISTRYINDEX, handle->ref);

    std::size_t const narg = args.size() - 1;
    for (std::size_t i = 0; i < narg; ++i)
    {
        Param p(args.get(i));
        Access::push(p, scope);
    }

    int const rc = lua_pcall(A, static_cast<int>(narg), 1, 0);
    if (rc != LUA_OK)
        return Unexpected(luaM_popError(A));

    dom::Value result = luaToDom(A, lua_gettop(A));
    lua_pop(A, 1);
    return result;
}

// Invoke the registry-anchored function with every argument (there is no
// Handlebars options object to strip), converting the Lua result back to
// a `dom::Value`. Errors from `lua_pcall` surface as `Unexpected`.
static
Expected<dom::Value, Error>
invokeRef(
    std::shared_ptr<LuaHelperHandle> const& handle,
    dom::Array const& args)
{
    Scope scope(handle->ctx);
    Access A(scope);

    lua_rawgeti(A, LUA_REGISTRYINDEX, handle->ref);

    std::size_t const narg = args.size();
    for (std::size_t i = 0; i < narg; ++i)
    {
        Param p(args.get(i));
        Access::push(p, scope);
    }

    // A default-constructed `Expected` holds a value; both branches below
    // overwrite it, so the default is never observed.
    Expected<dom::Value, Error> result;
    if (lua_pcall(A, static_cast<int>(narg), 1, 0) == LUA_OK)
    {
        result = luaToDom(A, lua_gettop(A));
        lua_pop(A, 1);
    }
    else
    {
        result = Unexpected(luaM_popError(A));
    }
    return result;
}

} // detail

Expected<void, Error>
registerHelper(
    Handlebars& hbs,
    std::string_view name,
    Context& ctx,
    std::string_view script)
{
    // Resolve a Lua chunk to a callable: the chunk's return value is preferred
    // (the "return function(...) ... end" idiom), falling back to a global of
    // the same name (the "function name(...) ... end" idiom).
    Scope scope(ctx);

    auto chunk = scope.loadChunk(script, std::string(name));
    if (! chunk)
        return Unexpected(chunk.error());

    auto chunkResult = chunk->call();
    if (! chunkResult)
        return Unexpected(chunkResult.error());

    Access A(scope);
    int ref;

    if (chunkResult->isFunction())
    {
        lua_pushvalue(A, Access::index(*chunkResult));
        ref = luaL_ref(A, LUA_REGISTRYINDEX);
    }
    else
    {
        auto global = scope.getGlobal(name);
        if (! global)
        {
            return Unexpected(formatError(
                "lua helper '{}': chunk did not return a function "
                "and no global of that name was defined",
                name));
        }
        if (! global->isFunction())
        {
            return Unexpected(formatError(
                "lua helper '{}' is not a function", name));
        }
        lua_pushvalue(A, Access::index(*global));
        ref = luaL_ref(A, LUA_REGISTRYINDEX);
    }

    auto handle = std::make_shared<detail::LuaHelperHandle>(ctx, ref);

    hbs.registerHelper(
        std::string(name),
        dom::makeVariadicInvocable(
            [handle](dom::Array const& args) -> Expected<dom::Value, Error>
            {
                return detail::invokeHelperRef(handle, args);
            }));

    return {};
}

dom::Function
makeCallable(Context ctx, int ref)
{
    auto handle = std::make_shared<detail::LuaHelperHandle>(
        std::move(ctx), ref);
    return dom::makeVariadicInvocable(
        [handle](dom::Array const& args) -> Expected<dom::Value, Error>
        {
            return detail::invokeRef(handle, args);
        });
}

//------------------------------------------------

void
lua_dump(dom::Object const& obj)
{
    Context ctx;

    auto exp = tryLoadHandlebars(ctx);
    MRDOCS_CHECK_OR(exp, luaM_report(exp.error()));

    Scope scope(ctx);
    scope.loadChunk(
    R"(
        function dump(data)
        for k,v in pairs(data) do
           print(k, v)
        end
        end
    )").value()();
    auto dump = scope.getGlobal("dump").value();
    dump(obj);
}

void
lua_main()
{
    Context ctx;

    auto exp = tryLoadHandlebars(ctx);
    MRDOCS_CHECK_OR(exp, luaM_report(exp.error()));

    Scope scope(ctx);
    scope.loadChunk(
    R"(
        function testFunc(data)
        for k,v in pairs(data) do
           print(k, v)
        end
        end
    )").value()();
    auto testFunc = scope.getGlobal("testFunc").value();
    dom::Object obj({
            { "x", "0" },
            { "y", "1" }
            });
    std::println("{}", testFunc(obj));
}

} // lua
} // mrdocs
