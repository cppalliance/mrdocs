// Impl fragment of Lua.cpp (one TU): the lua::Function methods.
// Included within `namespace mrdocs::lua {`. Not a standalone header.

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
