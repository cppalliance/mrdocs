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
#include <mrdox/Support/Dom.hpp>
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
        DomObject
    };

    Kind kind_ = Kind::Undefined;

    union
    {
        bool b_;
        int i_;
        unsigned int u_;
        double d_;
        std::string_view s_;
        int idx_;
        dom::ObjectPtr obj_;
    };

    friend struct Access;
    void push(void*) const;
    Param(Param&&) noexcept;
    Param(dom::ObjectPtr const&) noexcept;

public:
    /** Destructor.
    */
    ~Param();

    /** Constructor.

        Default constructed params have type undefined.
    */
    Param() noexcept;

    Param(std::nullptr_t) noexcept;
    Param(bool) noexcept;
    Param(int) noexcept;
    Param(unsigned int) noexcept;
    Param(double) noexcept;
    Param(std::string_view) noexcept;
    Param(Value const&) noexcept;
    Param(dom::Value const&) noexcept;

    template<class String>
    requires std::is_convertible_v<
        String, std::string_view>
    Param(
        String const& string)
        : Param(std::string_view(string))
    {
    }
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
    MRDOX_DECL Object getGlobalObject();

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

    MRDOX_DECL bool isArray() const noexcept;

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
        Param const* data,
        std::size_t size) const;
};

template<class... Args>
Expected<Value>
tryCall(
    Value const& fn,
    Args&&... args)
{
    std::array<Param, sizeof...(args)> va{ Param(args)... };
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

/** An ECMAScript Array.

    An Array is an Object which has the internal
    class Array prototype.
*/
class Array : public Value
{
    friend struct Access;

    Array(int idx, Scope&) noexcept;

public:
    MRDOX_DECL Array(Value value);
    MRDOX_DECL Array& operator=(Value value);
    MRDOX_DECL explicit Array(Scope& scope);

    MRDOX_DECL
    std::size_t
    size() const;

    MRDOX_DECL
    void
    push_back(
        Param value) const;
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
        Param const* data,
        std::size_t size) const;

public:
    MRDOX_DECL Object(Scope&,
        dom::Pointer<dom::Object> const& obj);
    MRDOX_DECL Object(Value value);
    MRDOX_DECL Object& operator=(Value value);
    MRDOX_DECL explicit Object(Scope& scope);

    MRDOX_DECL
    void
    insert(
        std::string_view name, Param value) const;

    /** Call a member or function.
    */
    template<class... Args>
    Expected<Value>
    tryCall(
        std::string_view name,
        Args&&... args) const
    {
        std::array<Param, sizeof...(args)> va{ Param(args)... };
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
