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

/** A bound value which can be passed to JavaScript.

    Objects of this type are used as parameter
    types in signatures of C++ functions. They
    should not be used anywhere else, otherwise
    the behavior is undefined.
*/
class MRDOX_DECL
    Param
{
    enum class Kind
    {
        Undefined,
        Null,
        Boolean,
        Integer,
        Unsigned,
        Double,
        String,
        Value,
        DomArray,
        DomObject
    };

    Kind kind_ = Kind::Undefined;

    union
    {
        bool b_;
        int i_;
        unsigned int u_;
        double d_;
        int idx_; // for Value
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
    Param(int) noexcept;
    Param(unsigned int) noexcept;
    Param(double) noexcept;
    Param(std::string_view s) noexcept;
    Param(Value const& value) noexcept;
    Param(dom::Array const& arr) noexcept;
    Param(dom::Object const& obj) noexcept;
    Param(dom::Value const& value) noexcept;

    Param(Param const&) = delete;
    Param& operator=(Param const&) = delete;

    template<class Boolean>
    requires std::is_same_v<Boolean, bool>
    Param(Boolean const& b) noexcept
        : kind_(Kind::Boolean)
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
        : kind_(Kind::Integer)
        , i_(static_cast<int>(v))
    {
    }
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
        std::array<Param, sizeof...(args)> va{ Param(args)... };
        return callImpl(va.data(), va.size());
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
        std::array<Param, sizeof...(args)> va{ Param(args)... };
        return callPropImpl(prop, va.data(), va.size());
    }

private:
    MRDOX_DECL
    Expected<Value>
    callImpl(
        Param const* data,
        std::size_t size) const;

    MRDOX_DECL
    Expected<Value>
    callPropImpl(
        std::string_view prop,
        Param const* data,
        std::size_t size) const;
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
