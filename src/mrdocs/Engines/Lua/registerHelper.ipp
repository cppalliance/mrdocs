// Impl fragment of Lua.cpp (one TU): lua::registerHelper, its detail helpers and Lua<->DOM marshalling.
// Included within `namespace mrdocs::lua {`. Not a standalone header.

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
dom::Expected<dom::Value>
invokeHelperRef(
    std::shared_ptr<LuaHelperHandle> const& handle,
    dom::Array const& args)
{
    if (args.empty())
    {
        return Unexpected(dom::Error(
            "handlebars::Handlebars helper called without arguments; "
            "expected options object as last argument"));
    }
    dom::Value const& options = args.back();
    if (! options.isObject())
    {
        return Unexpected(dom::Error(
            "handlebars::Handlebars helper options must be an object; "
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
        return Unexpected(dom::Error(std::string(luaM_popError(A).message())));

    dom::Value result = luaToDom(A, lua_gettop(A));
    lua_pop(A, 1);
    return result;
}

// Plain invocation used by makeCallable: push all positional args (no
// Handlebars options stripping), run the function, and convert the result.
static
dom::Expected<dom::Value>
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

    int const rc = lua_pcall(A, static_cast<int>(narg), 1, 0);
    if (rc != LUA_OK)
        return Unexpected(dom::Error(std::string(luaM_popError(A).message())));

    dom::Value result = luaToDom(A, lua_gettop(A));
    lua_pop(A, 1);
    return result;
}

} // detail

Expected<void, Error>
registerHelper(
    handlebars::Handlebars& hbs,
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
            [handle](dom::Array const& args) -> dom::Expected<dom::Value>
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
        [handle](dom::Array const& args) -> dom::Expected<dom::Value>
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
