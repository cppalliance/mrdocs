//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_JAVASCRIPT_HPP
#define MRDOX_API_SUPPORT_JAVASCRIPT_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Dom.hpp>
#include <mrdox/Support/Error.hpp>
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
class MRDOX_DECL
    Context
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

    /** Run a script.
    */
    MRDOX_DECL
    Error
    script(std::string_view jsCode);

    /** Return the global object.
    */
    MRDOX_DECL
    Value
    getGlobalObject();

    /** Return a global object if it exists.
    */
    MRDOX_DECL
    Expected<Value>
    getGlobal(std::string_view name);
};

//------------------------------------------------

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

    bool isUndefined() const noexcept;
    bool isNull() const noexcept;
    bool isBoolean() const noexcept;
    bool isNumber() const noexcept;
    bool isString() const noexcept;
    bool isArray() const noexcept;
    bool isObject() const noexcept;

    std::string getString() const;

void setlog();

    /** Call a function.
    */
    template<class... Args>
    Expected<Value>
    call(Args&&... args) const
    {
        return callImpl({ dom::Value(std::forward<Args>(args))... });
    }

    /** Call a function.
    */
    template<class... Args>
    Value
    operator()(Args&&... args) const
    {
        return call(std::forward<Args>(args)...).value();
    }

    /** Call a method.
    */
    template<class... Args>
    Expected<Value>
    callProp(
        std::string_view prop,
        Args&&... args) const
    {
        return callPropImpl(prop,
            { dom::Value(std::forward<Args>(args))... });
    }

private:
    MRDOX_DECL
    Expected<Value>
    callImpl(
        std::initializer_list<dom::Value> args) const;

    MRDOX_DECL
    Expected<Value>
    callPropImpl(
        std::string_view prop,
        std::initializer_list<dom::Value> args) const;
};

inline bool Value::isUndefined() const noexcept
{
    return type() == Type::undefined;
}

inline bool Value::isNull() const noexcept
{
    return type() == Type::null;
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

inline bool Value::isObject() const noexcept
{
    return type() == Type::object;
}

} // js
} // mrdox
} // clang

#endif
