//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SUPPORT_LUA_HPP
#define MRDOX_SUPPORT_LUA_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Dom.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/Expected.hpp>
#include <mrdox/Support/SharedPtr.hpp>
#include <mrdox/Support/source_location.hpp>
#include <fmt/format.h>
#include <cstdlib>
#include <string>
#include <string_view>

namespace clang {
namespace mrdox {
namespace lua {

struct Access;

class Context;
class Scope;

class Table;
class Value;
class Function;

using FunctionPtr = Value (*)(std::vector<Value>);

//------------------------------------------------

/** A null-terminated string.
*/
class zstring
{
    std::string s_;
    char const* c_str_;

public:
    zstring(char const* s) noexcept
        : c_str_(s)
    {
    }

    zstring(std::string_view s)
        : s_(s)
        , c_str_(s_.c_str())
    {
    }

    zstring(std::string const& s)
        : c_str_(s.c_str())
    {
    }

    char const* c_str() const noexcept
    {
        return c_str_;
    }
};

//------------------------------------------------

/** A reference to an instance of a Lua interpreter.
*/
class MRDOX_DECL
    Context
{
    struct Impl;

    SharedPtr<Impl> impl_;

    friend struct Access;
    friend class Scope;

public:
    /** Copy assignment.
    */
    Context& operator=(Context const&) = delete;

    /** Destructor.
    */
    ~Context();

    /** Constructor.
    */
    Context();

    /** Constructor.
    */
    Context(Context const&) noexcept;
};

//------------------------------------------------

class Scope
{
    Context ctx_;
    std::size_t refs_;
    int top_;

    friend struct Access;

    void reset();

public:
    MRDOX_DECL
    Scope(Context const& ctx) noexcept;

    MRDOX_DECL
    ~Scope();

    /** Load a Lua chunk
    */
    MRDOX_DECL
    Expected<Function>
    loadChunk(
        std::string_view luaChunk,
        zstring chunkName,
        source_location loc =
            source_location::current());

    /** Load a Lua chunk
    */
    MRDOX_DECL
    Expected<Function>
    loadChunk(
        std::string_view luaChunk,
        source_location loc =
            source_location::current());

    /** Run a Lua chunk.
    */
    MRDOX_DECL
    Expected<Function>
    loadChunkFromFile(
        std::string_view fileName,
        source_location loc =
            source_location::current());

    /** Return the global table.
    */
    MRDOX_DECL
    Table
    getGlobalTable();

    /** Return a value from the global table if it exists.
    */
    MRDOX_DECL
    Expected<Value>
    getGlobal(
        std::string_view key,
        source_location loc =
            source_location::current());
};

//------------------------------------------------

/** A lazy container to push values to the Lua stack.
*/
class MRDOX_DECL
    Param
{
    enum class Kind
    {
        nil,
        boolean,
        integer,
        string,
        value,
        domArray,
        domObject
    };

    Kind kind_;

    union
    {
        bool b_;
        int i_;
        int index_; // for Value
        std::string_view s_;
        dom::Array arr_;
        dom::Object obj_;
    };

    friend struct Access;

    void push(Scope&) const;
    Param(Param&&) noexcept;

public:
    ~Param();
    Param(std::nullptr_t) noexcept;
    Param(std::int64_t) noexcept;
    Param(std::string_view s) noexcept;
    Param(Value const& value) noexcept;
    Param(dom::Array arr) noexcept;
    Param(dom::Object obj) noexcept;
    Param(dom::Value const& value) noexcept;

    Param(Param const&) = delete;
    Param& operator=(Param const&) = delete;

    template<class Boolean>
    requires std::is_same_v<Boolean, bool>
    Param(Boolean const& b) noexcept
        : kind_(Kind::boolean)
        , b_(b)
    {
    }

    Param(char const* s) noexcept
        : Param(std::string_view(s))
    {
    }

    template<class String>
    requires std::is_convertible_v<
        String, std::string_view>
    Param(String const& s)
        : Param(std::string_view(s))
    {
    }

    template<class Enum>
    requires std::is_enum_v<Enum>
    Param(Enum v) noexcept
        : kind_(Kind::integer)
        , i_(static_cast<int>(v))
    {
    }
};

//------------------------------------------------

/** Types of values.
*/
enum class Type
{
    nil      = 0,
    boolean  = 1,
    number   = 3,
    string   = 4,
    table    = 5,
    function = 6
};

/** A Lua value.
*/
class MRDOX_DECL
    Value
{
protected:
    Scope* scope_;
    int index_;

    friend struct Access;

    Value(int, Scope&) noexcept;

public:
    /** Destructor.

        The Lua value will eventually be removed
        from the stack.
    */
    ~Value();

    /** Constructor.

        Default constructed values have no
        scope or assigned stack index and
        are equivalent to the value Nil.
    */
    Value() noexcept;

    /** Constructor.

        The newly constructed object will acquire
        the same stack index, while the moved-from
        object will become as if default-constructed.
    */
    Value(Value&&) noexcept;

    /** Constructor.

        The new value will be assigned a new stack
        index which has the same underlying value
        as `other`.

        @param other The value to copy.
    */
    Value(Value const& other);

    Type type() const noexcept;

    bool isNil() const noexcept;
    bool isBoolean() const noexcept;
    bool isNumber() const noexcept;
    bool isString() const noexcept;
    bool isFunction() const noexcept;
    bool isTable() const noexcept;

    /** Return a string representation.

        This function is used for diagnostics.
    */
    std::string
    displayString() const;

    /** Invoke the value as a function.

        If the invocation fails the return value
        will contain the corresponding error.

        @param args... Zero or more values to pass
        to the function.
    */
    template<class... Args>
    Expected<Value>
    call(
        Args&&... args)
    {
        if constexpr(sizeof...(args) > 0)
        {
            Param va[] = { Param(args)... };
            return callImpl(va, sizeof...(args));
        }
        return callImpl(nullptr, 0);
    }

    /** Invoke the value as a function.
    */
    template<class... Args>
    Value operator()(Args&&... args)
    {
        return call(
            std::forward<Args>(args)...).release();
    }

private:
    Expected<Value>
    callImpl(
        Param const* args,
        std::size_t narg);
};

inline bool Value::isNil() const noexcept
{
    return type() == Type::nil;
}

inline bool Value::isBoolean() const noexcept
{
    return type() == Type::boolean;
}

inline bool Value::isNumber() const noexcept
{
    return type() == Type::number;
}

inline bool Value::isString() const noexcept
{
    return type() == Type::string;
}

inline bool Value::isFunction() const noexcept
{
    return type() == Type::function;
}

inline bool Value::isTable() const noexcept
{
    return type() == Type::table;
}

//------------------------------------------------

/** A Lua string.
*/
class String : public Value
{
    friend struct Access;

    String(int index, Scope&) noexcept;

public:
    MRDOX_DECL String(Value value);
    MRDOX_DECL explicit String(std::string_view s);

    MRDOX_DECL std::string_view get() const noexcept;

    std::string_view operator*() const noexcept
    {
        return get();
    }

    operator std::string_view() const noexcept
    {
        return get();
    }
};

//------------------------------------------------

/** A Lua function.
*/
class MRDOX_DECL
    Function : public Value
{
    friend struct Access;

    Function(int index, Scope&) noexcept;

public:
    Function(Value value);
};

//------------------------------------------------

/** A Lua table.
*/
class Table : public Value
{
    friend struct Access;

    Table(int index, Scope&);

    Expected<Value>
    callImpl(
        std::string_view name,
        Param const* data,
        std::size_t size) const;

public:
    MRDOX_DECL Table(Scope&, dom::Object const& obj);
    MRDOX_DECL Table(Value value);
    MRDOX_DECL explicit Table(Scope& scope);

    //MRDOX_DECL Value get(zstring key) const;

    MRDOX_DECL
    Value
    get(
        std::string_view key) const;

    /** Create or replace the value with a key.
    */
    MRDOX_DECL
    void
    set(
        std::string_view key,
        Param value) const;
};

} // lua
} // mrdox
} // clang

//------------------------------------------------

template<>
struct fmt::formatter<clang::mrdox::lua::Value>
    : fmt::formatter<std::string>
{
    auto format(
        clang::mrdox::lua::Value const& value,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string>::format(
            value.displayString(), ctx);
    }
};

#endif
