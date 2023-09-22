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
