// Impl fragment of Lua.cpp (one TU): the lua::Scope methods.
// Included within `namespace mrdocs::lua {`. Not a standalone header.

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
