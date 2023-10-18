//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_KIND_HPP
#define MRDOX_API_DOM_KIND_HPP

#include <mrdox/Platform.hpp>
#include <string_view>

namespace clang {
namespace mrdox {
namespace dom {

/** The type of data in a Value.

    This is the type of data stored in a Value.
    These types are loosely modeled after the
    JavaScript types and data structures.

    Primitive values are Undefined, Null, Boolean,
    Integer, and String.

    Undefined and Null are inhabited by a single
    value each. The difference between Undefined
    and Null is that Undefined is the default
    value for a Value, while Null represents a
    value that is explicitly set. Undefined is
    used to represent things such as:

    @li An uninitialized Value
    @li The Value returned from a function that failed to return a value
    @li The result of accessing a nonexistent object property
    @li The result of a `find` algorithm when no element is found

    This distinction is semantically important as
    algorithms frequently need to distinguish between
    these two cases.

    Booleans, Integers, and Strings are also primitive
    values. This means they are deeply copied when assigned or
    passed as a parameter.

    Other value types, such as Array, Object, and Function
    are reference types, meaning that they are not copied
    when assigned or passed as a parameter. Instead, the
    reference is copied, and the original value is shared.

    These reference types are modeled after JavaScript
    "Objects". All non-primitive types (Object types)
    are derived from Object in Javascript. This means
    types such as Array and Function represent a
    relevant selection of built-in types that would
    derive from Object in JavaScript.

    Objects are a collection of properties, which are
    equivalent to key-value pairs. Property values
    can be any type, including other Objects, allowing
    for the creation of arbitrarily complex data
    structures.

    @li https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures

*/
enum class Kind
{
    Undefined,
    Null,
    Boolean,
    Integer,
    String,
    SafeString,
    Array,
    Object,
    Function
};

static constexpr
std::string_view
toString(Kind const& value)
{
    switch (value)
    {
    case Kind::Undefined:    return "undefined";
    case Kind::Null:         return "null";
    case Kind::Boolean:      return "boolean";
    case Kind::Integer:      return "integer";
    case Kind::String:       return "string";
    case Kind::SafeString:   return "safeString";
    case Kind::Array:        return "array";
    case Kind::Object:       return "object";
    case Kind::Function:     return "function";
    }
    return "unknown";
}

} // dom
} // mrdox
} // clang

#endif
