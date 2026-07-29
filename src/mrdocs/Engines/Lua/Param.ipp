// Impl fragment of Lua.cpp (one TU): the lua::Param methods.
// Included within `namespace mrdocs::lua {`. Not a standalone header.

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
