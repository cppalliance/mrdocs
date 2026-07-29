// Impl fragment of Lua.cpp (one TU): the lua::String methods.
// Included within `namespace mrdocs::lua {`. Not a standalone header.

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
