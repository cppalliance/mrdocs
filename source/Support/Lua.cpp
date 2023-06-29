//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Support/Lua.hpp>
#include <mrdox/Support/Path.hpp>
#include <mrdox/Support/Report.hpp>
#include "../../lua/src/lua.hpp"
#include <fmt/format.h>

#include <llvm/Support/raw_ostream.h>
#include "Support/LuaHandlebars.hpp"

namespace clang {
namespace mrdox {
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
static void domValue_push(Access& A, dom::Value const&);

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
    : impl_(makeShared<Impl>())
{
}

Context::
Context(
    Context const& other) noexcept = default;

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
    MRDOX_ASSERT(refs_ == 0);
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
    fmt::print(
        "{}\n"
        //"    at {}({})\n",
        ,err.reason()
        ,Loc.file_name()
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
    MRDOX_ASSERT(
        lua_type(A, index) == LUA_TUSERDATA);
    return *static_cast<dom::Array*>(
        lua_touserdata(A, index));
}

// Push the domObject metatable onto the stack
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

    lua_createtable(A, 0, 3);

    // Effect:      return t[i]
    // Signature:   (t, i)
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
        std::size_t index = lua_tonumber(A, 2);
        auto const& arr = domArray_get(A, 1);
        lua_pop(A, lua_gettop(A));
        if(index < arr.size())
            domValue_push(A, arr.at(index));
        else
            lua_pushnil(A);
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

    // Effect:      return next(t [, index])
    // Signature:   (t [, index])
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
        auto index = lua_tonumber(A, lua_upvalueindex(1));
        lua_pushnumber(A, index);
        domValue_push(A, arr.at(index));
        ++index;
        if(index < arr.size())
            lua_pushnumber(A, index);
        else
            lua_pushnil(A);
        lua_replace(A, lua_upvalueindex(1));
        return 2;
    };

    // Effect:      return pairs(t)
    // Signature:   (t)
    luaM_pushstring(A, "__pairs");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        auto arr = domArray_get(A, 1);
        if(! arr.empty())
            lua_pushnumber(A, 0);
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
    MRDOX_ASSERT(
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
        lua_replace(A, 1);
        return 1;
    });
    lua_settable(A, -3);

    // Effect:      t[k] = v
    // Signature:   (t, k, v)
    luaM_pushstring(A, "__newindex");
    lua_pushcfunction(A,
    [](lua_State* L)
    {
        Access A(L);
        auto& obj = domObject_get(A, 1);
        auto key = luaM_getstring(A, 2);
        switch(lua_type(A, 3))
        {
        case LUA_TNIL:
            // VFALCO should erase instead?
            obj.set(key, nullptr);
            break;
        case LUA_TBOOLEAN:
            obj.set(key, lua_toboolean(A, 3) != 0);
            break;
        case LUA_TLIGHTUSERDATA:
            MRDOX_UNREACHABLE();
        case LUA_TNUMBER:
            obj.set(key, lua_tonumber(A, 3));
            break;
        case LUA_TSTRING:
            obj.set(key, luaM_getstring(A, 3));
            break;
        case LUA_TTABLE:
            // VFALCO TODO
            MRDOX_UNREACHABLE();
            break;
        case LUA_TFUNCTION:
            // VFALCO TODO
            MRDOX_UNREACHABLE();
            break;
        case LUA_TUSERDATA:
            MRDOX_UNREACHABLE();
        case LUA_TTHREAD:
            MRDOX_UNREACHABLE();
        default:
            MRDOX_UNREACHABLE();
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
            auto entries = obj.entries();
            auto const& kv = entries[index];
            luaM_pushstring(A, kv.first);
            domValue_push(A, kv.second);
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
    case dom::Kind::Boolean:
        return lua_pushboolean(A, value.getBool());
    case dom::Kind::Integer:
        return lua_pushnumber(A, value.getInteger());
    case dom::Kind::String:
        return luaM_pushstring(A, value.getString());
    case dom::Kind::Array:
        MRDOX_UNREACHABLE();
        //return domArray_push(A, value.getArray());
    case dom::Kind::Object:
        return domObject_push(A, value.getObject());
    default:
        MRDOX_UNREACHABLE();
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
        return luaM_popError(A, loc);
    return A.construct<Function>(-1, *this);
}

Expected<Function>
Scope::
loadChunk(
    std::string_view luaChunk,
    source_location loc)
{
    SourceLocation Loc(loc);
    return loadChunk(
        luaChunk,
        fmt::format("{}({})", 
            Loc.file_name(),
            Loc.line()),
        loc);
}

Expected<Function>
Scope::
loadChunkFromFile(
    std::string_view fileName,
    source_location loc)
{
    auto luaChunk = files::getFileText(fileName);
    if(! luaChunk)
        return luaChunk.error();
    return loadChunk(*luaChunk, fileName, loc);
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
        MRDOX_ASSERT(lua_isnil(A, -1));
        lua_pop(A, 1);
        return formatError("global key '{}' not found", key);
    }
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
        MRDOX_UNREACHABLE();
    case Kind::domObject:
        domObject_push(A, obj_);
        return;
    default:
        MRDOX_UNREACHABLE();
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
        MRDOX_UNREACHABLE();
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
        MRDOX_UNREACHABLE();
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
                MRDOX_UNREACHABLE();
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
                        MRDOX_UNREACHABLE();
    case LUA_TNUMBER:   return Type::number;
    case LUA_TSTRING:   return Type::string;
    case LUA_TTABLE:    return Type::table;
    case LUA_TFUNCTION: return Type::function;
    case LUA_TUSERDATA: MRDOX_UNREACHABLE();
    case LUA_TTHREAD:   MRDOX_UNREACHABLE();
    default:
        MRDOX_UNREACHABLE();
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
        MRDOX_UNREACHABLE();
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
        return luaM_popError(A);
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
        throw Error("not a string");
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
        throw Error("not a function");
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
        throw Error("not a Table");
}

Table::
Table(
    Value value)
    : Value(std::move(value))
{
    Access A(*scope_);
    if(lua_type(A, index_) != LUA_TTABLE)
        throw Error("not a Table");
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
        return formatError("method {} not found", key);
    if(! lua_isfunction(A, -1))
        return formatError("table key '{}' is not a function", key);
    for(std::size_t i = 0; i < size; ++i)
        A.push(data[i], *scope_);
    auto rc = lua_pcall(A, size, 1, 0);
    if(rc != LUA_OK)
        return luaM_popError(A);
    return A.construct<Value>(-1, *scope_);
}

//------------------------------------------------

void
lua_dump(dom::Object const& obj)
{
    Context ctx;

    auto err = tryLoadHandlebars(ctx);
    if(err)
        return luaM_report(err);

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

    auto err = tryLoadHandlebars(ctx);
    if(err)
        return luaM_report(err);

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
    fmt::println("{}", testFunc(obj));
}

} // lua
} // mrdox
} // clang
