// Impl fragment of Lua.cpp (one TU): the lua::Value methods.
// Included within `namespace mrdocs::lua {`. Not a standalone header.

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
