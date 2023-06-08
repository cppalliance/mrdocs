//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SUPPORT_JAVASCRIPT_HPP
#define MRDOX_SUPPORT_JAVASCRIPT_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/Expected.hpp>
#include <cstdlib>
#include <string_view>

namespace clang {
namespace mrdox {
namespace js {

struct Access;
class Context;
class Scope;

class Array;
class Boolean;
class Object;
class String;
class Value;

//------------------------------------------------

class Prop
{
    unsigned int index_;
    std::string_view name_;

public:
    constexpr Prop(std::string_view name) noexcept
        : index_(0)
        , name_(name)
    {
    }

    constexpr Prop(unsigned int index) noexcept
        : index_(index)
    {
    }

    constexpr bool isIndex() const noexcept
    {
        return name_.empty();
    }
};

//------------------------------------------------

/** A reference to an instance of a JavaScript interpreter.
*/
class Context
{
    struct Impl;

    Impl* impl_;

    friend struct Access;

public:
    /** Copy assignment.
    */
    Context& operator=(Context const&) = delete;

    /** Destructor.
    */
    MRDOX_DECL ~Context();

    /** Constructor.
    */
    MRDOX_DECL Context();

    /** Constructor.
    */
    MRDOX_DECL Context(Context const&) noexcept;
};

//------------------------------------------------

struct Arg
{
    void (*push)(Arg const&, Scope&);

    int number = 0;
    char const* data = nullptr;
    std::size_t size = 0;
    unsigned int index = 0; // of object

    Arg() = default;

    Arg(std::string_view s) noexcept
        : push(&push_lstring)
        , data(s.data())
        , size(s.size())
    {
    }

    MRDOX_DECL Arg(Value const& value) noexcept;

    MRDOX_DECL static void push_Value(Arg const&, Scope&);
    MRDOX_DECL static void push_lstring(Arg const&, Scope&);
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

    /** Run a script.
    */
    MRDOX_DECL Error script(std::string_view jsCode);

    /** Return a global object if it exists.
    */
    MRDOX_DECL
    Expected<Object>
    tryGetGlobal(std::string_view name);

    /** Return a global object if it exists.
    */
    MRDOX_DECL
    Object
    getGlobal(std::string_view name);

    Expected<Value> call();
};

//------------------------------------------------

/** Types of values.
*/
enum class Type
{
    undefined = 1,
    null,
    boolean,
    number,
    string,
    object
};

/** An ECMAScript value.
*/
class Value
{
protected:
    Scope* scope_;
    int idx_;

    friend struct Access;

    Value(int, Scope&) noexcept;

public:
    MRDOX_DECL ~Value();
    MRDOX_DECL Value() noexcept;
    MRDOX_DECL Value(Value const&);
    MRDOX_DECL Value(Value&&) noexcept;
    MRDOX_DECL Value& operator=(Value const&);
    MRDOX_DECL Value& operator=(Value&&) noexcept;

    MRDOX_DECL Type type() const noexcept;

    bool isUndefined() const noexcept
    {
        return type() == Type::undefined;
    }

    bool isNull() const noexcept
    {
        return type() == Type::null;
    }

    bool isBoolean() const noexcept
    {
        return type() == Type::boolean;
    }

    bool isNumber() const noexcept
    {
        return type() == Type::number;
    }

    bool isString() const noexcept
    {
        return type() == Type::string;
    }

    bool isObject() const noexcept
    {
        return type() == Type::object;
    }

    /** Call a function.
    */
    template<class... Args>
    friend
    Expected<Value>
    tryCall(
        Value const& fn,
        Args&&... args);

    /** Call a function.
    */
    template<class... Args>
    friend
    Value
    call(
        Value const& fn,
        Args&&... args);

private:
    MRDOX_DECL
    Expected<Value>
    callImpl(
        Arg const* data,
        std::size_t size) const;
};

template<class... Args>
Expected<Value>
tryCall(
    Value const& fn,
    Args&&... args)
{
    std::array<Arg, sizeof...(args)> va{ Arg(args)... };
    return fn.callImpl(va.data(), va.size());
}

/** Call a function.
*/
template<class... Args>
Value
call(
    Value const& fn,
    Args&&... args)
{
    auto result = tryCall(fn,
        std::forward<Args>(args)...);
    if(result)
        throw result;
}

//------------------------------------------------

/** An ECMAScript Object.
*/
class String : public Value
{
    friend struct Access;

    String(int idx, Scope&) noexcept;

public:
    MRDOX_DECL String(Value value);
    MRDOX_DECL String& operator=(Value value);
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

/** An ECMAScript Object.
*/
class Object : public Value
{
    friend struct Access;

    Object(int idx, Scope&) noexcept;

    Expected<Value> callImpl(
        std::string_view name,
        Arg const* data,
        std::size_t size) const;

public:
    MRDOX_DECL Object(Value value);
    MRDOX_DECL Object& operator=(Value value);
    MRDOX_DECL explicit Object(Scope& scope);

    /** Call a member or function.
    */
    template<class... Args>
    Expected<Value>
    tryCall(
        std::string_view name,
        Args&&... args) const
    {
        std::array<Arg, sizeof...(args)> va{ Arg(args)... };
        return callImpl(name, va.data(), va.size());
    }

    /** Call a member or function.
    */
    template<class... Args>
    Value
    call(
        std::string_view name,
        Args&&... args) const
    {
        auto result = tryCall(name, std::forward<Args>(args)...);
        if(! result)
            throw result;
        return *result;
    }
};

} // js
} // mrdox
} // clang

#endif
