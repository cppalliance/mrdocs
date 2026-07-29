// Impl fragment of Lua.cpp (one TU): the lua::Table methods.
// Included within `namespace mrdocs::lua {`. Not a standalone header.

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
