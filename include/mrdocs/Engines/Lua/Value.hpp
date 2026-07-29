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

#ifndef MRDOCS_API_ENGINES_LUA_VALUE_HPP
#define MRDOCS_API_ENGINES_LUA_VALUE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Engines/Lua/Param.hpp>
#include <mrdocs/Engines/Lua/Type.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/polyfill/source_location.hpp>
#include <format>
#include <string>
#include <string_view>

namespace mrdocs {
namespace lua {

/** A Lua value.
*/
class MRDOCS_DECL
    Value
{
protected:
    /** Scope that owns the stack slot for this value.
    */
    Scope* scope_;
    /** Stack index where the value is stored.
    */
    int index_;

    friend struct Access;

    /** Create a value referring to a stack slot within a scope.
    */
    Value(int position, Scope& scope) noexcept;

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

    /** Return the Lua type of this value.
    */
    Type type() const noexcept;

    /** Return true if the value is nil.
    */
    bool isNil() const noexcept;
    /** Return true if the value is a boolean.
    */
    bool isBoolean() const noexcept;
    /** Return true if the value is numeric.
    */
    bool isNumber() const noexcept;
    /** Return true if the value is a string.
    */
    bool isString() const noexcept;
    /** Return true if the value is a function.
    */
    bool isFunction() const noexcept;
    /** Return true if the value is a table.
    */
    bool isTable() const noexcept;

    /** Return a string representation.

        This function is used for diagnostics.
    */
    std::string
    displayString() const;

    /** Invoke the value as a function.

        If the invocation fails the return value
        will contain the corresponding error.

        @param args Zero or more values to pass
        to the function.
        @return The return value of the function.
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

        @param args Zero or more values to pass
        to the function.
    */
    template<class... Args>
    Value operator()(Args&&... args)
    {
        return std::move(call(
            std::forward<Args>(args)...)).value();
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

} // lua
} // mrdocs

//------------------------------------------------

template <>
struct std::formatter<mrdocs::lua::Value> : std::formatter<std::string> {
  template <class FmtContext>
  auto format(mrdocs::lua::Value const &value, FmtContext &ctx) const {
    return std::formatter<std::string>::format(value.displayString(), ctx);
  }
};

#endif // MRDOCS_API_ENGINES_LUA_VALUE_HPP
