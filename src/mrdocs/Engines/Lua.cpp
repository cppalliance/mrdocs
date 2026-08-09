//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "LuaHandlebars.hpp"
#include <mrdocs/Engines/Lua.hpp>
#include <mrdocs/Handlebars.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
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
        auto& obj = domObject_get(A, 1);
        auto key = luaM_getstring(A, 2);
        dom::Value value = luaValueToDom(L, 3);
        try
        {
            obj.set(key, std::move(value));
        }
        catch (std::exception const& ex)
        {
            return luaL_error(L, "%s", ex.what());
        }
        return 0;
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
        // Keep every C++ object (args, fn, result, and any Error the result
        // holds) inside this scope so all of them are destroyed before a
        // failure raises a Lua error. lua_error never returns: it longjmps,
        // skipping C++ destructors, so an object alive across it leaks. On
        // failure the message is copied onto the Lua stack first; the error
        // is raised only after the scope exits and the C++ objects are gone.
        {
            dom::Array args;
            for (int i = 2; i <= top; ++i)
            {
                args.push_back(luaValueToDom(L, i));
            }
            dom::Function fn = domFunction_get(A, 1);
            Expected<dom::Value> result = fn.call(args);
            if (result)
            {
                domValue_push(A, *result);
                return 1;
            }
            lua_pushstring(L, result.error().reason().c_str());
        }
        return lua_error(L);
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
        // Lua has a single nullary value, so an absent field (for example
        // the global namespace's name) maps to nil just as Null does.
        return lua_pushnil(A);
    case dom::Kind::Boolean:
        return lua_pushboolean(A, value.getBool());
    case dom::Kind::Integer:
        return lua_pushnumber(A, value.getInteger());
    case dom::Kind::String:
    case dom::Kind::SafeString:
        // A SafeString is already marked safe for an output format; to a
        // Lua script it is just its bytes.
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


// The public symbols are defined in per-symbol impl fragments below;
// this file owns the shared engine machinery and aggregates them.
#include "Lua/Context.ipp"
#include "Lua/Function.ipp"
#include "Lua/Param.ipp"
#include "Lua/Scope.ipp"
#include "Lua/String.ipp"
#include "Lua/Table.ipp"
#include "Lua/Value.ipp"
#include "Lua/registerHelper.ipp"

} // lua
} // mrdocs
